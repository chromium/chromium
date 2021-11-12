// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_RECLAIMABLE_CODEC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_RECLAIMABLE_CODEC_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace base {
class TickClock;
}  // namespace base

namespace blink {

class DOMException;

class MODULES_EXPORT ReclaimableCodec : public GarbageCollectedMixin {
 public:
  ReclaimableCodec();

  // GarbageCollectedMixin override.
  void Trace(Visitor*) const override;

  bool IsReclamationTimerActiveForTesting() {
    return activity_timer_.IsActive();
  }

  void SimulateCodecReclaimedForTesting();
  void SimulateActivityTimerFiredForTesting();

  void set_tick_clock_for_testing(const base::TickClock* clock) {
    tick_clock_ = clock;
  }

  // Use 1.5 minutes since some RDP clients are only ticking at 1 FPM.
  static constexpr base::TimeDelta kInactivityReclamationThreshold =
      base::Seconds(90);
  static constexpr base::TimeDelta kTimerPeriod =
      kInactivityReclamationThreshold / 2;

 protected:
  // Pushes back the time at which |this| can be reclaimed due to inactivity.
  // Starts a inactivity reclamation timer, if it isn't already running.
  void MarkCodecActive();

  // Called when a codec should no longer be reclaimed, such as when it is not
  // holding on to any resources.
  //
  // Calling MarkCodecActive() will automatically unpause reclamation.
  void PauseCodecReclamation();

  virtual void OnCodecReclaimed(DOMException*) = 0;

 private:
  void ActivityTimerFired(TimerBase*);
  void StartTimer();

  // This is used to make sure that there are two consecutive ticks of the
  // timer, before we reclaim for inactivity. This prevents immediately
  // reclaiming otherwise active codecs, right after a page suspended/resumed.
  bool last_tick_was_inactive_ = false;

  const base::TickClock* tick_clock_;

  base::TimeTicks last_activity_;
  HeapTaskRunnerTimer<ReclaimableCodec> activity_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_DATA_H_
