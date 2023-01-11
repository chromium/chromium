// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains the necessary code to register the Breakpad exception
// handler. This implementation is based on Chrome crash reporting code. See:
//   - src/components/crash/core/app/breakpad_win.cc
//   - src/chrome/installer/setup/setup_main.cc

#include "remoting/base/breakpad.h"

#include <crtdbg.h>
#include <windows.h>

#include <memory>
#include <string>

#include "base/atomicops.h"
#include "base/check.h"
#include "base/file_version_info.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "base/win/wrapped_window_proc.h"
#include "third_party/breakpad/breakpad/src/client/windows/handler/exception_handler.h"

namespace remoting {
void InitializeCrashReportingForTest(const wchar_t* pipe_name);
}  // namespace remoting

namespace {

const wchar_t kBreakpadProductName[] = L"Chromoting";
const wchar_t kBreakpadVersionEntry[] = L"ver";
const wchar_t kBreakpadVersionDefault[] = L"0.1.0.0";
const wchar_t kBreakpadProdEntry[] = L"prod";
const wchar_t kBreakpadPlatformEntry[] = L"plat";
const wchar_t kBreakpadPlatformWin32[] = L"Win32";

// The protocol for connecting to the out-of-process Breakpad crash
// reporter is different for x86-32 and x86-64: the message sizes
// are different because the message struct contains a pointer.  As
// a result, there are two different named pipes to connect to.  The
// 64-bit one is distinguished with an "-x64" suffix.
#if defined(_WIN64)
const wchar_t kGoogleUpdatePipeName[] =
    L"\\\\.\\pipe\\GoogleCrashServices\\S-1-5-18-x64";
#else
const wchar_t kGoogleUpdatePipeName[] =
    L"\\\\.\\pipe\\GoogleCrashServices\\S-1-5-18";
#endif

using base::subtle::AtomicWord;
using base::subtle::NoBarrier_CompareAndSwap;

class BreakpadWin {
 public:
  BreakpadWin();

  BreakpadWin(const BreakpadWin&) = delete;
  BreakpadWin& operator=(const BreakpadWin&) = delete;

  ~BreakpadWin();

  static BreakpadWin* GetInstance();

 private:
  // Returns the Custom information to be used for crash reporting.
  google_breakpad::CustomClientInfo* GetCustomInfo();

  // This callback is executed when the process has crashed and *before*
  // the crash dump is created. To prevent duplicate crash reports we
  // make every thread calling this method, except the very first one,
  // go to sleep.
  static bool OnExceptionCallback(void* context,
                                  EXCEPTION_POINTERS* exinfo,
                                  MDRawAssertionInfo* assertion);

  // Crashes the process after generating a dump for the provided exception.
  // Note that the crash reporter should be initialized before calling this
  // function for it to do anything.
  static int OnWindowProcedureException(EXCEPTION_POINTERS* exinfo);

  // Breakpad's exception handler.
  std::unique_ptr<google_breakpad::ExceptionHandler> breakpad_;

  // This flag is used to indicate that an exception is already being handled.
  volatile AtomicWord handling_exception_;

  // The testing hook below allows overriding the crash server pipe name.
  static const wchar_t* pipe_name_;

  friend void ::remoting::InitializeCrashReportingForTest(const wchar_t*);
};

// |LazyInstance| is used to guarantee that the exception handler will be
// initialized exactly once.
// N.B. LazyInstance does not allow this to be a static member of the class.
static base::LazyInstance<BreakpadWin>::Leaky g_instance =
    LAZY_INSTANCE_INITIALIZER;

const wchar_t* BreakpadWin::pipe_name_ = kGoogleUpdatePipeName;

BreakpadWin::BreakpadWin() : handling_exception_(0) {
  // Disable the message box for assertions.
  _CrtSetReportMode(_CRT_ASSERT, 0);

  // Get the alternate dump directory. We use the temp path.
  // N.B. We don't use base::GetTempDir() here to avoid running more code then
  //      necessary before crashes can be properly reported.
  wchar_t temp_directory[MAX_PATH + 1] = {0};
  DWORD length = GetTempPath(MAX_PATH, temp_directory);
  if (length == 0) {
    return;
  }

  // Minidump with stacks, PEB, TEBs and unloaded module list.
  MINIDUMP_TYPE dump_type = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithProcessThreadData | MiniDumpWithUnloadedModules);
  breakpad_.reset(new google_breakpad::ExceptionHandler(
      temp_directory, &OnExceptionCallback, NULL, NULL,
      google_breakpad::ExceptionHandler::HANDLER_ALL, dump_type, pipe_name_,
      GetCustomInfo()));

  if (breakpad_->IsOutOfProcess()) {
    // Tells breakpad to handle breakpoint and single step exceptions.
    breakpad_->set_handle_debug_exceptions(true);
  }

  // Catch exceptions thrown from a window procedure.
  base::win::WinProcExceptionFilter exception_filter =
      base::win::SetWinProcExceptionFilter(&OnWindowProcedureException);
  CHECK(!exception_filter);
}

BreakpadWin::~BreakpadWin() {
  // This object should be leaked so that crashes which occur during process
  // shutdown will be caught.
  NOTREACHED();
}

// static
BreakpadWin* BreakpadWin::GetInstance() {
  return &g_instance.Get();
}

// Returns the Custom information to be used for crash reporting.
google_breakpad::CustomClientInfo* BreakpadWin::GetCustomInfo() {
  std::unique_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfoForModule(CURRENT_MODULE()));

  static wchar_t version[64];
  if (version_info.get()) {
    wcscpy_s(version, base::as_wcstr(version_info->product_version()));
  } else {
    wcscpy_s(version, kBreakpadVersionDefault);
  }

  static google_breakpad::CustomInfoEntry ver_entry(kBreakpadVersionEntry,
                                                    version);
  static google_breakpad::CustomInfoEntry prod_entry(kBreakpadProdEntry,
                                                     kBreakpadProductName);
  static google_breakpad::CustomInfoEntry plat_entry(kBreakpadPlatformEntry,
                                                     kBreakpadPlatformWin32);
  static google_breakpad::CustomInfoEntry entries[] = {ver_entry, prod_entry,
                                                       plat_entry};
  static google_breakpad::CustomClientInfo custom_info = {entries,
                                                          std::size(entries)};
  return &custom_info;
}

// static
bool BreakpadWin::OnExceptionCallback(void* /* context */,
                                      EXCEPTION_POINTERS* /* exinfo */,
                                      MDRawAssertionInfo* /* assertion */) {
  BreakpadWin* self = BreakpadWin::GetInstance();
  if (NoBarrier_CompareAndSwap(&self->handling_exception_, 0, 1) != 0) {
    // Capture every thread except the first one in the sleep. We don't
    // want multiple threads to concurrently report exceptions.
    ::Sleep(INFINITE);
  }
  return true;
}

// static
int BreakpadWin::OnWindowProcedureException(EXCEPTION_POINTERS* exinfo) {
  BreakpadWin* self = BreakpadWin::GetInstance();
  if (self->breakpad_.get() != NULL) {
    self->breakpad_->WriteMinidumpForException(exinfo);
    TerminateProcess(GetCurrentProcess(),
                     exinfo->ExceptionRecord->ExceptionCode);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

namespace remoting {

void InitializeCrashReporting() {
  // Touch the object to make sure it is initialized.
  BreakpadWin::GetInstance();
}

void InitializeCrashReportingForTest(const wchar_t* pipe_name) {
  BreakpadWin::pipe_name_ = pipe_name;
  InitializeCrashReporting();
}

}  // namespace remoting
