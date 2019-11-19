/*
 * Copyright (c) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/animation_clock.h"

#include <math.h>

namespace blink {

namespace {
// This is an approximation of time between frames, used when ticking the
// animation clock outside of animation frame callbacks.
constexpr base::TimeDelta kApproximateFrameTime =
    base::TimeDelta::FromSecondsD(1 / 60.0);
}  // namespace

unsigned AnimationClock::currently_running_task_ = 0;

void AnimationClock::UpdateTime(base::TimeTicks time) {
  task_for_which_time_was_calculated_ = currently_running_task_;

  // TODO(crbug.com/985770): Change this to a DCHECK_GE(time, time_) when
  // VR no longer sends historical timestamps.
  if (time < time_)
    return;
  time_ = time;
}

base::TimeTicks AnimationClock::CurrentTime() {
  // By spec, within a single rendering lifecycle the AnimationClock time should
  // not change (as it is set from the frame time).
  if (!can_dynamically_update_time_)
    return time_;

  // Outside of the rendering lifecycle, we may have to dynamically advance our
  // own time (see comments on |SetAllowedToDynamicallyUpdateTime|). However we
  // should never dynamically advance time inside a single task, as otherwise a
  // single long-running JavaScript function could see multiple different times
  // from document.timeline.currentTime.
  if (task_for_which_time_was_calculated_ == currently_running_task_)
    return time_;

  // Otherwise, we may need to dynamically update our own time. Again see the
  // comments on |SetAllowedToDynamicallyUpdateTime|.
  const base::TimeTicks current_time = clock_->NowTicks();
  base::TimeTicks new_time = time_;
  if (time_ < current_time) {
    // Attempt to predict what the most recent timestamp would have been. This
    // may not produce a result greater than |time_|, but it greatly reduces the
    // chance of conflicting with any future frame timestamp that does come in.
    const base::TimeDelta frame_shift =
        (current_time - time_) % kApproximateFrameTime;
    new_time = current_time - frame_shift;
    DCHECK_GE(new_time, time_);
  }
  UpdateTime(new_time);

  return time_;
}

void AnimationClock::ResetTimeForTesting() {
  time_ = base::TimeTicks();
}

void AnimationClock::OverrideDynamicClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
  ResetTimeForTesting();
  UpdateTime(clock_->NowTicks());
}

}  // namespace blink
