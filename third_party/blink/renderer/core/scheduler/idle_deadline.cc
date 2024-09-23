// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/idle_deadline.h"

#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

IdleDeadline::IdleDeadline(base::TimeTicks deadline,
                           bool cross_origin_isolated_capability,
                           CallbackType callback_type)
    : deadline_(deadline),
      cross_origin_isolated_capability_(cross_origin_isolated_capability),
      callback_type_(callback_type),
      clock_(base::DefaultTickClock::GetInstance()) {}

double IdleDeadline::timeRemaining() const {
  base::TimeDelta time_remaining = deadline_ - clock_->NowTicks();
  if (time_remaining.is_negative() ||
      ThreadScheduler::Current()->ShouldYieldForHighPriorityWork()) {
    return 0;
  }

  return Performance::ClampTimeResolution(time_remaining,
                                          cross_origin_isolated_capability_);
}

}  // namespace blink
