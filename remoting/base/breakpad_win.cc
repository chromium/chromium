// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad.h"

#include <windows.h>

#include <crtdbg.h>

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/win/wrapped_window_proc.h"
#include "remoting/base/breakpad_utils.h"
#include "third_party/breakpad/breakpad/src/client/windows/handler/exception_handler.h"

namespace remoting {

namespace {

class BreakpadWin {
 public:
  BreakpadWin();

  BreakpadWin(const BreakpadWin&) = delete;
  BreakpadWin& operator=(const BreakpadWin&) = delete;

  ~BreakpadWin() = delete;

  static BreakpadWin& GetInstance();

  BreakpadHelper& helper() { return helper_; }

 private:
  // Crashes the process after generating a dump for the provided exception.
  // Note that the crash reporter should be initialized before calling this
  // function for it to do anything.
  static int OnWindowProcedureException(EXCEPTION_POINTERS* exinfo);

  // Breakpad's exception handler.
  std::unique_ptr<google_breakpad::ExceptionHandler> breakpad_;

  // Shared logic for prepping a minidump for upload.
  BreakpadHelper helper_;
};

bool MinidumpCallback(const wchar_t* dump_path,
                      const wchar_t* minidump_id,
                      void* context,
                      EXCEPTION_POINTERS* exinfo,
                      MDRawAssertionInfo* assertion,
                      bool succeeded) {
  BreakpadWin& self = BreakpadWin::GetInstance();
  return self.helper().OnMinidumpGenerated(
      base::FilePath(dump_path).Append(minidump_id).AddExtension(L"dmp"));
}

BreakpadWin::BreakpadWin() {
  // Disable the message box for assertions.
  _CrtSetReportMode(_CRT_ASSERT, 0);

  // TODO: joedow - Enable out-of-process crash reporting.
  auto minidump_directory = GetMinidumpDirectoryPath();
  if (!helper().Initialize(minidump_directory)) {
    return;
  }

  breakpad_ = std::make_unique<google_breakpad::ExceptionHandler>(
      minidump_directory.value(), /*filter=*/nullptr, MinidumpCallback,
      /*callback_context=*/nullptr,
      google_breakpad::ExceptionHandler::HANDLER_ALL);

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

}  // namespace

void InitializeCrashReporting() {
  // Touch the object to make sure it is initialized.
  BreakpadWin::GetInstance();
}

}  // namespace remoting
