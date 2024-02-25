// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/exponential_moving_average.h"

#include <cmath>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"

namespace media {

ExponentialMovingAverage::ExponentialMovingAverage(
    base::TimeDelta max_window_size)
    : max_window_size_(max_window_size) {}

ExponentialMovingAverage::~ExponentialMovingAverage() = default;

float ExponentialMovingAverage::GetStdDeviation() const {
  return std::sqrt(std::max(mean_square_ - std::pow(mean_, 2.0f), 0.0f));
}

}  // namespace media
