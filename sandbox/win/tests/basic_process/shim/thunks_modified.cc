// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Thunks whose behavior differs from a plain OS forward.

#include "sandbox/win/tests/basic_process/shim/shim_runtime.h"

extern "C" {

ORIGINAL_FN_DECL(WideCharToMultiByte);
// Allow UTF-8 conversions. Treat CP_ACP as UTF-8; reject other code pages.
BASIC_STUB_EXPORT int WINAPI
ApifwWideCharToMultiByte(UINT CodePage,
                         DWORD dwFlags,
                         LPCWCH lpWideCharStr,
                         int cchWideChar,
                         LPSTR lpMultiByteStr,
                         int cbMultiByte,
                         LPCCH lpDefaultChar,
                         LPBOOL lpUsedDefaultChar) {
  if (CodePage == CP_ACP) {
    CodePage = CP_UTF8;
  }
  const bool unexpected_code_page = CodePage != CP_UTF8;
  if (unexpected_code_page) {
    basic_process::LogFnUInt(__FUNCTION__, "CodePage", CodePage);
    CAPTURE_CALLSITE();
  }

  ORIGINAL_FN_RESOLVE_NOLOG(WideCharToMultiByte, L"kernel32.dll");
  if (unexpected_code_page) {
    ApifwAuditForwardGate();
  }
  return WideCharToMultiByte_func(CodePage, dwFlags, lpWideCharStr, cchWideChar,
                                  lpMultiByteStr, cbMultiByte, lpDefaultChar,
                                  lpUsedDefaultChar);
}

ORIGINAL_FN_DECL(MultiByteToWideChar);
// Same code-page policy as WideCharToMultiByte.
BASIC_STUB_EXPORT int WINAPI ApifwMultiByteToWideChar(UINT CodePage,
                                                      DWORD dwFlags,
                                                      LPCCH lpMultiByteStr,
                                                      int cbMultiByte,
                                                      LPWSTR lpWideCharStr,
                                                      int cchWideChar) {
  if (CodePage == CP_ACP) {
    CodePage = CP_UTF8;
  }
  const bool unexpected_code_page = CodePage != CP_UTF8;
  if (unexpected_code_page) {
    basic_process::LogFnUInt(__FUNCTION__, "CodePage", CodePage);
    CAPTURE_CALLSITE();
  }

  ORIGINAL_FN_RESOLVE_NOLOG(MultiByteToWideChar, L"kernel32.dll");
  if (unexpected_code_page) {
    ApifwAuditForwardGate();
  }
  return MultiByteToWideChar_func(CodePage, dwFlags, lpMultiByteStr,
                                  cbMultiByte, lpWideCharStr, cchWideChar);
}

ORIGINAL_FN_DECL(CreateFileW);
BASIC_STUB_EXPORT HANDLE WINAPI
ApifwCreateFileW(LPCWSTR lpFileName,
                 DWORD dwDesiredAccess,
                 DWORD dwShareMode,
                 LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                 DWORD dwCreationDisposition,
                 DWORD dwFlagsAndAttributes,
                 HANDLE hTemplateFile) {
  // Log first so helpers cannot overwrite CreateFileW's last-error value, such
  // as ERROR_ALREADY_EXISTS for an existing OPEN_ALWAYS file.
  basic_process::LogFn("CreateFileW", lpFileName);
  CAPTURE_CALLSITE_W(lpFileName);
  ORIGINAL_FN_RESOLVE_NOLOG(CreateFileW, L"kernel32.dll");
  return ORIGINAL_FN(CreateFileW)(lpFileName, dwDesiredAccess, dwShareMode,
                                  lpSecurityAttributes, dwCreationDisposition,
                                  dwFlagsAndAttributes, hTemplateFile);
}

ORIGINAL_FN_DECL(CreateSemaphoreW);
BASIC_STUB_EXPORT HANDLE WINAPI
ApifwCreateSemaphoreW(LPSECURITY_ATTRIBUTES lpSemaphoreAttributes,
                      LONG lInitialCount,
                      LONG lMaximumCount,
                      LPCWSTR lpName) {
  ORIGINAL_FN_RESOLVE_WITH_ARG(CreateSemaphoreW, L"kernel32.dll", lpName);
  return ORIGINAL_FN(CreateSemaphoreW)(lpSemaphoreAttributes, lInitialCount,
                                       lMaximumCount, lpName);
}

ORIGINAL_FN_DECL(GetFileAttributesW);
BASIC_STUB_EXPORT DWORD WINAPI ApifwGetFileAttributesW(LPCWSTR lpFileName) {
  ORIGINAL_FN_RESOLVE_WITH_ARG(GetFileAttributesW, L"kernel32.dll", lpFileName);
  return ORIGINAL_FN(GetFileAttributesW)(lpFileName);
}

}  // extern "C"
