// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_RECLAIMABLE_CODEC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_RECLAIMABLE_CODEC_H_

#include "base/feature_list.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/timer.h"

#include <memory>

namespace base {
class TickClock;
}  // namespace base

namespace blink {

class DOMException;

extern const MODULES_EXPORT base::Feature kReclaimInactiveWebCodecs;
extern const MODULES_EXPORT base::Feature kOnlyReclaimBackgroundWebCodecs;

class MODULES_EXPORT ReclaimableCodec
    : public ExecutionContextLifecycleObserver {
 public:
  // Use 1.5 minutes since some RDP clients are only ticking at 1 FPM.
  static constexpr base::TimeDelta kInactivityReclamationThreshold =
      base::Seconds(90);

  enum class CodecType {
    kDecoder,
    kEncoder,
  };

  ReclaimableCodec(CodecType, ExecutionContext*);
  ~ReclaimableCodec() override = default;

  // GarbageCollectedMixin override.
  void Trace(Visitor*) const override;

  // Apply or release pressure, if this codec is holding on to constrained
  // resources.
  void ApplyCodecPressure();
  void ReleaseCodecPressure();

  // Notified when throttling state is changed. May be called consecutively
  // with the same value.
  void OnLifecycleStateChanged(scheduler::SchedulingLifecycleState);

  bool is_applying_codec_pressure() const { return is_applying_pressure_; }

  // Test support.
  void SimulateCodecReclaimedForTesting();
  void SimulateActivityTimerFiredForTesting();
  void SimulateLifecycleStateForTesting(scheduler::SchedulingLifecycleState);

  bool IsReclamationTimerActiveForTesting() {
    return activity_timer_.IsActive();
  }

  bool is_backgrounded_for_testing() { return is_backgrounded_; }

  void set_tick_clock_for_testing(const base::TickClock* clock) {
    tick_clock_ = clock;
  }

 protected:
  // Pushes back the time at which |this| can be reclaimed due to inactivity.
  void MarkCodecActive();

  virtual void OnCodecReclaimed(DOMException*) = 0;

  base::TimeTicks last_activity_for_testing() const { return last_activity_; }

 private:
  // Starts the idle reclamation timer if all preconditions are met, or stops it
  // otherwise. Called when any of the following criteria change:
  //   - |this| applies/releases codec pressure.
  //   - |this|'s background status changes.
  void OnReclamationPreconditionsUpdated();

  bool AreReclamationPreconditionsMet();

  void StartIdleReclamationTimer();
  void StopIdleReclamationTimer();

  void OnActivityTimerFired(TimerBase*);

  // This is used to make sure that there are two consecutive ticks of the
  // timer, before we reclaim for inactivity. This prevents immediately
  // reclaiming otherwise active codecs, right after a page suspended/resumed.
  bool last_tick_was_inactive_ = false;

  // Whether this codec is holding on to platform resources.
  bool is_applying_pressure_ = false;

  const base::TickClock* tick_clock_;

  // Period of time after which a codec is considered to be inactive.
  base::TimeDelta inactivity_threshold_;

  base::TimeTicks last_activity_;
  HeapTaskRunnerTimer<ReclaimableCodec> activity_timer_;

  // True iff document.visibilityState of the associated page is "hidden".
  // This includes being in bg of tab strip, minimized, or (depending on OS)
  // covered by other windows.
  bool is_backgrounded_ = false;

  // Handle to unhook from FrameOrWorkerScheduler upon destruction.
  std::unique_ptr<FrameOrWorkerScheduler::LifecycleObserverHandle>
      observer_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DATA_H_
