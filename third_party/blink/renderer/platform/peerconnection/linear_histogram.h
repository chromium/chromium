// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_LINEAR_HISTOGRAM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_LINEAR_HISTOGRAM_H_

#include <cstddef>
#include <vector>

#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT LinearHistogram {
 public:
  // A linear histogram between min_value (exclusive) and max_value (inclusive).
  // The resolution/width of each bucket is (max_value - min_value) /
  // number_of_buckets. In addition to the specified number of buckets, there
  // will be two more buckets to track under- and overflow.
  LinearHistogram(float min_value, float max_value, size_t number_of_buckets);

  // Add a value to the histogram.
  void Add(float value);

  // Calculates and returns the specified percentile, which corresponds to the
  // lowest value X so that P(X) >= probability. This function must not be
  // called if no values have been added. The maximum observed value is returned
  // if the percentile is greater than max_value. The specified probability must
  // be greater than 0.
  float GetPercentile(float probability) const;

  // How many values that make up this histogram.
  size_t NumValues() const;

 private:
  const float min_value_;
  const float resolution_;
  std::vector<size_t> buckets_;
  size_t count_;
  float max_observed_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_LINEAR_HISTOGRAM_H_
