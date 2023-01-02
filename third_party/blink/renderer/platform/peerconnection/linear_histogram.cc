// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/linear_histogram.h"

#include <cmath>

#include "base/check.h"
#include "base/check_op.h"

namespace blink {

LinearHistogram::LinearHistogram(float min_value,
                                 float max_value,
                                 wtf_size_t number_of_buckets)
    : min_value_(min_value),
      resolution_((max_value - min_value) / number_of_buckets),
      buckets_(number_of_buckets + 2) {
  DCHECK_GT(number_of_buckets, 0u);
  DCHECK_GT(max_value, min_value);
}

void LinearHistogram::Add(float value) {
  wtf_size_t ix = 0;
  if (value > min_value_) {
    ix = std::ceil((value - min_value_) / resolution_);
    ix = std::min(ix, buckets_.size() - 1);
  }

  DCHECK_GE(ix, 0u);
  DCHECK_LT(ix, buckets_.size());

  ++buckets_[ix];
  ++count_;
  if (value > max_observed_value_) {
    max_observed_value_ = value;
  }
}

float LinearHistogram::GetPercentile(float probability) const {
  DCHECK_GT(probability, 0.f);
  DCHECK_LE(probability, 1.f);
  DCHECK_GT(count_, 0ul);

  wtf_size_t bucket = 0;
  float accumulated_probability = 0;
  while (accumulated_probability < probability && bucket < buckets_.size()) {
    accumulated_probability += static_cast<float>(buckets_[bucket]) / count_;
    ++bucket;
  }

  if (bucket < buckets_.size()) {
    return min_value_ + (bucket - 1) * resolution_;
  } else {
    // Return the maximum observed value if we end up in the overflow bucket.
    return max_observed_value_;
  }
}

wtf_size_t LinearHistogram::NumValues() const {
  return count_;
}

}  // namespace blink
