// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/breakpad.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "remoting/base/breakpad_utils.h"
#include "third_party/breakpad/breakpad/src/client/linux/handler/exception_handler.h"

namespace remoting {

namespace {

class BreakpadLinux {
 public:
  BreakpadLinux();

  BreakpadLinux(const BreakpadLinux&) = delete;
  BreakpadLinux& operator=(const BreakpadLinux&) = delete;

  ~BreakpadLinux() = delete;

  static BreakpadLinux& GetInstance();

  BreakpadHelper& helper() { return helper_; }

 private:
  // Breakpad exception handler.
  std::unique_ptr<google_breakpad::ExceptionHandler> breakpad_;

  // Shared logic for handling exceptions and minidump processing.
  BreakpadHelper helper_;
};

bool FilterCallback(void* context) {
  // If an exception is already being handled, this thread will be put to sleep.
  BreakpadLinux::GetInstance().helper().OnException();
  return true;
}

bool MinidumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                      void* context,
                      bool succeeded) {
  BreakpadLinux& self = BreakpadLinux::GetInstance();
  return self.helper().OnMinidumpGenerated(base::FilePath(descriptor.path()));
}

BreakpadLinux::BreakpadLinux() {
  auto minidump_directory = GetMinidumpDirectoryPath();
  if (helper().Initialize(minidump_directory)) {
    google_breakpad::MinidumpDescriptor descriptor(minidump_directory.value());
    breakpad_ = std::make_unique<google_breakpad::ExceptionHandler>(
        descriptor, FilterCallback, MinidumpCallback,
        /*callback_context=*/nullptr,
        /*install_handler=*/true,
        /*server_fd=*/-1);
  }
}

// static
BreakpadLinux& BreakpadLinux::GetInstance() {
  static base::NoDestructor<BreakpadLinux> instance;
  return *instance;
}

}  // namespace

void InitializeCrashReporting() {
  // Touch the object to make sure it is initialized.
  BreakpadLinux::GetInstance();
}

}  // namespace remoting
