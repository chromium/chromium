// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_UI_FLING_TRACKER_H_
#define REMOTING_CLIENT_UI_FLING_TRACKER_H_

#include "base/time/time.h"

namespace remoting {

// A class for tracking the positions of an object moving through a viscous
// liquid.
class FlingTracker {
 public:
  // time_constant: The larger the number the longer it takes to fling and the
  // further the object can move.
  explicit FlingTracker(float time_constant);

  ~FlingTracker();

  // Sets the position of the object and start fling. This will reset the
  // existing fling.
  // |velocity_x| and |velocity_y| need to be in pixel per second.
  void StartFling(float velocity_x, float velocity_y);

  void StopFling();

  bool IsFlingInProgress() const;

  // time_elapsed: The time elapsed since the animation has started.
  // Moves forward the object to catch up with |time_elapsed|. The change in
  // positions will be written to |dx| and |dy|.
  // Returns true if the fling is still in progress at |time_elapsed|, false
  // otherwise, in which case |dx| and |dy| will not be touched.
  bool TrackMovement(base::TimeDelta time_elapsed, float* dx, float* dy);

 private:
  float time_constant_;
  float initial_speed_rate_ = 0.f;
  float fling_duration_ = 0.f;
  float velocity_ratio_x_ = 0.f;
  float velocity_ratio_y_ = 0.f;
  float previous_position_x_ = 0.f;
  float previous_position_y_ = 0.f;

  // FlingTracker is neither copyable nor movable.
  FlingTracker(const FlingTracker&) = delete;
  FlingTracker& operator=(const FlingTracker&) = delete;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_UI_FLING_TRACKER_H_
