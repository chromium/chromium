// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/sliding_average.h"

namespace device {

SlidingAverage::SlidingAverage(size_t window_size) : values_(window_size) {}

SlidingAverage::~SlidingAverage() = default;

void SlidingAverage::AddSample(int64_t value) {
  values_.AddSample(value);
}

int64_t SlidingAverage::GetAverageOrDefault(int64_t default_value) const {
  if (values_.GetCount() == 0)
    return default_value;
  return values_.GetSum() / values_.GetCount();
}

SlidingTimeDeltaAverage::SlidingTimeDeltaAverage(size_t window_size)
    : sample_microseconds_(window_size) {}

SlidingTimeDeltaAverage::~SlidingTimeDeltaAverage() = default;

void SlidingTimeDeltaAverage::AddSample(base::TimeDelta value) {
  sample_microseconds_.AddSample(value.InMicroseconds());
}

base::TimeDelta SlidingTimeDeltaAverage::GetAverageOrDefault(
    base::TimeDelta default_value) const {
  return base::Microseconds(
      sample_microseconds_.GetAverageOrDefault(default_value.InMicroseconds()));
}

}  // namespace device
