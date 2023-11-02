// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ui/fling_tracker.h"

#include <cmath>

namespace remoting {

namespace {

const float kSecToMs = 0.001f;

// TODO(yuweih): May need to tweak these numbers to get better smoothness.

// Stop flinging if the speed drops below this. This is a small speed to make
// sure the animation stops smoothly.
const float kMinTrackSpeed = 0.01f;

// The minimum displacement needed to trigger the fling animation. This is to
// prevent unintentional fling with low velocity.
const float kMinDisplacement = 20.f;

float GetDisplacement(float time_constant,
                      float initial_speed_rate,
                      float time_elapsed) {
  // x = T * v0 * (1 - e^(-t / T))
  // This comes from a solution to the linear drag equation F=-kv
  float exp_factor = -std::expm1(-time_elapsed / time_constant);
  return time_constant * initial_speed_rate * exp_factor;
}

// Returns the time needed for the object to get to the stable state where the
// speed drops below kMinTrackSpeed.
float GetDuration(float time_constant, float initial_speed_rate) {
  // t = -T * ln(kMinTrackSpeed / v0)
  // Solution of v(t) = kMinTrackSpeed
  return -time_constant * std::log(kMinTrackSpeed / initial_speed_rate);
}

}  // namespace

FlingTracker::FlingTracker(float time_constant)
    : time_constant_(time_constant) {}

FlingTracker::~FlingTracker() = default;

void FlingTracker::StartFling(float velocity_x, float velocity_y) {
  // Convert to pixel/ms
  velocity_x *= kSecToMs;
  velocity_y *= kSecToMs;

  initial_speed_rate_ =
      std::sqrt(velocity_x * velocity_x + velocity_y * velocity_y);

  if (initial_speed_rate_ < kMinTrackSpeed) {
    StopFling();
    return;
  }

  fling_duration_ = GetDuration(time_constant_, initial_speed_rate_);

  float final_displacement =
      GetDisplacement(time_constant_, initial_speed_rate_, fling_duration_);
  if (final_displacement < kMinDisplacement) {
    StopFling();
    return;
  }

  velocity_ratio_x_ = velocity_x / initial_speed_rate_;
  velocity_ratio_y_ = velocity_y / initial_speed_rate_;

  previous_position_x_ = 0;
  previous_position_y_ = 0;
}

void FlingTracker::StopFling() {
  initial_speed_rate_ = 0.f;
}

bool FlingTracker::IsFlingInProgress() const {
  return initial_speed_rate_ > 0;
}

bool FlingTracker::TrackMovement(base::TimeDelta time_elapsed,
                                 float* dx,
                                 float* dy) {
  if (!IsFlingInProgress()) {
    return false;
  }

  float time_elapsed_ms = time_elapsed.InMillisecondsF();

  if (time_elapsed_ms > fling_duration_) {
    StopFling();
    return false;
  }

  float displacement =
      GetDisplacement(time_constant_, initial_speed_rate_, time_elapsed_ms);

  float position_x = displacement * velocity_ratio_x_;
  float position_y = displacement * velocity_ratio_y_;

  *dx = position_x - previous_position_x_;
  *dy = position_y - previous_position_y_;

  previous_position_x_ = position_x;
  previous_position_y_ = position_y;

  return true;
}

}  // namespace remoting
