// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/base/weighted_samples.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(WeightedSamplesTest, CalculateWeightedAverage) {
  static constexpr double kWeightFactor = 0.9;
  static constexpr double kExpected[] = {
      1,
      1.5263157894736843,
      2.0701107011070110,
      2.6312881651642916,
  };
  WeightedSamples samples(kWeightFactor);
  for (size_t i = 0; i < std::size(kExpected); i++) {
    samples.Record(i + 1);
    EXPECT_DOUBLE_EQ(kExpected[i], samples.WeightedAverage());
  }
}

TEST(WeightedSamplesTest, CalculateWeightedAverage_SameValues) {
  WeightedSamples samples(0.9);
  for (int i = 0; i < 100; i++) {
    samples.Record(100);
  }
  EXPECT_EQ(samples.WeightedAverage(), 100);
}

TEST(WeightedSamplesTest, ReturnZeroIfNoRecords) {
  WeightedSamples samples(0.9);
  EXPECT_EQ(samples.WeightedAverage(), 0);
}

}  // namespace remoting
