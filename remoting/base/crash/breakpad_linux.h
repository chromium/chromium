// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_BREAKPAD_LINUX_H_
#define REMOTING_BASE_CRASH_BREAKPAD_LINUX_H_

#include "base/no_destructor.h"
#include "remoting/base/crash/breakpad_utils.h"
#include "third_party/breakpad/breakpad/src/client/linux/handler/exception_handler.h"

namespace remoting {

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

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_BREAKPAD_LINUX_H_
