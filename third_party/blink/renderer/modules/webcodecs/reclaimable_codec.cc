// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"

#include "base/feature_list.h"
#include "base/location.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

const base::Feature kReclaimInactiveWebCodecs{"ReclaimInactiveWebCodecs",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOnlyReclaimBackgroundWebCodecs{
    "OnlyReclaimBackgroundWebCodecs", base::FEATURE_ENABLED_BY_DEFAULT};

constexpr base::TimeDelta ReclaimableCodec::kInactivityReclamationThreshold;
constexpr base::TimeDelta ReclaimableCodec::kTimerPeriod;

ReclaimableCodec::ReclaimableCodec(ExecutionContext* context)
    : tick_clock_(base::DefaultTickClock::GetInstance()),
      last_activity_(tick_clock_->NowTicks()),
      activity_timer_(Thread::Current()->GetTaskRunner(),
                      this,
                      &ReclaimableCodec::ActivityTimerFired) {
  DCHECK(context);
  if (base::FeatureList::IsEnabled(kOnlyReclaimBackgroundWebCodecs)) {
    // Do this last, it will immediately re-enter via OnLifecycleStateChanged().
    observer_handle_ = context->GetScheduler()->AddLifecycleObserver(
        FrameOrWorkerScheduler::ObserverType::kWorkerScheduler,
        WTF::BindRepeating(&ReclaimableCodec::OnLifecycleStateChanged,
                           WrapWeakPersistent(this)));
  } else {
    // Pretend we're always in the background to _always_ reclaim.
    is_backgrounded_ = true;
  }
}

void ReclaimableCodec::OnLifecycleStateChanged(
    scheduler::SchedulingLifecycleState lifecycle_state) {
  DVLOG(5) << __func__
           << " lifecycle_state=" << static_cast<int>(lifecycle_state);
  bool is_backgrounded =
      lifecycle_state != scheduler::SchedulingLifecycleState::kNotThrottled;

  // Several life cycle states map to "backgrounded", but we only want to
  // observe the transition.
  if (is_backgrounded == is_backgrounded_)
    return;

  is_backgrounded_ = is_backgrounded;

  // Nothing to do when paused.
  if (is_reclamation_paused_) {
    DCHECK(!activity_timer_.IsActive());
    return;
  }

  if (is_backgrounded_) {
    // (Re)entered background, so start timer again from "now".
    MarkCodecActive();
    DCHECK(activity_timer_.IsActive());
  } else {
    // We're in foreground, so pause reclamation to improve UX.
    PauseCodecReclamationInternal();
  }
}

void ReclaimableCodec::MarkCodecActive() {
  DVLOG(5) << __func__;
  is_reclamation_paused_ = false;
  last_activity_ = tick_clock_->NowTicks();
  last_tick_was_inactive_ = false;

  if (!is_backgrounded_) {
    DCHECK(!activity_timer_.IsActive());
    DVLOG(5) << __func__ << " Suppressing reclamation of foreground codec.";
    return;
  }

  StartTimer();
}

void ReclaimableCodec::SimulateCodecReclaimedForTesting() {
  OnCodecReclaimed(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kQuotaExceededError, "Codec reclaimed for testing."));
}

void ReclaimableCodec::SimulateActivityTimerFiredForTesting() {
  ActivityTimerFired(nullptr);
}

void ReclaimableCodec::SimulateLifecycleStateForTesting(
    scheduler::SchedulingLifecycleState state) {
  OnLifecycleStateChanged(state);
}

void ReclaimableCodec::PauseCodecReclamation() {
  DVLOG(5) << __func__;
  is_reclamation_paused_ = true;
  PauseCodecReclamationInternal();
}

void ReclaimableCodec::PauseCodecReclamationInternal() {
  DVLOG(5) << __func__;
  activity_timer_.Stop();
}

void ReclaimableCodec::StartTimer() {
  DCHECK(is_backgrounded_);
  DCHECK(!is_reclamation_paused_);

  if (activity_timer_.IsActive())
    return;

  if (base::FeatureList::IsEnabled(kReclaimInactiveWebCodecs)) {
    DVLOG(5) << __func__ << " Starting timer.";
    activity_timer_.StartRepeating(kTimerPeriod, FROM_HERE);
  }
}

void ReclaimableCodec::ActivityTimerFired(TimerBase*) {
  DCHECK(is_backgrounded_);
  DCHECK(!is_reclamation_paused_);
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
