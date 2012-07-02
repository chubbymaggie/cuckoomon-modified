/*
Cuckoo Sandbox - Automated Malware Analysis
Copyright (C) 2010-2012 Cuckoo Sandbox Developers

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <windows.h>
#include "ntapi.h"

//
// Log API
//

static void log_bytes(const void *bytes, int len)
{
    const unsigned char *b = (const unsigned char *) bytes;
    while (len--) {
        if(*b >= ' ' && *b < 0x7f) {
            fwrite(b, 1, 1, stderr);
        }
        else {
            fprintf(stderr, "\\x%02x", *b);
        }
        b++;
    }
}

static void log_string(const char *str, int len)
{
    log_bytes("\"", 1);
    while (len--) {
        if(*str == '"' || *str == '\\') {
            log_bytes("\\", 1);
        }
        log_bytes(str++, 1);
    }
    log_bytes("\"", 1);
}

// utf8 encodes an utf16 wchar_t
static void log_wchar(unsigned short c)
{
    if(c < 0x80) {
        unsigned char b[] = {c & 0x7f};
        log_bytes(b, 1);
    }
    else if(c < 0x800) {
        unsigned char b[] = {
            0xc0 + ((c >> 8) << 2) + (c >> 6),
            0x80 + (c & 0x3f),
        };
        log_bytes(b, 2);
    }
    else {
        unsigned char b[] = {
            0xe0 + (c >> 12),
            0x80 + (((c >> 8) & 0x1f) << 2) + ((c >> 6) & 0x3),
            0x80 + (c & 0x3f),
        };
        log_bytes(b, 3);
    }
}

static void log_wstring(const wchar_t *str, int len)
{
    log_bytes("\"", 1);
    while (len--) {
        if(*str == '"' || *str == '\\') {
            log_bytes("\\", 1);
        }
        log_wchar(*str++);
    }
    log_bytes("\"", 1);
}

void loq(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int count = 1, first = 1; char key = 0;

    log_bytes("{", 1);

    while (--count || *fmt != 0) {
        // first time
        if(first != 0) first = 0;
        // comma-seperator
        else log_bytes(", ", 2);

        // we have to find the next format specifier
        if(count == 0) {
            // end of format
            if(*fmt == 0) break;

            // set the count, possibly with a repeated format specifier
            count = *fmt >= '2' && *fmt <= '9' ? *fmt++ - '0' : 1;

            // the next format specifier
            key = *fmt++;
        }

        // log the key
        const char *key_str = va_arg(args, const char *);
        log_string(key_str, strlen(key_str));
        log_bytes(": ", 2);

        // log the value
        if(key == 's') {
            const char *s = va_arg(args, const char *);
            if(s == NULL) s = "";
            log_string(s, strlen(s));
        }
        else if(key == 'S' || key == 'b') {
            int len = va_arg(args, int);
            const char *s = va_arg(args, const char *);
            log_string(s, len);
        }
        else if(key == 'u') {
            const wchar_t *s = va_arg(args, const wchar_t *);
            if(s == NULL) s = L"";
            log_wstring(s, lstrlenW(s));
        }
        else if(key == 'U') {
            int len = va_arg(args, int);
            const wchar_t *s = va_arg(args, const wchar_t *);
            log_wstring(s, len);
        }
        else if(key == 'B') {
            int *len = va_arg(args, int *);
            const char *s = va_arg(args, const char *);
            log_string(s, len != NULL ? *len : 0);
        }
        else if(key == 'i') {
            char buf[16];
            int value = va_arg(args, int);
            sprintf(buf, "%d", value);
            log_bytes(buf, strlen(buf));
        }
        else if(key == 'l' || key == 'p') {
            char buf[20];
            long value = va_arg(args, long);
            sprintf(buf, "%ld", value);
            log_bytes(buf, strlen(buf));
        }
        else if(key == 'L' || key == 'P') {
            char buf[20];
            void **ptr = va_arg(args, void **);
            sprintf(buf, "%ld", ptr != NULL ? *ptr : NULL);
            log_bytes(buf, strlen(buf));
        }
        else if(key == 'o') {
            UNICODE_STRING *str = va_arg(args, UNICODE_STRING *);
            if(str == NULL) {
                log_string("", 0);
            }
            else {
                log_wstring(str->Buffer, str->Length >> 1);
            }
        }
        else if(key == 'O') {
            OBJECT_ATTRIBUTES *obj = va_arg(args, OBJECT_ATTRIBUTES *);
            if(obj == NULL || obj->ObjectName == NULL) {
                log_string("", 0);
            }
            else {
                log_wstring(obj->ObjectName->Buffer,
                    obj->ObjectName->Length >> 1);
            }
        }
        else if(key == 'a') {
            int argc = va_arg(args, int);
            const char **argv = va_arg(args, const char **);
            log_bytes("[", 1);
            while (argc--) {
                log_string(*argv, strlen(*argv));
                argv++;
                if(argc != 0) {
                    log_bytes(", ", 2);
                }
            }
            log_bytes("]", 1);
        }
        else if(key == 'A') {
            int argc = va_arg(args, int);
            const wchar_t **argv = va_arg(args, const wchar_t **);
            log_bytes("[", 1);
            while (argc--) {
                log_wstring(*argv, lstrlenW(*argv));
                argv++;
                if(argc != 0) {
                    log_bytes(", ", 2);
                }
            }
            log_bytes("]", 1);
        }
    }

    log_bytes("}", 1);
    va_end(args);
}