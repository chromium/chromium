// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_METRICS_UTILS_H_
#define SERVICES_METRICS_PUBLIC_CPP_METRICS_UTILS_H_

#include <stdint.h>

#include "services/metrics/public/cpp/metrics_export.h"

namespace ukm {

// Calculates the exponential bucket |sample| falls in and returns the lower
// threshold of that bucket. |bucket_spacing| is the exponential spacing factor
// from one bucket to the next. Only returns a non-negative value.
int64_t METRICS_EXPORT GetExponentialBucketMin(int64_t sample,
                                               double bucket_spacing);

// Like GetExponentialBucketMin but uses a standard bucket_spacing of 2.0 for
// timing user actions.
int64_t METRICS_EXPORT GetExponentialBucketMinForUserTiming(int64_t sample);

// Like GetExponentialBucketMin but uses a standard bucket_spacing of 1.3 for
// timing user actions with higher resolution.
int64_t METRICS_EXPORT GetExponentialBucketMinForFineUserTiming(int64_t sample);

// Like GetExponentialBucketMin but uses a standard bucket_spacing of 1.15.
int64_t METRICS_EXPORT GetExponentialBucketMinForCounts1000(int64_t sample);

// Like GetExponentialBucketMin but uses a standard bucket_spacing of 1.3.
int64_t METRICS_EXPORT GetExponentialBucketMinForBytes(int64_t sample);

// Like GetExponentialBucketMin but uses a standard bucket_spacing of 1.15.
int64_t METRICS_EXPORT GetExponentialBucketMinForBytesUnder1KB(int64_t sample);

// Calculates the linear bucket |sample| falls in and returns the lower
// threshold of that bucket (i.e., rounding down to the nearest multiple of
// |bucket_size|). Negative sample values will be rounded down as well (away
// from zero). |bucket_size| is the size of each bucket, and must be a non-zero
// positive integer.
int64_t METRICS_EXPORT GetLinearBucketMin(int64_t sample, int32_t bucket_size);
int64_t METRICS_EXPORT GetLinearBucketMin(double sample, int32_t bucket_size);

// A specialized bucketing function for durations, based on the assumption that
// as durations get longer, specific timings matter less and less.
// Buckets at the 1 millisecond level up to 10 ms, at the 10ms level up
// to 100ms, at the 100ms up to five seconds, at the 1 second level up to twenty
// seconds, at the ten second level up to one minute, at the minute level up to
// 10 minutes, at the hour level up to 1 day. Once days are reached the sample
// will be bucketed exponentially by day.
// |sample| should be a positive value in milliseconds.
int64_t METRICS_EXPORT GetSemanticBucketMinForDurationTiming(int64_t sample);

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_METRICS_UTILS_H_
