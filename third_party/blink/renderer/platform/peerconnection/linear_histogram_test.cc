// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/linear_histogram.h"

#include <vector>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

constexpr float kMinValue = 0.0f;
constexpr float kMaxValue = 10.0f;
constexpr wtf_size_t kNumBuckets = 10;

class LinearHistogramTest : public ::testing::Test {
 protected:
  LinearHistogramTest() : histogram_(kMinValue, kMaxValue, kNumBuckets) {}
  LinearHistogram histogram_;
};

TEST_F(LinearHistogramTest, NumValues) {
  EXPECT_EQ(0ul, histogram_.NumValues());
  histogram_.Add(0.0);
  EXPECT_EQ(1ul, histogram_.NumValues());
  histogram_.Add(5.0);
  EXPECT_EQ(2ul, histogram_.NumValues());
}

TEST_F(LinearHistogramTest, ReturnsCorrectPercentiles) {
  const std::vector<float> kTestValues = {
      -1.0f, 0.0f,  1.0f,  2.9f,  3.1f,  4.1f,  5.0f,  8.0f,  9.0f,  9.9f,
      10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f};
  // Pairs of {fraction, percentile value} computed by hand
  // for `kTestValues`.
  const std::vector<std::pair<float, float>> kTestPercentiles = {
      {0.01f, 0.0f},  {0.05f, 0.0f},  {0.1f, 0.0f},   {0.11f, 1.0f},
      {0.15f, 1.0f},  {0.20f, 3.0f},  {0.25f, 4.0f},  {0.30f, 5.0f},
      {0.35f, 5.0f},  {0.40f, 8.0f},  {0.41f, 9.0f},  {0.45f, 9.0f},
      {0.50f, 10.0f}, {0.55f, 10.0f}, {0.56f, 19.0f}, {0.80f, 19.0f},
      {0.95f, 19.0f}, {0.99f, 19.0f}, {1.0f, 19.0f}};
  for (float value : kTestValues) {
    histogram_.Add(value);
  }
  for (const auto& test_percentile : kTestPercentiles) {
    EXPECT_EQ(test_percentile.second,
              histogram_.GetPercentile(test_percentile.first));
  }
}

TEST_F(LinearHistogramTest, UnderflowReturnsHistogramMinValue) {
  histogram_.Add(-10.0);
  histogram_.Add(-5.0);
  histogram_.Add(-1.0);

  EXPECT_EQ(kMinValue, histogram_.GetPercentile(0.1));
  EXPECT_EQ(kMinValue, histogram_.GetPercentile(0.5));
  EXPECT_EQ(kMinValue, histogram_.GetPercentile(1.0));
}

TEST_F(LinearHistogramTest, OverflowReturnsMaximumObservedValue) {
  histogram_.Add(10.1);
  histogram_.Add(15.0);
  constexpr float kMaximumObservedValue = 20.0f;
  histogram_.Add(kMaximumObservedValue);

  EXPECT_EQ(kMaximumObservedValue, histogram_.GetPercentile(0.1));
  EXPECT_EQ(kMaximumObservedValue, histogram_.GetPercentile(0.5));
  EXPECT_EQ(kMaximumObservedValue, histogram_.GetPercentile(1.0));
}

}  // namespace
}  // namespace blink
