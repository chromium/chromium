// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/breakpad_linux.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "remoting/base/crash/breakpad_utils.h"
#include "third_party/breakpad/breakpad/src/client/linux/handler/exception_handler.h"

namespace remoting {

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

}  // namespace remoting
