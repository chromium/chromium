// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/tests/basic_process/nocrt.h"

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

// SAFETY: these are freestanding string/number primitives that walk
// caller-supplied NUL-terminated buffers with raw pointer arithmetic. //base
// is unavailable to this no-CRT code, so base::span and the other safe-buffer
// helpers cannot be used; the check is scoped to this block.
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

char tolower_ascii(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

wchar_t tolower_ascii(wchar_t c) {
  return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c + (L'a' - L'A')) : c;
}

int strcmpi_ascii(const char* str1, const char* str2) {
  while (*str1 && *str2) {
    char c1 = tolower_ascii(*str1);
    char c2 = tolower_ascii(*str2);
    if (c1 != c2) {
      return static_cast<unsigned char>(c1) - static_cast<unsigned char>(c2);
    }
    ++str1;
    ++str2;
  }
  return static_cast<unsigned char>(*str1) - static_cast<unsigned char>(*str2);
}

const char* path_basename_a(const char* path) {
  const char* basename = path;
  for (const char* p = path; p && *p; ++p) {
    if (*p == '\\' || *p == '/') {
      basename = p + 1;
    }
  }
  return basename;
}

const wchar_t* path_basename_w(const wchar_t* path) {
  const wchar_t* basename = path;
  for (const wchar_t* p = path; p && *p; ++p) {
    if (*p == L'\\' || *p == L'/') {
      basename = p + 1;
    }
  }
  return basename;
}

int wcsicmp(const wchar_t* str1, const wchar_t* str2) {
  while (*str1 && *str2) {
    wchar_t c1 = *str1;
    wchar_t c2 = *str2;

    // Convert ASCII uppercase to lowercase
    if (c1 >= L'A' && c1 <= L'Z') {
      c1 += 0x20;
    }
    if (c2 >= L'A' && c2 <= L'Z') {
      c2 += 0x20;
    }

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
