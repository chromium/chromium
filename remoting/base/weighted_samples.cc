// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/weighted_samples.h"

namespace remoting {

WeightedSamples::WeightedSamples(double weight_factor)
    : weight_factor_(weight_factor) {}
WeightedSamples::~WeightedSamples() = default;

void WeightedSamples::Record(double value) {
  weighted_sum_ *= weight_factor_;
  weighted_sum_ += value;
  weight_ *= weight_factor_;
  weight_++;
}

double WeightedSamples::WeightedAverage() const {
  if (weight_ == 0) {
    return 0;
  }
  return weighted_sum_ / weight_;
}

}  // namespace remoting
