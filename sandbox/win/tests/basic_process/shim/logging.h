// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_LOGGING_H_
#define SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_LOGGING_H_

#include <windows.h>

#include <intrin.h>

// Logging helpers for apifw thunks. Use these to record which redirected APIs
// are reached and, when requested, where they were called from.

// Log sink for the measured test child.
extern HANDLE g_log_file_handle;

// Enables caller-stack logging for logged thunks.
extern bool g_log_call_sites;

// Allows ORIGINAL_FN thunks to forward during explicit audit runs.
extern bool g_audit_basic_sandbox;

namespace basic_process {

// Use for ordinary logged thunks.
void LogFn(const char* function, const char* arg = nullptr);
void LogFn(const char* function, const wchar_t* arg);

// Use when the argument value is the signal and repeated calls are noise.
void LogFnOnceW(const char* function, const wchar_t* arg);
void LogFnOnceA(const char* function, const char* arg);

// Use when the interesting signal is a named integer argument.
void LogFnUInt(const char* function,
               const char* arg_name,
               unsigned int arg_value);

// Use for handle-based thunks.
void LogHandle(const char* function, const void* handle);

// Records DLLs loaded on demand by the shim.
void LogOnDemandLoad(const wchar_t* module_name);

// Records a normal thunk entry followed by its raw call stack.
void LogStackFn(const char* function);

// Records a thunk's caller stack unless the same function, detail, and stack
// were already logged. Optional detail identifies the symbol or DLL being
// resolved by loader/resolver thunks.
void MaybeLogCallSite(const char* function,
                      void* return_address,
                      const char* detail_narrow = nullptr,
                      const wchar_t* detail_wide = nullptr);

}  // namespace basic_process

#define LOG()                           \
  do {                                  \
    basic_process::LogFn(__FUNCTION__); \
    CAPTURE_CALLSITE();                 \
  } while (0)
#define LOG1(a)                            \
  do {                                     \
    basic_process::LogFn(__FUNCTION__, a); \
    CAPTURE_CALLSITE();                    \
  } while (0)
// Use the _A/_W forms from loader/resolver thunks to attach the name being
// loaded or resolved.
#define CAPTURE_CALLSITE()                                             \
  do {                                                                 \
    if (g_log_call_sites) {                                            \
      basic_process::MaybeLogCallSite(__FUNCTION__, _ReturnAddress()); \
    }                                                                  \
  } while (0)
#define CAPTURE_CALLSITE_A(detail)                                    \
  do {                                                                \
    if (g_log_call_sites) {                                           \
      basic_process::MaybeLogCallSite(__FUNCTION__, _ReturnAddress(), \
                                      (detail), nullptr);             \
    }                                                                 \
  } while (0)
#define CAPTURE_CALLSITE_W(detail)                                             \
  do {                                                                         \
    if (g_log_call_sites) {                                                    \
      basic_process::MaybeLogCallSite(__FUNCTION__, _ReturnAddress(), nullptr, \
                                      (detail));                               \
    }                                                                          \
  } while (0)
// Log only the first time this process sees each unique (function, arg) pair.
#define LOG1_ONCE_W(a) basic_process::LogFnOnceW(__FUNCTION__, a)
#define LOG1_ONCE_A(a) basic_process::LogFnOnceA(__FUNCTION__, a)
// For APIs that should never be called, but should not crash the audit run.
#define LOG_FAIL_LOUD() basic_process::LogStackFn(__FUNCTION__)

#endif  // SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_LOGGING_H_
