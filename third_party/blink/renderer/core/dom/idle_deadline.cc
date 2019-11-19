// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/idle_deadline.h"

#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

IdleDeadline::IdleDeadline(base::TimeTicks deadline, CallbackType callback_type)
    : deadline_(deadline),
      callback_type_(callback_type),
      clock_(base::DefaultTickClock::GetInstance()) {}

double IdleDeadline::timeRemaining() const {
  base::TimeDelta time_remaining = deadline_ - clock_->NowTicks();
  if (time_remaining < base::TimeDelta() ||
      ThreadScheduler::Current()->ShouldYieldForHighPriorityWork()) {
    return 0;
  }

  return 1000.0 * Performance::ClampTimeResolution(time_remaining.InSecondsF());
}

}  // namespace blink
