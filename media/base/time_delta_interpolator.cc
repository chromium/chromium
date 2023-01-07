// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/time_delta_interpolator.h"

#include <stdint.h>

#include <algorithm>

#include "base/check.h"
#include "base/time/tick_clock.h"
#include "media/base/timestamp_constants.h"

namespace media {

TimeDeltaInterpolator::TimeDeltaInterpolator(const base::TickClock* tick_clock)
    : tick_clock_(tick_clock),
      interpolating_(false),
      upper_bound_(kNoTimestamp),
      playback_rate_(0) {
  DCHECK(tick_clock_);
}

TimeDeltaInterpolator::~TimeDeltaInterpolator() = default;

base::TimeDelta TimeDeltaInterpolator::StartInterpolating() {
  DCHECK(!interpolating_);
  reference_ = tick_clock_->NowTicks();
  interpolating_ = true;
  return lower_bound_;
}

base::TimeDelta TimeDeltaInterpolator::StopInterpolating() {
  DCHECK(interpolating_);
  lower_bound_ = GetInterpolatedTime();
  interpolating_ = false;
  return lower_bound_;
}

void TimeDeltaInterpolator::SetPlaybackRate(double playback_rate) {
  lower_bound_ = GetInterpolatedTime();
  reference_ = tick_clock_->NowTicks();
  playback_rate_ = playback_rate;
}

void TimeDeltaInterpolator::SetBounds(base::TimeDelta lower_bound,
                                      base::TimeDelta upper_bound,
                                      base::TimeTicks capture_time) {
  DCHECK(lower_bound <= upper_bound);
  DCHECK(lower_bound != kNoTimestamp);

  lower_bound_ = std::max(base::TimeDelta(), lower_bound);
  upper_bound_ = std::max(base::TimeDelta(), upper_bound);
  reference_ = capture_time;
}

void TimeDeltaInterpolator::SetUpperBound(base::TimeDelta upper_bound) {
  DCHECK(upper_bound != kNoTimestamp);

  lower_bound_ = GetInterpolatedTime();
  reference_ = tick_clock_->NowTicks();
  upper_bound_ = upper_bound;
}

base::TimeDelta TimeDeltaInterpolator::GetInterpolatedTime() {
  if (!interpolating_)
    return lower_bound_;

  int64_t now_us = (tick_clock_->NowTicks() - reference_).InMicroseconds();
  now_us = static_cast<int64_t>(now_us * playback_rate_);
  base::TimeDelta interpolated_time = lower_bound_ + base::Microseconds(now_us);

  if (upper_bound_ == kNoTimestamp)
    return interpolated_time;

  return std::min(interpolated_time, upper_bound_);
}

}  // namespace media
