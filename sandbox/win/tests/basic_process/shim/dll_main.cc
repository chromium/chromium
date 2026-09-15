// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Process-wide state for the apifw.dll logging shim.

#include <windows.h>

#include <cstdint>

#include "sandbox/win/tests/basic_process/nocrt.h"
#include "sandbox/win/tests/basic_process/shim/logging.h"
#include "sandbox/win/tests/basic_process/shim/shim_runtime.h"

namespace {

constexpr wchar_t kLogFileSwitch[] = L"--basic-process-api-test-log-file=";
constexpr wchar_t kLogCallSitesSwitch[] =
    L"--basic-process-api-test-log-call-sites";
constexpr wchar_t kAuditSwitch[] = L"--basic-process-api-test-audit";

// SAFETY: This parses a switch value from the NUL-terminated process command
// line into a fixed-size local buffer. Every write is bounded by MAX_PATH.
#pragma clang unsafe_buffer_usage begin
HANDLE OpenLogFileFromCommandLine() {
  wchar_t* command_line = ::GetCommandLineW();
  wchar_t* switch_position = wcsstr(command_line, kLogFileSwitch);
  if (!switch_position) {
    return INVALID_HANDLE_VALUE;
  }

  wchar_t* value = switch_position + wcslen(kLogFileSwitch);
  bool quoted = switch_position > command_line && switch_position[-1] == L'"';
  if (*value == L'"') {
    quoted = true;
    ++value;
  }

  wchar_t path[MAX_PATH];
  size_t path_length = 0;
  while (*value && path_length < MAX_PATH - 1) {
    if ((quoted && *value == L'"') ||
        (!quoted && (*value == L' ' || *value == L'\t'))) {
      break;
    }
    path[path_length++] = *value++;
  }
  if (path_length == 0 ||
      (path_length == MAX_PATH - 1 && *value && *value != L'"')) {
    return INVALID_HANDLE_VALUE;
  }
  path[path_length] = L'\0';
  return ::CreateFileW(path, FILE_APPEND_DATA,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool CommandLineHasSwitch(const wchar_t* command_line,
                          const wchar_t* switch_name) {
  return wcsstr(command_line, switch_name) != nullptr;
}
#pragma clang unsafe_buffer_usage end

}  // namespace

extern "C" {

HMODULE g_module = nullptr;

// Loads redirected DLLs on demand without recursing through the shim exports.
HMODULE WINAPI GetOrLoadModule(LPCWSTR module_name) {
  HMODULE h = ::GetModuleHandleW(module_name);
  if (!h) {
    h = ::LoadLibraryW(module_name);
    if (h) {
      basic_process::LogOnDemandLoad(module_name);
    }
  }
  return h;
}

extern int __stdcall DllEntryPoint(HANDLE hDllHandle,
                                   DWORD dwReason,
                                   LPVOID lpReserved) {
  switch (dwReason) {
    case DLL_PROCESS_ATTACH: {
      g_module = reinterpret_cast<HMODULE>(hDllHandle);
      LPWSTR command_line = GetCommandLineW();
      g_log_file_handle = OpenLogFileFromCommandLine();
      if (CommandLineHasSwitch(command_line, kLogCallSitesSwitch)) {
        g_log_call_sites = true;
      }
      if (CommandLineHasSwitch(command_line, kAuditSwitch)) {
        g_audit_basic_sandbox = true;
      }
    } break;

    case DLL_THREAD_ATTACH:
      break;

    case DLL_THREAD_DETACH:
      break;

    case DLL_PROCESS_DETACH:
      if (g_log_file_handle != INVALID_HANDLE_VALUE) {
        ::CloseHandle(g_log_file_handle);
        g_log_file_handle = INVALID_HANDLE_VALUE;
      }
      g_module = nullptr;
      break;
  }
  return TRUE;
}

}  // extern "C"
