// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Loader and module-resolution thunks for apifw.dll. Load and resolution calls
// are logged so audits capture dynamic and delay-loaded APIs. FreeLibrary
// protects redirected module handles.

#include "sandbox/win/tests/basic_process/nocrt.h"
#include "sandbox/win/tests/basic_process/shim/shim_runtime.h"

namespace {

template <typename Char>
bool EqualsAsciiIgnoringCase(Char lhs, char rhs) {
  if (lhs >= static_cast<Char>('A') && lhs <= static_cast<Char>('Z')) {
    lhs += static_cast<Char>('a' - 'A');
  }
  if (rhs >= 'A' && rhs <= 'Z') {
    rhs += 'a' - 'A';
  }
  return lhs == static_cast<Char>(rhs);
}

// SAFETY: both arguments are caller-provided NUL-terminated strings.
#pragma clang unsafe_buffer_usage begin
template <typename Char>
bool EqualsModuleName(const Char* module_name, const char* expected_name) {
  while (*module_name && *expected_name) {
    if (!EqualsAsciiIgnoringCase(*module_name, *expected_name)) {
      return false;
    }
    ++module_name;
    ++expected_name;
  }
  if (*expected_name) {
    return false;
  }
  if (!*module_name) {
    return true;
  }

  const char* suffix = ".dll";
  while (*module_name && *suffix) {
    if (!EqualsAsciiIgnoringCase(*module_name, *suffix)) {
      return false;
    }
    ++module_name;
    ++suffix;
  }
  return !*module_name && !*suffix;
}
#pragma clang unsafe_buffer_usage end

// Identifies system DLL names redirected to apifw.dll, including full paths
// and names with an omitted ".dll" suffix.
template <typename Char>
bool IsRedirectedModuleName(const Char* module_name) {
  if (!module_name) {
    return false;
  }
  const Char* basename = nullptr;
  if constexpr (sizeof(Char) == sizeof(char)) {
    basename = path_basename_a(module_name);
  } else {
    basename = path_basename_w(module_name);
  }
  return EqualsModuleName(basename, "advapi32") ||
         EqualsModuleName(basename, "bcryptprimitives") ||
         EqualsModuleName(basename, "combase") ||
         EqualsModuleName(basename, "crypt32") ||
         EqualsModuleName(basename, "dwrite") ||
         EqualsModuleName(basename, "kernel32") ||
         EqualsModuleName(basename, "kernelbase") ||
         EqualsModuleName(basename, "ntdll") ||
         EqualsModuleName(basename, "oleaut32") ||
         EqualsModuleName(basename, "version") ||
         EqualsModuleName(basename, "winmm") ||
         EqualsModuleName(basename, "ws2_32");
}

template <typename Char>
HMODULE MaybeReturnApifwForModule(const char* function,
                                  const Char* module_name) {
  if (!IsRedirectedModuleName(module_name)) {
    return nullptr;
  }
  basic_process::LogFn(function, "returning apifw.dll");
  return g_module;
}

}  // namespace

extern "C" {

ORIGINAL_FN_DECL(LoadLibraryExW);
// Loads the specified module into the address space of the calling process.
// Logs every call so we can see the full load path (e.g., the renderer's
// bcryptprimitives.dll load that satisfies ProcessPrng) before forwarding.
BASIC_STUB_EXPORT HMODULE WINAPI ApifwLoadLibraryExW(LPCWSTR lpLibFileName,
                                                     HANDLE hFile,
                                                     DWORD dwFlags) {
  // Log once per unique DLL name — same rationale as GetModuleHandleW.
  LOG1_ONCE_W(lpLibFileName);
  // Record the call site plus the DLL being loaded so the report can attribute
  // each load to its caller and target module.
  CAPTURE_CALLSITE_W(lpLibFileName);
  if (HMODULE redirected =
          MaybeReturnApifwForModule(__FUNCTION__, lpLibFileName)) {
    return redirected;
  }
  ORIGINAL_FN_RESOLVE_NOLOG(LoadLibraryExW, L"kernel32.dll");
  return ORIGINAL_FN(LoadLibraryExW)(lpLibFileName, hFile, dwFlags);
}

ORIGINAL_FN_DECL(LoadLibraryW);
// Loads the specified module into the address space of the calling process
// (wide-character version). Apifw* wrapper — logs unique DLL names then
// forwards to kernel32.
BASIC_STUB_EXPORT HMODULE WINAPI ApifwLoadLibraryW(LPCWSTR lpLibFileName) {
  // Log once per unique DLL name — same rationale as GetModuleHandleW.
  LOG1_ONCE_W(lpLibFileName);
  CAPTURE_CALLSITE_W(lpLibFileName);
  if (HMODULE redirected =
          MaybeReturnApifwForModule(__FUNCTION__, lpLibFileName)) {
    return redirected;
  }
  ORIGINAL_FN_RESOLVE_NOLOG(LoadLibraryW, L"kernel32.dll");
  return ORIGINAL_FN(LoadLibraryW)(lpLibFileName);
}

ORIGINAL_FN_DECL(LoadLibraryA);
BASIC_STUB_EXPORT HMODULE WINAPI ApifwLoadLibraryA(LPCSTR lpLibFileName) {
  LOG1_ONCE_A(lpLibFileName);
  CAPTURE_CALLSITE_A(lpLibFileName);
  if (HMODULE redirected =
          MaybeReturnApifwForModule(__FUNCTION__, lpLibFileName)) {
    return redirected;
  }
  ORIGINAL_FN_RESOLVE_NOLOG(LoadLibraryA, L"kernel32.dll");
  return ORIGINAL_FN(LoadLibraryA)(lpLibFileName);
}

ORIGINAL_FN_DECL(LoadLibraryExA);
BASIC_STUB_EXPORT HMODULE WINAPI ApifwLoadLibraryExA(LPCSTR lpLibFileName,
                                                     HANDLE hFile,
                                                     DWORD dwFlags) {
  LOG1_ONCE_A(lpLibFileName);
  CAPTURE_CALLSITE_A(lpLibFileName);
  if (HMODULE redirected =
          MaybeReturnApifwForModule(__FUNCTION__, lpLibFileName)) {
    return redirected;
  }
  ORIGINAL_FN_RESOLVE_NOLOG(LoadLibraryExA, L"kernel32.dll");
  return ORIGINAL_FN(LoadLibraryExA)(lpLibFileName, hFile, dwFlags);
}

ORIGINAL_FN_DECL(FreeLibrary);
// Redirected loads return g_module without incrementing its reference count.
// Ignore matching frees to avoid unloading the shim.
BASIC_STUB_EXPORT BOOL WINAPI ApifwFreeLibrary(HMODULE hModule) {
  if (hModule == g_module) {
    return TRUE;
  }
  ORIGINAL_FN_RESOLVE_NOLOG(FreeLibrary, L"kernel32.dll");
  return FreeLibrary_func(hModule);
}

// Retrieves a module handle for the specified module
ORIGINAL_FN_DECL(GetModuleHandleA);
BASIC_STUB_EXPORT HMODULE WINAPI ApifwGetModuleHandleA(LPCSTR lpModuleName) {
  // Log once per unique module name — same rationale as GetModuleHandleW.
  LOG1_ONCE_A(lpModuleName);
  CAPTURE_CALLSITE_A(lpModuleName);
  if (HMODULE redirected =
          MaybeReturnApifwForModule(__FUNCTION__, lpModuleName)) {
    return redirected;
  }
  ORIGINAL_FN_RESOLVE_NOLOG(GetModuleHandleA, L"kernel32.dll");
  return ORIGINAL_FN(GetModuleHandleA)(lpModuleName);
}

ORIGINAL_FN_DECL(GetModuleHandleW);
BASIC_STUB_EXPORT HMODULE WINAPI ApifwGetModuleHandleW(LPCWSTR lpModuleName) {
  LOG1_ONCE_W(lpModuleName);
  CAPTURE_CALLSITE_W(lpModuleName);
  if (HMODULE redirected =
          MaybeReturnApifwForModule(__FUNCTION__, lpModuleName)) {
    return redirected;
  }
  ORIGINAL_FN_RESOLVE_NOLOG(GetModuleHandleW, L"kernel32.dll");
  return ORIGINAL_FN(GetModuleHandleW)(lpModuleName);
}

ORIGINAL_FN_DECL(GetModuleHandleExW);
BASIC_STUB_EXPORT BOOL WINAPI ApifwGetModuleHandleExW(DWORD dwFlags,
                                                      LPCWSTR lpModuleName,
                                                      HMODULE* phModule) {
  const bool name_is_address =
      (dwFlags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS) != 0;
  if (name_is_address) {
    basic_process::LogHandle(__FUNCTION__, lpModuleName);
    CAPTURE_CALLSITE_A("#address");
  } else {
    LOG1_ONCE_W(lpModuleName);
    CAPTURE_CALLSITE_W(lpModuleName);
  }
  if (!name_is_address && lpModuleName && phModule) {
    if (HMODULE redirected =
            MaybeReturnApifwForModule(__FUNCTION__, lpModuleName)) {
      *phModule = redirected;
      return TRUE;
    }
  }
  ORIGINAL_FN_RESOLVE_NOLOG(GetModuleHandleExW, L"kernel32.dll");
  return ORIGINAL_FN(GetModuleHandleExW)(dwFlags, lpModuleName, phModule);
}

ORIGINAL_FN_DECL(SetDllDirectoryW);
BASIC_STUB_EXPORT BOOL WINAPI ApifwSetDllDirectoryW(LPCWSTR lpPathName) {
  ORIGINAL_FN_RESOLVE_WITH_ARG(SetDllDirectoryW, L"kernel32.dll", lpPathName);
  return ORIGINAL_FN(SetDllDirectoryW)(lpPathName);
}

BASIC_STUB_EXPORT
FARPROC WINAPI ApifwGetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
  const bool is_ordinal = (reinterpret_cast<uintptr_t>(lpProcName) >> 16) == 0;
  if (is_ordinal) {
    basic_process::LogFn(__FUNCTION__, "#ordinal");
    CAPTURE_CALLSITE_A("#ordinal");
  } else {
    basic_process::LogFn(__FUNCTION__, lpProcName);
    CAPTURE_CALLSITE_A(lpProcName);
  }
  return ::GetProcAddress(hModule, lpProcName);
}

}  // extern "C"
