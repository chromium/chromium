// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/shutdown_watchdog.h"

#include <stdlib.h>  // For _exit() on Windows.

#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif  // BUILDFLAG(IS_POSIX)

namespace remoting {

ShutdownWatchdog::ShutdownWatchdog(const base::TimeDelta& duration)
    : watchdog_(duration, "Shutdown watchdog", true, this) {}

void ShutdownWatchdog::Arm() {
  watchdog_.Arm();
}

void ShutdownWatchdog::SetExitCode(int exit_code) {
  base::AutoLock lock(lock_);
  exit_code_ = exit_code;
}

void ShutdownWatchdog::Alarm() {
  // Holding a lock while calling _exit() might not be a safe thing to do, so
  // make a local copy.
  int exit_code;
  {
    base::AutoLock lock(lock_);
    exit_code = exit_code_;
  }

  LOG(ERROR) << "Shutdown watchdog triggered, exiting with code " << exit_code;
  _exit(exit_code);
}

}  // namespace remoting
