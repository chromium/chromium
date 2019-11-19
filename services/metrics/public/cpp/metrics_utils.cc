// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/metrics_utils.h"

#include <cmath>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace ukm {

int64_t GetExponentialBucketMin(int64_t sample, double bucket_spacing) {
  if (sample <= 0) {
    return 0;
  }
  // This is similar to the bucketing methodology used in histograms, but
  // instead of iteratively calculating each bucket, this calculates the lower
  // end of the specific bucket for network and cached bytes.
  return std::ceil(std::pow(
      bucket_spacing, std::floor(std::log(sample) / std::log(bucket_spacing))));
}

int64_t GetExponentialBucketMinForUserTiming(int64_t sample) {
  return GetExponentialBucketMin(sample, 2.0);
}

int64_t GetExponentialBucketMinForCounts1000(int64_t sample) {
  return GetExponentialBucketMin(sample, 1.15);
}

int64_t GetExponentialBucketMinForBytes(int64_t sample) {
  return GetExponentialBucketMin(sample, 1.3);
}

int64_t GetExponentialBucketMinForBytesUnder1KB(int64_t sample) {
  return GetExponentialBucketMin(sample, 1.15);
}

int64_t GetLinearBucketMin(int64_t sample, int32_t bucket_size) {
  DCHECK(bucket_size > 0);
  // Round down to the nearest multiple of |bucket_size| (for negative samples,
  // this rounds away from zero).
  int64_t remainder = sample % bucket_size;
  if (remainder < 0)
    return sample - (remainder + bucket_size);
  return sample - remainder;
}

int64_t GetLinearBucketMin(double sample, int32_t bucket_size) {
  int64_t val = GetLinearBucketMin(
      base::saturated_cast<int64_t>(std::floor(sample)), bucket_size);
  // Ensure that |sample| can't get put into a bucket higher than itself.
  DCHECK(val <= sample);
  return val;
}

}  // namespace ukm
