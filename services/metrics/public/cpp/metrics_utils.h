// Copyright 2017 The Chromium Authors. All rights reserved.
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

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_METRICS_UTILS_H_
