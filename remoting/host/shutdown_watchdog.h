// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SHUTDOWN_WATCHDOG_H_
#define REMOTING_HOST_SHUTDOWN_WATCHDOG_H_

#include "base/synchronization/lock.h"
#include "base/threading/watchdog.h"

namespace remoting {

// This implements a watchdog timer that ensures the host process eventually
// terminates, even if some threads are blocked or being kept alive for
// some reason. This is not expected to trigger if host shutdown is working
// correctly (on a normally loaded system). The triggering of the alarm
// indicates a sign of trouble, and so the Alarm() method will log some
// diagnostic information before shutting down the process.
class ShutdownWatchdog : public base::Watchdog::Delegate {
 public:
  // Creates a watchdog that waits for |duration| (after the watchdog is
  // armed) before shutting down the process.
  explicit ShutdownWatchdog(const base::TimeDelta& duration);

  ShutdownWatchdog(const ShutdownWatchdog&) = delete;
  ShutdownWatchdog& operator=(const ShutdownWatchdog&) = delete;

  void Arm();

  // This method should be called to set the process's exit-code before arming
  // the watchdog. Otherwise an exit-code of 0 is assumed.
  void SetExitCode(int exit_code);

  void Alarm() override;

 private:
  int exit_code_;

  // Protects |exit_code_|, since Alarm() gets called on a separate thread.
  base::Lock lock_;
  base::Watchdog watchdog_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SHUTDOWN_WATCHDOG_H_
