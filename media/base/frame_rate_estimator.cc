// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/frame_rate_estimator.h"

#include <array>

namespace media {

// Number of samples we need before we'll trust the estimate.  All samples must
// end up in the same bucket, else we won't report an FPS for the window.
// kMaxSamples is the maximum that we'll need, if we think that things are
// unstable.  kMinSamples is the minimum that we'll need to establish a
// baseline fps optimistically.
static constexpr int kMaxSamples = 15;
static constexpr int kMinSamples = 3;

// Size (in FPS) of our buckets, which  take on integral multiples of
// |BucketSize|.  Observed frame rates are rounded to the nearest bucket, so
// that 1.75 and 1.25 might both end up in bucket 2.
static constexpr int BucketSize = 1;

namespace {

// Convert |duration| into an FPS bucket.
int ToBucket(base::TimeDelta duration) {
  return static_cast<int>(((1.0 / duration.InSecondsF()) + (BucketSize / 2.0)) /
                          BucketSize) *
         BucketSize;
}

}  // namespace

FrameRateEstimator::FrameRateEstimator()
    : duration_(kMaxSamples), required_samples_(kMinSamples) {}

FrameRateEstimator::~FrameRateEstimator() = default;

void FrameRateEstimator::AddSample(base::TimeDelta frame_duration) {
  duration_.AddSample(frame_duration);

  // See if the duration averages have enough samples.  If not, then we can't
  // do anything else yet.
  if (duration_.count() < required_samples_)
    return;

  // Make sure that the entire window is in the same bucket.
  auto extremes = duration_.GetMinAndMax();
  // See if the current sample is too far from the bucketed average.
  int bucketed_fps_min = ToBucket(extremes.first);
  int bucketed_fps_max = ToBucket(extremes.second);

  if (bucketed_fps_min != bucketed_fps_max) {
    // There's no current bucket until the entire window agrees.  Use the
    // maximum window size since we don't like disagreement.
    most_recent_bucket_.reset();
    required_samples_ = kMaxSamples;
    return;
  }

  most_recent_bucket_ = bucketed_fps_min;
}

absl::optional<int> FrameRateEstimator::ComputeFPS() {
  return most_recent_bucket_;
}

void FrameRateEstimator::Reset() {
  duration_.Reset();
  most_recent_bucket_.reset();
  required_samples_ = kMinSamples;
}

int FrameRateEstimator::GetRequiredSamplesForTesting() const {
  return required_samples_;
}

int FrameRateEstimator::GetMinSamplesForTesting() const {
  return kMinSamples;
}

int FrameRateEstimator::GetMaxSamplesForTesting() const {
  return kMaxSamples;
}

}  // namespace media
