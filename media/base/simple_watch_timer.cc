// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/simple_watch_timer.h"

#include "base/location.h"
#include "media/base/timestamp_constants.h"

namespace media {

namespace {

constexpr base::TimeDelta kQueryInterval =
    base::TimeDelta::FromMilliseconds(750);

}  // namespace

SimpleWatchTimer::SimpleWatchTimer(TickCB tick_cb,
                                   GetCurrentTimeCB get_current_time_cb)
    : tick_cb_(std::move(tick_cb)),
      get_current_time_cb_(std::move(get_current_time_cb)) {
  DCHECK(!tick_cb_.is_null());
  DCHECK(!get_current_time_cb_.is_null());
}

SimpleWatchTimer::~SimpleWatchTimer() {}

void SimpleWatchTimer::Start() {
  if (timer_.IsRunning())
    return;

  last_current_time_ = get_current_time_cb_.Run();
  timer_.Start(FROM_HERE, kQueryInterval, this, &SimpleWatchTimer::Tick);
}

void SimpleWatchTimer::Stop() {
  if (!timer_.IsRunning())
    return;

  timer_.Stop();
  Tick();
}

void SimpleWatchTimer::Tick() {
  base::TimeDelta current_time = get_current_time_cb_.Run();
  base::TimeDelta duration;
  if (last_current_time_ != kNoTimestamp &&
      last_current_time_ != kInfiniteDuration) {
    duration = current_time - last_current_time_;
  }
  last_current_time_ = current_time;

  // Accumulate watch time if the duration is reasonable.
  if (duration > base::TimeDelta() && duration < kQueryInterval * 2) {
    unreported_ms_ += duration.InMilliseconds();
  }

  // Tick if the accumulated time is about a second.
  if (unreported_ms_ >= 500) {
    unreported_ms_ -= 1000;
    tick_cb_.Run();
  }
}

}  // namespace media
