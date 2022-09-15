// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_WEIGHTED_SAMPLES_H_
#define REMOTING_BASE_WEIGHTED_SAMPLES_H_

#include <cstdint>

namespace remoting {

// Aggregates the samples and gives each of them a weight based on its age. This
// class can help to smooth the input data.
class WeightedSamples final {
 public:
  explicit WeightedSamples(double weight_factor);
  ~WeightedSamples();

  void Record(double value);
  double WeightedAverage() const;

 private:
  const double weight_factor_;
  double weighted_sum_ = 0;
  double weight_ = 0;
};

}  // namespace remoting
#endif  // REMOTING_BASE_WEIGHTED_SAMPLES_H_
