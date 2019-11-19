// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/leaky_bucket.h"

#include "base/logging.h"

namespace remoting {

LeakyBucket::LeakyBucket(int depth, int rate)
    : depth_(depth),
      rate_(rate),
      current_level_(0),
      level_updated_time_(base::TimeTicks::Now()) {}

LeakyBucket::~LeakyBucket() = default;

bool LeakyBucket::RefillOrSpill(int drops, base::TimeTicks now) {
  UpdateLevel(now);

  int new_level = current_level_ + drops;
  if (depth_ >= 0 && new_level > depth_)
    return false;
  current_level_ = new_level;
  return true;
}

base::TimeTicks LeakyBucket::GetEmptyTime() {
  // To avoid unnecessary complexity in WebrtcFrameSchedulerSimple, we return
  // a fairly large value (1 minute) here if the b/w estimate is 0 (which means
  // that the video stream should be paused). This means that
  // WebrtcFrameSchedulerSimple does not need to handle any overflow isssues
  // caused by returning TimeDelta::Max().
  base::TimeDelta time_to_empty =
      (rate_ != 0) ? base::TimeDelta::FromMicroseconds(
                         base::TimeTicks::kMicrosecondsPerSecond *
                         current_level_ / rate_)
                   : base::TimeDelta::FromMinutes(1);
  return level_updated_time_ + time_to_empty;
}

void LeakyBucket::UpdateRate(int new_rate, base::TimeTicks now) {
  UpdateLevel(now);
  rate_ = new_rate;
}

void LeakyBucket::UpdateLevel(base::TimeTicks now) {
  int64_t drainage_amount = rate_ *
                            (now - level_updated_time_).InMicroseconds() /
                            base::TimeTicks::kMicrosecondsPerSecond;
  if (current_level_ < drainage_amount) {
    current_level_ = 0;
  } else {
    current_level_ -= drainage_amount;
  }
  level_updated_time_ = now;
}

}  // namespace remoting
