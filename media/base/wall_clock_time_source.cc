// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/wall_clock_time_source.h"

#include "base/logging.h"

namespace media {

WallClockTimeSource::WallClockTimeSource()
    : tick_clock_(base::DefaultTickClock::GetInstance()),
      ticking_(false),
      playback_rate_(1.0) {}

WallClockTimeSource::~WallClockTimeSource() = default;

void WallClockTimeSource::StartTicking() {
  DVLOG(1) << __func__;
  base::AutoLock auto_lock(lock_);
  DCHECK(!ticking_);
  ticking_ = true;
  reference_time_ = tick_clock_->NowTicks();
}

void WallClockTimeSource::StopTicking() {
  DVLOG(1) << __func__;
  base::AutoLock auto_lock(lock_);
  DCHECK(ticking_);
  base_timestamp_ = CurrentMediaTime_Locked();
  ticking_ = false;
  reference_time_ = tick_clock_->NowTicks();
}

void WallClockTimeSource::SetPlaybackRate(double playback_rate) {
  DVLOG(1) << __func__ << "(" << playback_rate << ")";
  base::AutoLock auto_lock(lock_);
  // Estimate current media time using old rate to use as a new base time for
  // the new rate.
  if (ticking_) {
    base_timestamp_ = CurrentMediaTime_Locked();
    reference_time_ = tick_clock_->NowTicks();
  }

  playback_rate_ = playback_rate;
}

void WallClockTimeSource::SetMediaTime(base::TimeDelta time) {
  DVLOG(1) << __func__ << "(" << time.InMicroseconds() << ")";
  base::AutoLock auto_lock(lock_);
  CHECK(!ticking_);
  base_timestamp_ = time;
  reference_time_ = base::TimeTicks();
}

base::TimeDelta WallClockTimeSource::CurrentMediaTime() {
  base::AutoLock auto_lock(lock_);
  return CurrentMediaTime_Locked();
}

bool WallClockTimeSource::GetWallClockTimes(
    const std::vector<base::TimeDelta>& media_timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
  base::AutoLock auto_lock(lock_);
  DCHECK(wall_clock_times->empty());

  if (media_timestamps.empty()) {
    wall_clock_times->push_back(reference_time_);
  } else {
    // When playback is paused (rate is zero), assume a rate of 1.0.
    const double playback_rate = playback_rate_ ? playback_rate_ : 1.0;

    wall_clock_times->reserve(media_timestamps.size());
    for (const auto& media_timestamp : media_timestamps) {
      wall_clock_times->push_back(reference_time_ +
                                  (media_timestamp - base_timestamp_) /
                                      playback_rate);
    }
  }

  return playback_rate_ && ticking_;
}

void WallClockTimeSource::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  base::AutoLock auto_lock(lock_);
  tick_clock_ = tick_clock;
}

base::TimeDelta WallClockTimeSource::CurrentMediaTime_Locked() {
  lock_.AssertAcquired();
  if (!ticking_ || !playback_rate_)
    return base_timestamp_;

  base::TimeTicks now = tick_clock_->NowTicks();
  return base_timestamp_ +
         base::TimeDelta::FromMicroseconds(
             (now - reference_time_).InMicroseconds() * playback_rate_);
}

}  // namespace media
