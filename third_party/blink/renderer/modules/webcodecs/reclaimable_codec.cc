// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"

#include "base/location.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager_provider.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

constexpr base::TimeDelta ReclaimableCodec::kInactivityReclamationThreshold;

ReclaimableCodec::ReclaimableCodec(CodecType type, ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context),
      codec_type_(type),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      inactivity_threshold_(kInactivityReclamationThreshold),
      last_activity_(tick_clock_->NowTicks()),
      activity_timer_(context->GetTaskRunner(TaskType::kInternalMedia),
                      this,
                      &ReclaimableCodec::OnActivityTimerFired) {
  DCHECK(context);
  // Do this last, it will immediately re-enter via OnLifecycleStateChanged().
  observer_handle_ = context->GetScheduler()->AddLifecycleObserver(
      FrameOrWorkerScheduler::ObserverType::kWorkerScheduler,
      WTF::BindRepeating(&ReclaimableCodec::OnLifecycleStateChanged,
                         WrapWeakPersistent(this)));
}

void ReclaimableCodec::Trace(Visitor* visitor) const {
  visitor->Trace(activity_timer_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void ReclaimableCodec::ApplyCodecPressure() {
  if (is_applying_pressure_)
    return;

  is_applying_pressure_ = true;

  if (auto* pressure_manager = PressureManager())
    pressure_manager->AddCodec(this);
}

void ReclaimableCodec::ReleaseCodecPressure() {
  if (!is_applying_pressure_) {
    DCHECK(!activity_timer_.IsActive());
    return;
  }

  if (auto* pressure_manager = PressureManager()) {
    // If we fail to get |pressure_manager| here (say, because the
    // ExecutionContext is being destroyed), this is harmless. The
    // CodecPressureManager maintains its own local pressure count, and it will
    // properly decrement it from the global pressure count upon the manager's
    // disposal. The CodecPressureManager's WeakMember reference to |this| will
    // be cleared by the GC when |this| is disposed. The manager might still
    // call into SetGlobalPressureExceededFlag() before |this| is disposed, but
    // we will simply noop those calls.
    pressure_manager->RemoveCodec(this);
  }

  // We might still exceed global codec pressure at this point, but this codec
  // isn't contributing to it, and needs to reset its own flag.
  SetGlobalPressureExceededFlag(false);

  is_applying_pressure_ = false;
}

void ReclaimableCodec::Dispose() {
  if (!is_applying_pressure_)
    return;

  if (auto* pressure_manager = PressureManager())
    pressure_manager->OnCodecDisposed(this);
}

void ReclaimableCodec::SetGlobalPressureExceededFlag(
    bool global_pressure_exceeded) {
  if (!is_applying_pressure_) {
    // We should only hit this call because we failed to get the
    // PressureManager() in ReleaseCodecPressure(). See the note above.
    DCHECK(!PressureManager());
    return;
  }

  if (global_pressure_exceeded_ == global_pressure_exceeded)
    return;

  global_pressure_exceeded_ = global_pressure_exceeded;

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
  // If |global_pressure_exceeded_| is true, so should |is_applying_pressure_|.
  DCHECK_EQ(global_pressure_exceeded_,
            global_pressure_exceeded_ && is_applying_pressure_);

  return is_applying_pressure_ && global_pressure_exceeded_ && is_backgrounded_;
}

void ReclaimableCodec::StartIdleReclamationTimer() {
  DCHECK(AreReclamationPreconditionsMet());

  if (activity_timer_.IsActive())
    return;

  DVLOG(5) << __func__ << " Starting timer.";
  activity_timer_.StartRepeating(inactivity_threshold_ / 2, FROM_HERE);
}

void ReclaimableCodec::StopIdleReclamationTimer() {
  DCHECK(!AreReclamationPreconditionsMet());

  activity_timer_.Stop();
}

void ReclaimableCodec::OnActivityTimerFired(TimerBase*) {
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

CodecPressureManager* ReclaimableCodec::PressureManager() {
  auto* execution_context = GetExecutionContext();

  if (!execution_context || execution_context->IsContextDestroyed())
    return nullptr;

  auto& manager_provider =
      CodecPressureManagerProvider::From(*execution_context);

  switch (codec_type_) {
    case CodecType::kDecoder:
      return manager_provider.GetDecoderPressureManager();
    case CodecType::kEncoder:
      return manager_provider.GetEncoderPressureManager();
  }
}

}  // namespace blink
