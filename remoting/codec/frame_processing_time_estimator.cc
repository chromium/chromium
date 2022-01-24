// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/frame_processing_time_estimator.h"

#include <algorithm>

#include "base/check.h"
#include "remoting/base/constants.h"

namespace remoting {

namespace {

// We tracks the frame information in last 6 seconds.
static constexpr int kWindowSizeInSeconds = 6;

// A key-frame is assumed to be generated roughly every 3 seconds, though the
// accurate frequency is dependent on host/client software versions, the encoder
// being used, and the quality of the network.
static constexpr int kKeyFrameWindowSize = kWindowSizeInSeconds / 3;

// The count of delta frames we are tracking.
static constexpr int kDeltaFrameWindowSize =
    kTargetFrameRate * kWindowSizeInSeconds - kKeyFrameWindowSize;

// The count of bandwidth estimates we are tracking.
static constexpr int kBandwidthEstimateWindowSize =
    kTargetFrameRate * kWindowSizeInSeconds;

// The size of the circular_deque<TimeTicks> we are using to track the time
// interval between frames.
static constexpr int kFrameFinishTicksCount = kBandwidthEstimateWindowSize;

base::TimeDelta CalculateEstimatedTransitTime(int size, int kbps) {
  return base::Microseconds(size * 1000 * 8 / kbps);
}

// Uses the |time| to estimate the frame rate, and round the result in ceiling.
// May return values over |kTargetFrameRate|.
int CalculateEstimatedFrameRate(base::TimeDelta time) {
  if (time.is_zero()) {
    return kTargetFrameRate;
  } else {
    int64_t us = time.InMicroseconds();
    return (base::Time::kMicrosecondsPerSecond + us - 1) / us;
  }
}

}  // namespace

FrameProcessingTimeEstimator::FrameProcessingTimeEstimator()
    : delta_frame_processing_us_(kDeltaFrameWindowSize),
      delta_frame_size_(kDeltaFrameWindowSize),
      key_frame_processing_us_(kKeyFrameWindowSize),
      key_frame_size_(kKeyFrameWindowSize),
      frame_finish_ticks_(),
      bandwidth_kbps_(kBandwidthEstimateWindowSize) {
  frame_finish_ticks_.reserve(kFrameFinishTicksCount);
}

FrameProcessingTimeEstimator::~FrameProcessingTimeEstimator() = default;

void FrameProcessingTimeEstimator::FinishFrame(
    const WebrtcVideoEncoder::EncodedFrame& frame) {
  if (!frame.stats) {
    return;
  }

  base::TimeTicks start_time = frame.stats->capture_started_time;
  base::TimeTicks end_time = frame.stats->encode_ended_time;
  DCHECK(!start_time.is_null());
  DCHECK(!end_time.is_null());

  if (frame_finish_ticks_.size() == kFrameFinishTicksCount) {
    frame_finish_ticks_.pop_front();
  }
  frame_finish_ticks_.push_back(end_time);
  DCHECK(frame_finish_ticks_.size() <= kFrameFinishTicksCount);
  if (frame.key_frame) {
    key_frame_processing_us_.Record((end_time - start_time).InMicroseconds());
    key_frame_size_.Record(frame.data.length());
    key_frame_count_++;
  } else {
    delta_frame_processing_us_.Record((end_time - start_time).InMicroseconds());
    delta_frame_size_.Record(frame.data.length());
    delta_frame_count_++;
  }
}

void FrameProcessingTimeEstimator::SetBandwidthKbps(int bandwidth_kbps) {
  if (bandwidth_kbps >= 0) {
    bandwidth_kbps_.Record(bandwidth_kbps);
  }
}

base::TimeDelta FrameProcessingTimeEstimator::EstimatedProcessingTime(
    bool key_frame) const {
  // Avoid returning 0 if there are no records for delta-frames.
  if ((key_frame && !key_frame_processing_us_.IsEmpty()) ||
      delta_frame_processing_us_.IsEmpty()) {
    return base::Microseconds(key_frame_processing_us_.Average());
  }
  return base::Microseconds(delta_frame_processing_us_.Average());
}

base::TimeDelta FrameProcessingTimeEstimator::EstimatedTransitTime(
    bool key_frame) const {
  if (bandwidth_kbps_.IsEmpty()) {
    // To avoid unnecessary complexity in WebrtcFrameSchedulerSimple, we return
    // a fairly large value (1 minute) here. So WebrtcFrameSchedulerSimple does
    // not need to handle the overflow issue caused by returning
    // TimeDelta::Max().
    return base::Minutes(1);
  }
  // Avoid returning 0 if there are no records for delta-frames.
  if ((key_frame && !key_frame_size_.IsEmpty()) ||
      delta_frame_size_.IsEmpty()) {
    return CalculateEstimatedTransitTime(
        key_frame_size_.Average(), AverageBandwidthKbps());
  }
  return CalculateEstimatedTransitTime(
      delta_frame_size_.Average(), AverageBandwidthKbps());
}

int FrameProcessingTimeEstimator::AverageBandwidthKbps() const {
  return bandwidth_kbps_.Average();
}

int FrameProcessingTimeEstimator::EstimatedFrameSize() const {
  if (delta_frame_count_ + key_frame_count_ == 0) {
    return 0;
  }
  double key_frame_rate = key_frame_count_;
  key_frame_rate /= (delta_frame_count_ + key_frame_count_);
  return key_frame_rate * key_frame_size_.Average() +
         (1 - key_frame_rate) * delta_frame_size_.Average();
}

base::TimeDelta FrameProcessingTimeEstimator::EstimatedProcessingTime() const {
  if (delta_frame_count_ + key_frame_count_ == 0) {
    return base::TimeDelta();
  }
  double key_frame_rate = key_frame_count_;
  key_frame_rate /= (delta_frame_count_ + key_frame_count_);
  return base::Microseconds(
      key_frame_rate * key_frame_processing_us_.Average() +
      (1 - key_frame_rate) * delta_frame_processing_us_.Average());
}

base::TimeDelta FrameProcessingTimeEstimator::EstimatedTransitTime() const {
  if (bandwidth_kbps_.IsEmpty()) {
    return base::Minutes(1);
  }
  return CalculateEstimatedTransitTime(
      EstimatedFrameSize(), AverageBandwidthKbps());
}

base::TimeDelta FrameProcessingTimeEstimator::
RecentAverageFrameInterval() const {
  if (frame_finish_ticks_.size() < 2) {
    return base::TimeDelta();
  }

  return (frame_finish_ticks_.back() - frame_finish_ticks_.front()) /
         (frame_finish_ticks_.size() - 1);
}

int FrameProcessingTimeEstimator::RecentFrameRate() const {
  return std::min(kTargetFrameRate,
                  CalculateEstimatedFrameRate(RecentAverageFrameInterval()));
}

int FrameProcessingTimeEstimator::PredictedFrameRate() const {
  return std::min({
      kTargetFrameRate,
      CalculateEstimatedFrameRate(EstimatedProcessingTime()),
      CalculateEstimatedFrameRate(EstimatedTransitTime())
  });
}

int FrameProcessingTimeEstimator::EstimatedFrameRate() const {
  return std::min(RecentFrameRate(), PredictedFrameRate());
}

base::TimeTicks FrameProcessingTimeEstimator::Now() const {
  return base::TimeTicks::Now();
}

}  // namespace remoting
