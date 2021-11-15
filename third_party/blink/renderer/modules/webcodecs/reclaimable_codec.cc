// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"

#include "base/feature_list.h"
#include "base/location.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

const base::Feature kReclaimInactiveWebCodecs{"ReclaimInactiveWebCodecs",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

constexpr base::TimeDelta ReclaimableCodec::kInactivityReclamationThreshold;
constexpr base::TimeDelta ReclaimableCodec::kTimerPeriod;

ReclaimableCodec::ReclaimableCodec()
    : tick_clock_(base::DefaultTickClock::GetInstance()),
      last_activity_(tick_clock_->NowTicks()),
      activity_timer_(Thread::Current()->GetTaskRunner(),
                      this,
                      &ReclaimableCodec::ActivityTimerFired) {}

void ReclaimableCodec::MarkCodecActive() {
  last_activity_ = tick_clock_->NowTicks();
  last_tick_was_inactive_ = false;
  StartTimer();
}

void ReclaimableCodec::SimulateCodecReclaimedForTesting() {
  OnCodecReclaimed(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kQuotaExceededError, "Codec reclaimed for testing."));
}

void ReclaimableCodec::SimulateActivityTimerFiredForTesting() {
  ActivityTimerFired(nullptr);
}

void ReclaimableCodec::PauseCodecReclamation() {
  activity_timer_.Stop();
}

void ReclaimableCodec::StartTimer() {
  if (activity_timer_.IsActive())
    return;

  if (base::FeatureList::IsEnabled(kReclaimInactiveWebCodecs))
    activity_timer_.StartRepeating(kTimerPeriod, FROM_HERE);
}

void ReclaimableCodec::ActivityTimerFired(TimerBase*) {
  DCHECK(base::FeatureList::IsEnabled(kReclaimInactiveWebCodecs));

  auto time_inactive = tick_clock_->NowTicks() - last_activity_;
  bool is_inactive = time_inactive >= kInactivityReclamationThreshold;

  // Do not immediately reclaim. Make sure the codec is inactive for 2 ticks.
  // Otherwise, tabs that were suspended could see their codecs reclaimed
  // immediately after being resumed.
  if (is_inactive && last_tick_was_inactive_) {
    activity_timer_.Stop();
    OnCodecReclaimed(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kQuotaExceededError,
        "Codec reclaimed due to inactivity."));
  }

  last_tick_was_inactive_ =
      time_inactive >= (kInactivityReclamationThreshold / 2);
}

void ReclaimableCodec::Trace(Visitor* visitor) const {
  visitor->Trace(activity_timer_);
}

}  // namespace blink
