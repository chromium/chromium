// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_UI_FLING_ANIMATION_H_
#define REMOTING_CLIENT_UI_FLING_ANIMATION_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "remoting/client/ui/fling_tracker.h"

namespace remoting {

// This class helps interpolating the positions of an object with the given
// initial velocity and feeds the change in position back to the callback.
class FlingAnimation {
 public:
  // arguments are delta_x and delta_y with respect to the positions at previous
  // tick.
  using FlingCallback = base::RepeatingCallback<void(float, float)>;

  FlingAnimation(float time_constant, const FlingCallback& fling_callback);
  ~FlingAnimation();

  // (Re)starts the fling animation with the given initial velocity.
  void SetVelocity(float velocity_x, float velocity_y);

  bool IsAnimationInProgress() const;

  // Moves forward the animation to catch up with current time. Calls the fling
  // callback with the new positions. No-op if fling animation is not in
  // progress.
  void Tick();

  // Aborts the animation.
  void Abort();

  void SetTickClockForTest(const base::TickClock* clock);

 private:
  FlingTracker fling_tracker_;
  FlingCallback fling_callback_;

  base::TimeTicks fling_start_time_;

  raw_ptr<const base::TickClock> clock_;

  // FlingAnimation is neither copyable nor movable.
  FlingAnimation(const FlingAnimation&) = delete;
  FlingAnimation& operator=(const FlingAnimation&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_UI_FLING_ANIMATION_H_
