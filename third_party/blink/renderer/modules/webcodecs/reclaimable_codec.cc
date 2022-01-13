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

ReclaimableCodec::ReclaimableCodec(CodecType type, ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      inactivity_threshold_(kInactivityReclamationThreshold),
      last_activity_(tick_clock_->NowTicks()),
      activity_timer_(Thread::Current()->GetTaskRunner(),
                      this,
                      &ReclaimableCodec::OnActivityTimerFired) {
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

void ReclaimableCodec::Trace(Visitor* visitor) const {
  visitor->Trace(activity_timer_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void ReclaimableCodec::ApplyCodecPressure() {
  if (is_applying_pressure_)
    return;

  is_applying_pressure_ = true;

  OnReclamationPreconditionsUpdated();
}

void ReclaimableCodec::ReleaseCodecPressure() {
  if (!is_applying_pressure_) {
    DCHECK(!activity_timer_.IsActive());
    return;
  }

  is_applying_pressure_ = false;
  OnReclamationPreconditionsUpdated();
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

  // Make sure we wait the full inactivity timer period before reclaiming a
  // newly backgrounded codec.
  if (is_backgrounded_)
    MarkCodecActive();

  OnReclamationPreconditionsUpdated();
}

void ReclaimableCodec::SimulateLifecycleStateForTesting(
    scheduler::SchedulingLifecycleState state) {
  OnLifecycleStateChanged(state);
}

void ReclaimableCodec::SimulateCodecReclaimedForTesting() {
  OnCodecReclaimed(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kQuotaExceededError, "Codec reclaimed for testing."));
}

void ReclaimableCodec::SimulateActivityTimerFiredForTesting() {
  OnActivityTimerFired(nullptr);
}

void ReclaimableCodec::MarkCodecActive() {
  last_activity_ = tick_clock_->NowTicks();
  last_tick_was_inactive_ = false;
}

void ReclaimableCodec::OnReclamationPreconditionsUpdated() {
  if (AreReclamationPreconditionsMet())
    StartIdleReclamationTimer();
  else
    StopIdleReclamationTimer();
}

bool ReclaimableCodec::AreReclamationPreconditionsMet() {
  return is_applying_pressure_ && is_backgrounded_;
}

void ReclaimableCodec::StartIdleReclamationTimer() {
  DCHECK(AreReclamationPreconditionsMet());

  if (activity_timer_.IsActive())
    return;

  if (base::FeatureList::IsEnabled(kReclaimInactiveWebCodecs)) {
    DVLOG(5) << __func__ << " Starting timer.";
    activity_timer_.StartRepeating(inactivity_threshold_ / 2, FROM_HERE);
  }
}

void ReclaimableCodec::StopIdleReclamationTimer() {
  DCHECK(!AreReclamationPreconditionsMet());

  activity_timer_.Stop();
}

void ReclaimableCodec::OnActivityTimerFired(TimerBase*) {
  DCHECK(base::FeatureList::IsEnabled(kReclaimInactiveWebCodecs));
  DCHECK(AreReclamationPreconditionsMet());

  auto time_inactive = tick_clock_->NowTicks() - last_activity_;
  bool is_inactive = time_inactive >= inactivity_threshold_;

  // Do not immediately reclaim. Make sure the codec is inactive for 2 ticks.
  // Otherwise, tabs that were suspended could see their codecs reclaimed
  // immediately after being resumed.
  if (is_inactive && last_tick_was_inactive_) {
    activity_timer_.Stop();
    OnCodecReclaimed(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kQuotaExceededError,
        "Codec reclaimed due to inactivity."));
  }

  last_tick_was_inactive_ = time_inactive >= (inactivity_threshold_ / 2);
}

}  // namespace blink
