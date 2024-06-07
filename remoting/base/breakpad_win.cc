// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad.h"

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
#include "base/strings/string_number_conversions_win.h"
#include "base/time/time.h"
#include "base/win/wrapped_window_proc.h"
#include "remoting/base/breakpad_utils.h"
#include "remoting/base/version.h"
#include "third_party/breakpad/breakpad/src/client/windows/common/ipc_protocol.h"
#include "third_party/breakpad/breakpad/src/client/windows/handler/exception_handler.h"

namespace remoting {

namespace {

// Minidump with stacks, PEB, TEBs and unloaded module list.
const MINIDUMP_TYPE kMinidumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData | MiniDumpWithUnloadedModules);

// A data class used to hold values which are referenced in the CustomClientInfo
// structure for this process. These must not be destroyed as we want them to be
// available in the case of a crash during destruction.
struct CustomClientInfoData {
  std::wstring process_id;
  std::wstring program_name;
  std::wstring process_start_time;
};

// Returns the CustomClientInfo to be used for crash reporting.
google_breakpad::CustomClientInfo* GetCustomClientInfo() {
  static base::NoDestructor<CustomClientInfoData> data{
      base::NumberToWString(base::GetCurrentProcId()),
      base::CommandLine::ForCurrentProcess()->GetProgram().BaseName().value(),
      base::NumberToWString(base::Time::NowFromSystemTime().ToTimeT()),
  };
  static google_breakpad::CustomInfoEntry entries[] = {
      {kCustomClientInfoVersionKey, REMOTING_VERSION_WSTRING},
      {kCustomClientInfoProcessIdKey, data->process_id.data()},
      {kCustomClientInfoProcessNameKey, data->program_name.data()},
      {kCustomClientInfoProcessStartTimeKey, data->process_start_time.data()},
  };
  static google_breakpad::CustomClientInfo custom_info = {entries,
                                                          std::size(entries)};
  return &custom_info;
}

class BreakpadWin {
 public:
  BreakpadWin();

  BreakpadWin(const BreakpadWin&) = delete;
  BreakpadWin& operator=(const BreakpadWin&) = delete;

  ~BreakpadWin() = delete;

  static BreakpadWin& GetInstance();

  BreakpadHelper& helper() { return helper_; }

  void Initialize(std::optional<std::string> server_pipe_handle);

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

void BreakpadWin::Initialize(
    std::optional<std::string> server_pipe_handle = std::nullopt) {
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
      pipe_handle, GetCustomClientInfo());

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

}  // namespace

void InitializeCrashReporting() {
  // Touch the object to make sure it is initialized.
  BreakpadWin::GetInstance().Initialize();
}

void InitializeOopCrashClient(const std::string& server_pipe_handle) {
  // Touch the object to make sure it is initialized.
  BreakpadWin::GetInstance().Initialize(server_pipe_handle);
}

}  // namespace remoting
