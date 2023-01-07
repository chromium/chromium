// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/target_histogram.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class TargetHistogramTest : public testing::Test {
 public:
  TargetHistogramTest() : value_1(123), value_2(456), value_3(789) {}

  TargetHistogram histogram_;

  TargetValue value_1;
  const size_t counts_1 = 100;

  TargetValue value_2;
  const size_t counts_2 = 10;

  TargetValue value_3;
};

TEST_F(TargetHistogramTest, EmptyTargetHistogramHasZeroCounts) {
  EXPECT_EQ(histogram_.total_counts(), 0u);
}

TEST_F(TargetHistogramTest, AddingCountsWorks) {
  histogram_[value_1] = counts_1;
  EXPECT_EQ(histogram_.total_counts(), counts_1);
  EXPECT_EQ(histogram_[value_1], counts_1);
  histogram_[value_1] += counts_1;
  EXPECT_EQ(histogram_.total_counts(), counts_1 * 2u);
  EXPECT_EQ(histogram_[value_1], counts_1 * 2u);
}

TEST_F(TargetHistogramTest, MultipleValuesAreSeparate) {
  histogram_[value_1] = counts_1;
  histogram_[value_2] = counts_2;
  EXPECT_EQ(histogram_.total_counts(), counts_1 + counts_2);
  EXPECT_EQ(histogram_[value_1], counts_1);
  EXPECT_EQ(histogram_[value_2], counts_2);
}

TEST_F(TargetHistogramTest, AddingTargetValues) {
  histogram_ += value_1;
  EXPECT_EQ(histogram_.total_counts(), 1u);
  EXPECT_EQ(histogram_[value_1], 1u);
  EXPECT_EQ(histogram_[value_2], 0u);

  histogram_ += value_1;
  EXPECT_EQ(histogram_.total_counts(), 2u);
  EXPECT_EQ(histogram_[value_1], 2u);
  EXPECT_EQ(histogram_[value_2], 0u);

  histogram_ += value_2;
  EXPECT_EQ(histogram_.total_counts(), 3u);
  EXPECT_EQ(histogram_[value_1], 2u);
  EXPECT_EQ(histogram_[value_2], 1u);
}

TEST_F(TargetHistogramTest, AddingTargetHistograms) {
  histogram_[value_1] = counts_1;

  TargetHistogram rhs;
  rhs[value_2] = counts_2;

  histogram_ += rhs;

  EXPECT_EQ(histogram_.total_counts(), counts_1 + counts_2);
  EXPECT_EQ(histogram_[value_1], counts_1);
  EXPECT_EQ(histogram_[value_2], counts_2);
}

TEST_F(TargetHistogramTest, FindSingularMaxFindsTheSingularMax) {
  histogram_[value_1] = counts_1;
  histogram_[value_2] = counts_2;
  ASSERT_TRUE(counts_1 > counts_2);

  TargetValue max_value(0);
  double max_counts = 0;
  EXPECT_TRUE(histogram_.FindSingularMax(&max_value, &max_counts));
  EXPECT_EQ(max_value, value_1);
  EXPECT_EQ(max_counts, counts_1);
}

TEST_F(TargetHistogramTest, FindSingularMaxFindsTheSingularMaxAlternateOrder) {
  // Switch the order, to handle sorting in different directions.
  histogram_[value_1] = counts_2;
  histogram_[value_2] = counts_1;
  ASSERT_TRUE(counts_1 > counts_2);

  TargetValue max_value(0);
  double max_counts = 0;
  EXPECT_TRUE(histogram_.FindSingularMax(&max_value, &max_counts));
  EXPECT_EQ(max_value, value_2);
  EXPECT_EQ(max_counts, counts_1);
}

TEST_F(TargetHistogramTest, FindSingularMaxReturnsFalsForNonSingularMax) {
  histogram_[value_1] = counts_1;
  histogram_[value_2] = counts_1;

  TargetValue max_value(0);
  double max_counts = 0;
  EXPECT_FALSE(histogram_.FindSingularMax(&max_value, &max_counts));
}

TEST_F(TargetHistogramTest, FindSingularMaxIgnoresNonSingularNonMax) {
  histogram_[value_1] = counts_1;
  // |value_2| and |value_3| are tied, but not the max.
  histogram_[value_2] = counts_2;
  histogram_[value_3] = counts_2;
  ASSERT_TRUE(counts_1 > counts_2);

  TargetValue max_value(0);
  double max_counts = 0;
  EXPECT_TRUE(histogram_.FindSingularMax(&max_value, &max_counts));
  EXPECT_EQ(max_value, value_1);
  EXPECT_EQ(max_counts, counts_1);
}

TEST_F(TargetHistogramTest, FindSingularMaxDoesntRequireCounts) {
  histogram_[value_1] = counts_1;

  TargetValue max_value(0);
  EXPECT_TRUE(histogram_.FindSingularMax(&max_value));
  EXPECT_EQ(max_value, value_1);
}

TEST_F(TargetHistogramTest, EqualDistributionsCompareAsEqual) {
  histogram_[value_1] = counts_1;
  TargetHistogram histogram_2;
  histogram_2[value_1] = counts_1;

  EXPECT_TRUE(histogram_ == histogram_2);
}

TEST_F(TargetHistogramTest, UnequalDistributionsCompareAsNotEqual) {
  histogram_[value_1] = counts_1;
  TargetHistogram histogram_2;
  histogram_2[value_2] = counts_2;

  EXPECT_FALSE(histogram_ == histogram_2);
}

TEST_F(TargetHistogramTest, WeightedLabelledExamplesCountCorrectly) {
  LabelledExample example = {{}, value_1};
  example.weight = counts_1;
  histogram_ += example;

  TargetHistogram histogram_2;
  for (size_t i = 0; i < counts_1; i++)
    histogram_2 += value_1;

  EXPECT_EQ(histogram_, histogram_2);
}

TEST_F(TargetHistogramTest, Normalize) {
  histogram_[value_1] = counts_1;
  histogram_[value_2] = counts_2;
  histogram_.Normalize();
  EXPECT_EQ(histogram_[value_1],
            counts_1 / static_cast<double>(counts_1 + counts_2));
  EXPECT_EQ(histogram_[value_2],
            counts_2 / static_cast<double>(counts_1 + counts_2));
}

TEST_F(TargetHistogramTest, NormalizeEmptyDistribution) {
  // Normalizing an empty distribution should result in an empty distribution.
  histogram_.Normalize();
  EXPECT_EQ(histogram_.total_counts(), 0);
}

}  // namespace learning
}  // namespace media
