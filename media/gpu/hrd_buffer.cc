// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/hrd_buffer.h"

#include "base/check.h"
#include "base/check_op.h"
#include "media/gpu/h264_rate_control_util.h"

namespace media {
HRDBuffer::HRDBuffer(size_t buffer_size, uint32_t avg_bitrate) {
  SetParameters(buffer_size, avg_bitrate, 0, false);
}

HRDBuffer::HRDBuffer(size_t buffer_size,
                     uint32_t avg_bitrate,
                     int last_frame_buffer_bytes,
                     base::TimeDelta last_frame_timestamp)
    : last_frame_buffer_bytes_(last_frame_buffer_bytes),
      last_frame_timestamp_(last_frame_timestamp) {
  SetParameters(buffer_size, avg_bitrate, 0, false);
}

HRDBuffer::~HRDBuffer() = default;

int HRDBuffer::GetBytesAtTime(base::TimeDelta timestamp) const {
  const base::TimeDelta elapsed_time =
      h264_rate_control_util::ClampedTimestampDiff(timestamp,
                                                   last_frame_timestamp_);
  const float max_rate_bytes_per_sec = avg_bitrate_ / 8.0f;
  const float max_flow_in_bytes_since_last_frame =
      max_rate_bytes_per_sec / base::Time::kMicrosecondsPerSecond *
      elapsed_time.InMicroseconds();
  return std::max(static_cast<int>(last_frame_buffer_bytes_ -
                                   max_flow_in_bytes_since_last_frame),
                  0);
}

int HRDBuffer::GetBytesRemainingAtTime(base::TimeDelta timestamp) const {
  int bytes_remaining = buffer_size_ - GetBytesAtTime(timestamp);
  return std::max(bytes_remaining, 0);
}

void HRDBuffer::SetParameters(size_t buffer_size,
                              uint32_t avg_bitrate,
                              uint32_t peak_bitrate,
                              bool ease_hrd_reduction) {
  new_buffer_size_ = buffer_size;
  size_t old_buffer_size = buffer_size_;
  buffer_size_ = buffer_size;
  avg_bitrate_ = avg_bitrate;

  // Buffer shrinking time is limited to the interval 500 ms - 5 seconds. The
  // limiting values are chosen arbitrarily. Buffer size is changed instantly
  // for the values below 500 ms and the shrinking time is never greater than 5
  // seconds.
  constexpr base::TimeDelta kMaxShrinkingBucketWaitTime = base::Seconds(5);
  constexpr base::TimeDelta kMinShrinkingBucketWaitTime =
      base::Milliseconds(500);

  base::TimeDelta wait_time = base::TimeDelta();
  // Check if HRD becomes smaller. If so, gradually converge to the smaller
  // size.
  if (last_frame_buffer_bytes_ > 0 && ease_hrd_reduction &&
      old_buffer_size > new_buffer_size_) {
    float peak_avg_ratio = 1;
    if (avg_bitrate > 0) {
      peak_avg_ratio =
          static_cast<float>(peak_bitrate) / static_cast<float>(avg_bitrate);
    }

    // Shrink HRD buffer size over time - the duration depends on the peak to
    // average bitrate ratio.
    wait_time = std::max(
        wait_time, base::Microseconds((peak_avg_ratio - 1.0f) *
                                      base::Time::kMicrosecondsPerSecond));
    wait_time = std::min(wait_time, kMaxShrinkingBucketWaitTime);
    if (wait_time > kMinShrinkingBucketWaitTime) {
      buffer_size_delta_rate_ = old_buffer_size - buffer_size;
      buffer_size_ = old_buffer_size;
    } else {
      // peak_avg_ratio < 1.5, apply new buffer size immediately.
      wait_time = base::TimeDelta();
    }
  }
  shrinking_bucket_wait_time_ = wait_time;
}

void HRDBuffer::Shrink(base::TimeDelta timestamp) {
  if (new_buffer_size_ >= buffer_size_ ||
      shrinking_bucket_wait_time_ <= base::TimeDelta()) {
    return;
  }

  base::TimeDelta elapsed_time = h264_rate_control_util::ClampedTimestampDiff(
      timestamp, last_frame_timestamp_);
  size_t target_buffer_size =
      buffer_size_ -
      static_cast<int>((elapsed_time.InMillisecondsF() /
                        shrinking_bucket_wait_time_.InMillisecondsF()) *
                       buffer_size_delta_rate_);
  buffer_size_ = std::max(target_buffer_size, new_buffer_size_);
}

void HRDBuffer::AddFrameBytes(size_t frame_bytes,
                              base::TimeDelta frame_timestamp) {
  const int buffer_bytes = GetBytesAtTime(frame_timestamp);
  const int buffer_bytes_new = buffer_bytes + frame_bytes;

  frame_overshooting_ = buffer_bytes_new > static_cast<int>(buffer_size_);

  last_frame_buffer_bytes_ = buffer_bytes_new;
  last_frame_timestamp_ = frame_timestamp;
}

}  // namespace media
