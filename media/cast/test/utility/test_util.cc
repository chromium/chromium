// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <algorithm>

#include "base/strings/stringprintf.h"
#include "media/cast/test/utility/test_util.h"

namespace media {
namespace cast {
namespace test {

MeanAndError::MeanAndError(const std::vector<double>& values) {
  double sum = 0.0;
  double sqr_sum = 0.0;
  num_values = values.size();
  if (num_values) {
    for (size_t i = 0; i < num_values; i++) {
      sum += values[i];
      sqr_sum += values[i] * values[i];
    }
    mean = sum / num_values;
    std_dev =
        sqrt(std::max(0.0, num_values * sqr_sum - sum * sum)) / num_values;
  }
}

std::string MeanAndError::AsString() const {
  return base::StringPrintf("%f +/- %f", mean, std_dev);
}

}  // namespace test
}  // namespace cast
}  // namespace media
