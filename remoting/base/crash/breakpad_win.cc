// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/breakpad_win.h"

#include <windows.h>

#include <crtdbg.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/win/wrapped_window_proc.h"
#include "remoting/base/crash/breakpad_utils.h"
#include "remoting/base/version.h"
#include "third_party/breakpad/breakpad/src/client/windows/handler/exception_handler.h"

namespace remoting {

bool FilterCallback(void* context,
                    EXCEPTION_POINTERS* exinfo,
                    MDRawAssertionInfo* assertion) {
  // If an exception is already being handled, this thread will be put to sleep.
  BreakpadWin::GetInstance().helper().OnException();
  return true;
}

bool MinidumpCallback(const wchar_t* dump_path,
                      const wchar_t* minidump_id,
                      void* context,
                      EXCEPTION_POINTERS* exinfo,
                      MDRawAssertionInfo* assertion,
                      bool succeeded) {
  return BreakpadWin::GetInstance().helper().OnMinidumpGenerated(
      base::FilePath(dump_path).Append(minidump_id).AddExtension(L"dmp"));
}

BreakpadWin::BreakpadWin() = default;

void BreakpadWin::Initialize(std::optional<std::string> server_pipe_handle) {
  // Initialize should only be called once.
  CHECK(!breakpad_);

  // Disable the message box for assertions.
  _CrtSetReportMode(_CRT_ASSERT, 0);

  HANDLE pipe_handle = nullptr;
  auto minidump_directory = GetMinidumpDirectoryPath();
  bool register_oop_handler =
      server_pipe_handle.has_value() && !server_pipe_handle->empty();
  if (register_oop_handler) {
    uint64_t pipe_handle_value = 0;
    if (base::StringToUint64(*server_pipe_handle, &pipe_handle_value)) {
      // We don't support mixed 32- and 64-bit binaries so HANDLE (really void*)
      // can be cast safely since the crash server will be the same bitness as
      // the client.
      pipe_handle = reinterpret_cast<HANDLE>(pipe_handle_value);
    } else {
      LOG(ERROR) << "server_pipe_handle conversion to number failed: "
                 << *server_pipe_handle;
      return;
    }
  } else if (!helper().Initialize(minidump_directory)) {
    LOG(ERROR) << "Failed to initialize minidump directory for in-proc "
               << "exception handling: " << minidump_directory;
    return;
  }

  breakpad_ = std::make_unique<google_breakpad::ExceptionHandler>(
      minidump_directory.value(), FilterCallback, MinidumpCallback,
      /*callback_context=*/nullptr,
      google_breakpad::ExceptionHandler::HANDLER_ALL, kMinidumpType,
      pipe_handle, /*custom_client_info=*/nullptr);

  bool using_oop_handler = breakpad_->IsOutOfProcess();
  LOG_IF(ERROR, register_oop_handler != using_oop_handler)
      << "Expected crash handling to be done "
      << (register_oop_handler ? "out-of-proc" : "in-proc") << " but is "
      << (using_oop_handler ? "out-of-proc" : "in-proc");

  // Tells breakpad to handle breakpoint and single step exceptions.
  breakpad_->set_handle_debug_exceptions(true);

  // Catch exceptions thrown from a window procedure.
  base::win::WinProcExceptionFilter exception_filter =
      base::win::SetWinProcExceptionFilter(&OnWindowProcedureException);
  CHECK(!exception_filter);
}

// static
BreakpadWin& BreakpadWin::GetInstance() {
  static base::NoDestructor<BreakpadWin> instance;
  return *instance;
}

// static
int BreakpadWin::OnWindowProcedureException(EXCEPTION_POINTERS* exinfo) {
  BreakpadWin& self = BreakpadWin::GetInstance();
  if (self.breakpad_.get() != nullptr) {
    self.breakpad_->WriteMinidumpForException(exinfo);
    TerminateProcess(GetCurrentProcess(),
                     exinfo->ExceptionRecord->ExceptionCode);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace remoting
