// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tpcd_heuristics/opener_heuristic_metrics.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "base/metrics/histogram.h"

namespace content {

int32_t Bucketize3PCDHeuristicSample(int64_t sample, int64_t maximum) {
  static constexpr size_t kBucketCount = 50;

  // Clamp the sample between 0 and maximum, and to the max int32 value (only
  // int32 is supported by histograms).
  if (sample <= 0) {
    return 0;
  }
  if (sample > std::min(maximum, static_cast<int64_t>(INT32_MAX))) {
    return std::min(maximum, static_cast<int64_t>(INT32_MAX));
  }

  // This bucketing implementation is based heavily on
  // base::Histogram::InitializeBucketRanges, but without allocating extra
  // memory.
  base::Histogram::Sample32 current = 1;
  double log_current = 0;
  double log_max = log(static_cast<double>(maximum));
  // Iterate over buckets and return the one closest to the sample.
  // Two of the buckets are 0 and `maximum`. Loop over the remaining buckets.
  static constexpr size_t kCutoffCount = kBucketCount - 2;
  for (size_t cutoff_index = 0; cutoff_index < kCutoffCount; ++cutoff_index) {
    // Increment the log of the bucket proportional to the current log over the
    // number of remaining buckets.
    double log_next =
        log_current + (log_max - log_current) / (kCutoffCount - cutoff_index);
    base::Histogram::Sample32 next = static_cast<int>(std::round(exp(log_next)));

    // If the difference between the buckets is too close, just add 1 to the
    // previous bucket.
    if (next <= current) {
      next = current + 1;
    }

    // Check if the sample falls in the bucket, and return the lower bound if
    // it does.
    if (sample < next) {
      return current;
    }

    // Increment the current values to the next values.
    current = next;
    log_current = log_next;
  }
  return maximum;
}

}  // namespace content
