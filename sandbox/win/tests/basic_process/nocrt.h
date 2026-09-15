// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_BASIC_PROCESS_NOCRT_H_
#define SANDBOX_WIN_TESTS_BASIC_PROCESS_NOCRT_H_

#include <cstddef>

// Enough CRT APIs to get the basic process stub API DLL off the ground, as the
// built-in CRT is not compatible with the available API set. Definitions live
// in nocrt.cc.
//
// These are declared with C language linkage to match the prototypes the
// Windows SDK / ucrt headers provide for the same names (windows.h pulls in
// <string.h>), so the definitions in nocrt.cc satisfy those references
// directly without a linkage mismatch.
extern "C" {
const wchar_t* wcsstr(const wchar_t* haystack, const wchar_t* needle);
size_t wcslen(const wchar_t* str);
size_t strlen(const char* str);
int strcmp(const char* str1, const char* str2);
int strcmpi_ascii(const char* str1, const char* str2);
const char* path_basename_a(const char* path);
const wchar_t* path_basename_w(const wchar_t* path);
int wcsicmp(const wchar_t* str1, const wchar_t* str2);
}  // extern "C"

#endif  // SANDBOX_WIN_TESTS_BASIC_PROCESS_NOCRT_H_
