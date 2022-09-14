// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_timestamp_helper.h"

#include <cmath>

#include "base/check_op.h"
#include "media/base/timestamp_constants.h"

namespace media {

// static
base::TimeDelta AudioTimestampHelper::FramesToTime(int64_t frames,
                                                   int samples_per_second) {
  DCHECK_GT(samples_per_second, 0);
  return base::Microseconds(frames * base::Time::kMicrosecondsPerSecond /
                            samples_per_second);
}

// static
int64_t AudioTimestampHelper::TimeToFrames(base::TimeDelta time,
                                           int samples_per_second) {
  return std::round(time.InSecondsF() * samples_per_second);
}

AudioTimestampHelper::AudioTimestampHelper(int samples_per_second)
    : base_timestamp_(kNoTimestamp), frame_count_(0) {
  DCHECK_GT(samples_per_second, 0);
  double fps = samples_per_second;
  microseconds_per_frame_ = base::Time::kMicrosecondsPerSecond / fps;
}

void AudioTimestampHelper::SetBaseTimestamp(base::TimeDelta base_timestamp) {
  base_timestamp_ = base_timestamp;
  frame_count_ = 0;
}

base::TimeDelta AudioTimestampHelper::base_timestamp() const {
  return base_timestamp_;
}

void AudioTimestampHelper::AddFrames(int frame_count) {
  DCHECK_GE(frame_count, 0);
  DCHECK(base_timestamp_ != kNoTimestamp);
  frame_count_ += frame_count;
}

base::TimeDelta AudioTimestampHelper::GetTimestamp() const {
  return ComputeTimestamp(frame_count_);
}

base::TimeDelta AudioTimestampHelper::GetFrameDuration(int frame_count) const {
  DCHECK_GE(frame_count, 0);
  base::TimeDelta current_timestamp = GetTimestamp();
  base::TimeDelta end_timestamp = ComputeTimestamp(frame_count_ + frame_count);

  if ((current_timestamp.is_min() && end_timestamp.is_min()) ||
      (current_timestamp.is_max() && end_timestamp.is_max())) {
    return base::TimeDelta();
  }

  return end_timestamp - current_timestamp;
}

int64_t AudioTimestampHelper::GetFramesToTarget(base::TimeDelta target) const {
  DCHECK(base_timestamp_ != kNoTimestamp);
  DCHECK(target >= base_timestamp_);

  int64_t delta_in_us = (target - GetTimestamp()).InMicroseconds();
  if (delta_in_us == 0)
    return 0;

  // Compute a timestamp relative to |base_timestamp_| since timestamps
  // created from |frame_count_| are computed relative to this base.
  // This ensures that the time to frame computation here is the proper inverse
  // of the frame to time computation in ComputeTimestamp().
  base::TimeDelta delta_from_base = target - base_timestamp_;

  // Compute frame count for the time delta. This computation rounds to
  // the nearest whole number of frames.
  double threshold = microseconds_per_frame_ / 2;
  int64_t target_frame_count =
      (delta_from_base.InMicroseconds() + threshold) / microseconds_per_frame_;
  return target_frame_count - frame_count_;
}

base::TimeDelta AudioTimestampHelper::ComputeTimestamp(
    int64_t frame_count) const {
  DCHECK_GE(frame_count, 0);
  DCHECK(base_timestamp_ != kNoTimestamp);
  double frames_us = microseconds_per_frame_ * frame_count;
  return base_timestamp_ + base::Microseconds(frames_us);
}

}  // namespace media
