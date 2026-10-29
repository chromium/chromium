// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/hang_watchdog.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"

namespace remoting {

class HangWatchdog::Checker {
 public:
  Checker(const std::atomic<base::LiveTicks>* last_heartbeat,
          base::TimeDelta hang_timeout,
          base::TimeDelta check_interval,
          base::RepeatingClosure on_hang)
      : last_heartbeat_(last_heartbeat),
        hang_timeout_(hang_timeout),
        on_hang_(std::move(on_hang)) {
    timer_.Start(FROM_HERE, check_interval,
                 base::BindRepeating(&Checker::Check, base::Unretained(this)));
  }

  Checker(const Checker&) = delete;
  Checker& operator=(const Checker&) = delete;

 private:
  void Check() {
    const base::TimeDelta since_heartbeat =
        base::LiveTicks::Now() -
        last_heartbeat_->load(std::memory_order_relaxed);
    if (since_heartbeat < hang_timeout_) {
      hang_reported_ = false;
      return;
    }
    // Report each hang only once.
    if (!hang_reported_) {
      hang_reported_ = true;
      on_hang_.Run();
    }
  }

  // Owned by HangWatchdog, which destroys `this` before it.
  const raw_ptr<const std::atomic<base::LiveTicks>> last_heartbeat_;
  const base::TimeDelta hang_timeout_;
  const base::RepeatingClosure on_hang_;
  bool hang_reported_ = false;
  base::RepeatingTimer timer_;
};

HangWatchdog::HangWatchdog(base::TimeDelta hang_timeout,
                           base::TimeDelta heartbeat_interval,
                           base::RepeatingClosure on_hang)
    : watchdog_thread_("HangWatchdog") {
  CHECK(heartbeat_interval.is_positive());
  CHECK_LT(heartbeat_interval, hang_timeout);
  RecordHeartbeat();
  heartbeat_timer_.Start(FROM_HERE, heartbeat_interval,
                         base::BindRepeating(&HangWatchdog::RecordHeartbeat,
                                             base::Unretained(this)));
  CHECK(watchdog_thread_.Start());
  checker_.emplace(watchdog_thread_.task_runner(), &last_heartbeat_,
                   hang_timeout, heartbeat_interval, std::move(on_hang));
}

HangWatchdog::~HangWatchdog() {
  heartbeat_timer_.Stop();
  // Destroy the checker on the watchdog thread before stopping the thread,
  // which runs the pending deletion. `last_heartbeat_` outlives it.
  checker_.Reset();
  watchdog_thread_.Stop();
}

void HangWatchdog::RecordHeartbeat() {
  last_heartbeat_.store(base::LiveTicks::Now(), std::memory_order_relaxed);
}

}  // namespace remoting
