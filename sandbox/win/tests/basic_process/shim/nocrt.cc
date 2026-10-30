// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/tests/basic_process/shim/nocrt.h"

#include "sandbox/win/tests/basic_process/shim/string_util.h"

// Enough CRT APIs to get the basic process stub API DLL off the ground, as the
// built-in CRT is not compatible with the available API set.
//
// This also provides the memset/memcpy intrinsics the compiler may emit for
// stack-buffer zeroing and POD struct assignment even under /GS- with no CRT.

extern "C" {

// SAFETY: memset/memcpy operate on caller-supplied buffers whose length is
// passed explicitly as `count`; //base's safe-buffer helpers are unavailable
// in this no-CRT code, so the check is scoped to these two intrinsics.
#pragma clang unsafe_buffer_usage begin

// Minimal CRT intrinsic: the compiler may emit memset calls for stack
// buffer initialization or struct zeroing even with /GS- and no CRT.
#pragma function(memset)
void* memset(void* dest, int c, size_t count) {
  unsigned char* p = static_cast<unsigned char*>(dest);
  for (size_t i = 0; i < count; ++i) {
    p[i] = static_cast<unsigned char>(c);
  }
  return dest;
}

// Minimal CRT intrinsic: the compiler emits memcpy calls for struct assignment.
#pragma function(memcpy)
void* memcpy(void* dest, const void* src, size_t count) {
  unsigned char* d = static_cast<unsigned char*>(dest);
  const unsigned char* s = static_cast<const unsigned char*>(src);
  for (size_t i = 0; i < count; ++i) {
    d[i] = s[i];
  }
  return dest;
}

#pragma clang unsafe_buffer_usage end

}  // extern "C"

// SAFETY: these CRT entry points walk caller-provided NUL-terminated strings.
// Length/search primitives must not delegate to string_view operations, which
// can call these entry points themselves.
#pragma clang unsafe_buffer_usage begin

const wchar_t* wcsstr(const wchar_t* haystack, const wchar_t* needle) {
  if (!*needle) {
    return haystack;  // Empty needle matches the start of haystack
  }

  while (*haystack) {
    const wchar_t* h = haystack;
    const wchar_t* n = needle;

    while (*h && *n && *h == *n) {
      ++h;
      ++n;
    }

    if (!*n) {
      return haystack;  // Found the needle
    }

    ++haystack;
  }

  return nullptr;  // Needle not found
}

extern "C" const wchar_t* wcspbrk(const wchar_t* str, const wchar_t* chars) {
  for (; *str; ++str) {
    for (const wchar_t* c = chars; *c; ++c) {
      if (*str == *c) {
        return str;
      }
    }
  }
  return nullptr;
}

size_t wcslen(const wchar_t* str) {
  const wchar_t* s = str;
  while (*s) {
    ++s;
  }
  return s - str;
}
size_t strlen(const char* str) {
  const char* s = str;
  while (*s) {
    ++s;
  }
  return s - str;
}

int strcmp(const char* str1, const char* str2) {
  while (*str1 && *str1 == *str2) {
    ++str1;
    ++str2;
  }
  return static_cast<unsigned char>(*str1) - static_cast<unsigned char>(*str2);
}

int strcmpi_ascii(const char* str1, const char* str2) {
  while (*str1 && *str2) {
    char c1 = basic_process::ToLowerAscii(*str1);
    char c2 = basic_process::ToLowerAscii(*str2);
    if (c1 != c2) {
      return static_cast<unsigned char>(c1) - static_cast<unsigned char>(c2);
    }
    ++str1;
    ++str2;
  }
  return static_cast<unsigned char>(*str1) - static_cast<unsigned char>(*str2);
}

int wcsicmp(const wchar_t* str1, const wchar_t* str2) {
  while (*str1 && *str2) {
    wchar_t c1 = basic_process::ToLowerAscii(*str1);
    wchar_t c2 = basic_process::ToLowerAscii(*str2);

    if (c1 != c2) {
      return (int)c1 - (int)c2;
    }

    ++str1;
    ++str2;
  }
  // If both ended, they're equal. Otherwise, the longer one is greater.
  return (int)(*str1) - (int)(*str2);
}

#pragma clang unsafe_buffer_usage end
