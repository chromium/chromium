// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_WEIGHTED_OBSERVATION_H_
#define NET_NQE_WEIGHTED_OBSERVATION_H_

#include "net/base/net_export.h"

namespace net::nqe::internal {

// Holds an observation and its weight.
struct NET_EXPORT_PRIVATE WeightedObservation {
  WeightedObservation(int32_t value, double weight)
      : value(value), weight(weight) {}

  WeightedObservation(const WeightedObservation& other)
      : WeightedObservation(other.value, other.weight) {}

  WeightedObservation& operator=(const WeightedObservation& other) = default;

  // Required for sorting the samples in the ascending order of values.
  bool operator<(const WeightedObservation& other) const {
    return (value < other.value);
  }

  // Value of the sample.
  int32_t value;

  // Weight of the sample. This is computed based on how much time has passed
  // since the sample was taken.
  double weight;
};

}  // namespace net::nqe::internal

#endif  // NET_NQE_WEIGHTED_OBSERVATION_H_
