// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/metrics_utils.h"

#include <cmath>

#include "base/check.h"
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

int64_t GetExponentialBucketMinForFineUserTiming(int64_t sample) {
  return GetExponentialBucketMin(sample, 1.3);
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

int64_t METRICS_EXPORT GetSemanticBucketMinForDurationTiming(int64_t sample) {
  if (sample == 0)
    return 0;
  DCHECK(sample > 0);
  const int64_t kMillisecondsPerMinute = 60 * 1000;
  const int64_t kMillisecondsPerTenMinutes = 10 * kMillisecondsPerMinute;
  const int64_t kMillisecondsPerHour = 60 * kMillisecondsPerMinute;
  const int64_t kMillisecondsPerDay = 24 * kMillisecondsPerHour;
  int64_t modulus = 1;
  // If |sample| is a duration longer than a day, then use exponential bucketing
  // by number of days.
  // Algorithm is: convert ms to days, rounded down. Exponentially bucket.
  // Convert back to milliseconds, return sample.
  if (sample > kMillisecondsPerDay) {
    sample = sample / kMillisecondsPerDay;
    sample = GetExponentialBucketMinForUserTiming(sample);
    return sample * kMillisecondsPerDay;
  }

  if (sample > kMillisecondsPerHour) {
    modulus = kMillisecondsPerHour;
  } else if (sample > kMillisecondsPerTenMinutes) {
    modulus = kMillisecondsPerTenMinutes;
  } else if (sample > kMillisecondsPerMinute) {
    modulus = kMillisecondsPerMinute;
  } else if (sample > 20000) {  // Above 20s, 10s granularity
    modulus = 10000;
  } else if (sample > 5000) {  // Above 5s, 1s granularity
    modulus = 1000;
  } else if (sample > 100) {  // Above 100ms, 100ms granularity
    modulus = 100;
  } else if (sample > 10) {  // Above 10ms, 10ms granularity
    modulus = 10;
  }
  return sample - (sample % modulus);
}

}  // namespace ukm
