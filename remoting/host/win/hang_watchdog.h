// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_HANG_WATCHDOG_H_
#define REMOTING_HOST_WIN_HANG_WATCHDOG_H_

#include <atomic>

#include "base/functional/callback.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace remoting {

// Runs `on_hang` on a separate thread, once per hang, if the thread that
// created the HangWatchdog stops running tasks, e.g. because it is blocked in a
// synchronous call. The watched thread records a heartbeat, and the watchdog
// thread checks it, every `heartbeat_interval`, so a hang is detected after
// `hang_timeout` give or take `heartbeat_interval`. Must be created and
// destroyed on the same thread, which must have a current default task runner.
//
// base::LiveTicks is used to measure time since it doesn't advance while the
// system is suspended, so that resuming doesn't look like a hang.
class HangWatchdog {
 public:
  HangWatchdog(base::TimeDelta hang_timeout,
               base::TimeDelta heartbeat_interval,
               base::RepeatingClosure on_hang);

  HangWatchdog(const HangWatchdog&) = delete;
  HangWatchdog& operator=(const HangWatchdog&) = delete;

  ~HangWatchdog();

 private:
  // Lives on the watchdog thread and periodically checks the last heartbeat.
  class Checker;

  // Called on the watched thread.
  void RecordHeartbeat();

  std::atomic<base::LiveTicks> last_heartbeat_;
  base::RepeatingTimer heartbeat_timer_;
  base::Thread watchdog_thread_;
  base::SequenceBound<Checker> checker_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_HANG_WATCHDOG_H_
