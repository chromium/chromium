// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Process-wide state for the apifw.dll logging shim.

#include <windows.h>

// clang-format off: Must precede C++ standard library headers.
#include "sandbox/win/tests/basic_process/shim/libcxx_support.h"
// clang-format on

#include <cstdint>
#include <limits>
#include <string_view>

#include "sandbox/win/tests/basic_process/shim/logging.h"
#include "sandbox/win/tests/basic_process/shim/nocrt.h"
#include "sandbox/win/tests/basic_process/shim/shim_runtime.h"
#include "sandbox/win/tests/basic_process/shim/string_util.h"

namespace {

constexpr wchar_t kLogFileSwitch[] = L"--basic-process-api-test-log-file=";
constexpr wchar_t kLogDirSwitch[] = L"--basic-process-api-test-log-dir=";
constexpr wchar_t kLogCallSitesSwitch[] =
    L"--basic-process-api-test-log-call-sites";
constexpr wchar_t kAuditSwitch[] = L"--basic-process-api-test-audit";
constexpr wchar_t kLogOnceSwitch[] = L"--basic-process-api-test-log-once";
constexpr size_t kMaxDwordDecimalDigits =
    std::numeric_limits<DWORD>::digits10 + 1;

bool CopyLogPath(std::wstring_view command_line,
                 size_t switch_position,
                 std::wstring_view switch_name,
                 basic_process::WideStringBuffer<MAX_PATH>& path) {
  const bool quoted =
      switch_position > 0 && command_line[switch_position - 1] == L'"';
  const std::wstring_view value = basic_process::ExtractQuotedValue(
      command_line.substr(switch_position + switch_name.size()), quoted);
  if (value.empty() || !path.TryAppend(value)) {
    ::SetLastError(ERROR_INVALID_PARAMETER);
    return false;
  }
  return true;
}

HANDLE CreateLogInDirectory(basic_process::WideStringBuffer<MAX_PATH>& path) {
  // Reserve the maximum decimal widths of the PID and collision suffix.
  constexpr size_t kFilenameCapacity =
      sizeof(L"\\apifw__.log") / sizeof(wchar_t) + 2 * kMaxDwordDecimalDigits;
  if (path.size() + kFilenameCapacity > MAX_PATH) {
    ::SetLastError(ERROR_FILENAME_EXCED_RANGE);
    return INVALID_HANDLE_VALUE;
  }
  if (!path.view().ends_with(L'\\') && !path.view().ends_with(L'/') &&
      !path.TryAppend(L'\\')) {
    ::SetLastError(ERROR_FILENAME_EXCED_RANGE);
    return INVALID_HANDLE_VALUE;
  }
  const size_t directory_length = path.size();

  for (DWORD suffix = 0;; ++suffix) {
    path.Truncate(directory_length);
    if (!path.TryAppend(L"apifw_") ||
        !path.TryAppendDecimal(::GetCurrentProcessId()) ||
        !path.TryAppend(L'_') || !path.TryAppendDecimal(suffix) ||
        !path.TryAppend(L".log")) {
      ::SetLastError(ERROR_FILENAME_EXCED_RANGE);
      return INVALID_HANDLE_VALUE;
    }

    // Avoid appending to old logs when PIDs are reused.
    HANDLE handle = ::CreateFileW(path.c_str(), FILE_APPEND_DATA,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle != INVALID_HANDLE_VALUE ||
        ::GetLastError() != ERROR_FILE_EXISTS || suffix == MAXDWORD) {
      return handle;
    }
  }
}

HANDLE OpenLogFileFromCommandLine(std::wstring_view command_line) {
  const size_t file_switch = command_line.find(kLogFileSwitch);
  const size_t dir_switch = command_line.find(kLogDirSwitch);
  const bool has_file = file_switch != std::wstring_view::npos;
  const bool has_dir = dir_switch != std::wstring_view::npos;
  if (!has_file && !has_dir) {
    return INVALID_HANDLE_VALUE;
  }

  HANDLE handle = INVALID_HANDLE_VALUE;
  basic_process::WideStringBuffer<MAX_PATH> path;
  if (has_file && has_dir) {
    ::SetLastError(ERROR_INVALID_PARAMETER);
  } else if (has_file &&
             CopyLogPath(command_line, file_switch, kLogFileSwitch, path)) {
    handle = ::CreateFileW(path.c_str(), FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  } else if (has_dir &&
             CopyLogPath(command_line, dir_switch, kLogDirSwitch, path)) {
    handle = CreateLogInDirectory(path);
  }
  if (handle == INVALID_HANDLE_VALUE) {
    basic_process::LogFnUInt("ApifwOpenLogFile", "error", ::GetLastError());
  }
  return handle;
}

bool CommandLineHasSwitch(std::wstring_view command_line,
                          std::wstring_view switch_name) {
  return command_line.find(switch_name) != std::wstring_view::npos;
}

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
      const std::wstring_view command_line = ::GetCommandLineW();
      const bool directory_audit =
          CommandLineHasSwitch(command_line, kLogDirSwitch);
      g_log_file_handle = OpenLogFileFromCommandLine(command_line);
      if (directory_audit && g_log_file_handle == INVALID_HANDLE_VALUE) {
        return FALSE;
      }
      if (directory_audit ||
          CommandLineHasSwitch(command_line, kLogCallSitesSwitch)) {
        g_log_call_sites = true;
      }
      if (directory_audit || CommandLineHasSwitch(command_line, kAuditSwitch)) {
        g_audit_basic_sandbox = true;
      }
      if (directory_audit ||
          CommandLineHasSwitch(command_line, kLogOnceSwitch)) {
        g_log_once_mode = true;
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
