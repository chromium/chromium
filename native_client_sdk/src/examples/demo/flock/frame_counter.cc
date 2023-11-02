// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include <limits>

#include "nacl_app/flock.h"

void FrameCounter::BeginFrame() {
  struct timeval start_time;
  gettimeofday(&start_time, NULL);
  frame_start_ = start_time.tv_sec * kMicroSecondsPerSecond +
                 start_time.tv_usec;
}

void FrameCounter::EndFrame() {
  struct timeval end_time;
  gettimeofday(&end_time, NULL);
  double frame_end = end_time.tv_sec * kMicroSecondsPerSecond +
                     end_time.tv_usec;
  double dt = frame_end - frame_start_;
  if (dt < 0)
    return;
  frame_duration_accumulator_ += dt;
  frame_count_++;
  if (frame_count_ > kFrameRateRefreshCount ||
      frame_duration_accumulator_ >= kMicroSecondsPerSecond) {
    double elapsed_time = frame_duration_accumulator_ /
                          kMicroSecondsPerSecond;
    if (fabs(elapsed_time) > std::numeric_limits<double>::epsilon()) {
      frames_per_second_ = frame_count_ / elapsed_time;
    }
    frame_duration_accumulator_ = 0;
    frame_count_ = 0;
  }
}

void FrameCounter::Reset() {
  frames_per_second_ = 0;
  frame_duration_accumulator_ = 0;
  frame_count_ = 0;
}
