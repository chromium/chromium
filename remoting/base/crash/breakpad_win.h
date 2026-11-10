// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_BREAKPAD_WIN_H_
#define REMOTING_BASE_CRASH_BREAKPAD_WIN_H_

#include <memory>
#include <optional>
#include <string>

#include "remoting/base/crash/breakpad_utils.h"
#include "third_party/breakpad/breakpad/src/client/windows/handler/exception_handler.h"

namespace remoting {

// Minidump with stacks, PEB, TEBs, and unloaded module list. Auxiliary state
// providers are disabled so dbghelp does not load external provider DLLs in the
// crash server process.
// TODO(crbug.com/581367663): Migrate Windows host crash reporting to Crashpad.
const MINIDUMP_TYPE kMinidumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData | MiniDumpWithUnloadedModules |
    MiniDumpWithoutAuxiliaryState);

class BreakpadWin {
 public:
  BreakpadWin();

  BreakpadWin(const BreakpadWin&) = delete;
  BreakpadWin& operator=(const BreakpadWin&) = delete;

  ~BreakpadWin() = delete;

  static BreakpadWin& GetInstance();

  BreakpadHelper& helper() { return helper_; }

  void Initialize(std::optional<std::string> server_pipe_handle = std::nullopt);

 private:
  // Crashes the process after generating a dump for the provided exception.
  // Note that the crash reporter should be initialized before calling this
  // function for it to do anything.
  static int OnWindowProcedureException(EXCEPTION_POINTERS* exinfo);

  // Breakpad exception handler.
  std::unique_ptr<google_breakpad::ExceptionHandler> breakpad_;

  // Shared logic for handling exceptions and minidump processing.
  BreakpadHelper helper_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_BREAKPAD_WIN_H_
