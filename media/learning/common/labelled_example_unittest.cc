// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/labelled_example.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearnerLabelledExampleTest : public testing::Test {};

TEST_F(LearnerLabelledExampleTest, InitListWorks) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  std::vector<FeatureValue> features = {FeatureValue(kFeature1),
                                        FeatureValue(kFeature2)};
  TargetValue target(789);
  LabelledExample example({FeatureValue(kFeature1), FeatureValue(kFeature2)},
                          target);

  EXPECT_EQ(example.features, features);
  EXPECT_EQ(example.target_value, target);
}

TEST_F(LearnerLabelledExampleTest, CopyConstructionWorks) {
  LabelledExample example_1({FeatureValue(123), FeatureValue(456)},
                            TargetValue(789));
  LabelledExample example_2(example_1);

  EXPECT_EQ(example_1, example_2);
}

TEST_F(LearnerLabelledExampleTest, MoveConstructionWorks) {
  LabelledExample example_1({FeatureValue(123), FeatureValue(456)},
                            TargetValue(789));

  LabelledExample example_1_copy(example_1);
  LabelledExample example_1_move(std::move(example_1));

  EXPECT_EQ(example_1_copy, example_1_move);
  EXPECT_NE(example_1_copy, example_1);
}

TEST_F(LearnerLabelledExampleTest, EqualExamplesCompareAsEqual) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  TargetValue target(789);
  LabelledExample example_1({FeatureValue(kFeature1), FeatureValue(kFeature2)},
                            target);
  LabelledExample example_2({FeatureValue(kFeature1), FeatureValue(kFeature2)},
                            target);
  // Verify both that == and != work.
  EXPECT_EQ(example_1, example_2);
  EXPECT_FALSE(example_1 != example_2);
  // Also insist that equal examples are not less.
  EXPECT_FALSE(example_1 < example_2);
  EXPECT_FALSE(example_2 < example_1);
}

TEST_F(LearnerLabelledExampleTest, UnequalFeaturesCompareAsUnequal) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  TargetValue target(789);
  LabelledExample example_1({FeatureValue(kFeature1), FeatureValue(kFeature1)},
                            target);
  LabelledExample example_2({FeatureValue(kFeature2), FeatureValue(kFeature2)},
                            target);
  EXPECT_TRUE(example_1 != example_2);
  EXPECT_FALSE(example_1 == example_2);
  // We don't care which way is <, but we do care that one is less than the
  // other but not both.
  EXPECT_NE((example_1 < example_2), (example_2 < example_1));
}

TEST_F(LearnerLabelledExampleTest, WeightDoesntChangeExampleEquality) {
  const int kFeature1 = 123;
  TargetValue target(789);
  LabelledExample example_1({FeatureValue(kFeature1)}, target);
  LabelledExample example_2 = example_1;

  // Set the weights to be unequal.  This should not affect the comparison.
  example_1.weight = 10u;
  example_2.weight = 20u;

  // Verify both that == and != ignore weights.
  EXPECT_EQ(example_1, example_2);
  EXPECT_FALSE(example_1 != example_2);
  // Also insist that equal examples are not less.
  EXPECT_FALSE(example_1 < example_2);
  EXPECT_FALSE(example_2 < example_1);
}

TEST_F(LearnerLabelledExampleTest, ExampleAssignmentCopiesWeights) {
  // While comparisons ignore weights, copy / assign should not.
  const int kFeature1 = 123;
  TargetValue target(789);
  LabelledExample example_1({FeatureValue(kFeature1)}, target);
  example_1.weight = 10u;

  // Copy-assignment.
  LabelledExample example_2;
  example_2 = example_1;
  EXPECT_EQ(example_1, example_2);
  EXPECT_EQ(example_1.weight, example_2.weight);

  // Copy-construction.
  LabelledExample example_3(example_1);
  EXPECT_EQ(example_1, example_3);
  EXPECT_EQ(example_1.weight, example_3.weight);

  // Move-assignment.
  LabelledExample example_4;
  example_4 = std::move(example_2);
  EXPECT_EQ(example_1, example_4);
  EXPECT_EQ(example_1.weight, example_4.weight);

  // Move-construction.
  LabelledExample example_5(std::move(example_3));
  EXPECT_EQ(example_1, example_5);
  EXPECT_EQ(example_1.weight, example_5.weight);
}

TEST_F(LearnerLabelledExampleTest, UnequalTargetsCompareAsUnequal) {
  const int kFeature1 = 123;
  const int kFeature2 = 456;
  LabelledExample example_1({FeatureValue(kFeature1), FeatureValue(kFeature1)},
                            TargetValue(789));
  LabelledExample example_2({FeatureValue(kFeature2), FeatureValue(kFeature2)},
                            TargetValue(987));
  EXPECT_TRUE(example_1 != example_2);
  EXPECT_FALSE(example_1 == example_2);
  // Exactly one should be less than the other, but we don't care which one.
  EXPECT_TRUE((example_1 < example_2) ^ (example_2 < example_1));
}

TEST_F(LearnerLabelledExampleTest, OrderingIsTransitive) {
  // Verify that ordering is transitive.  We don't particularly care what the
  // ordering is, otherwise.

  const FeatureValue kFeature1(123);
  const FeatureValue kFeature2(456);
  const FeatureValue kTarget1(789);
  const FeatureValue kTarget2(987);
  std::vector<LabelledExample> examples;
  examples.push_back(LabelledExample({kFeature1}, kTarget1));
  examples.push_back(LabelledExample({kFeature1}, kTarget2));
  examples.push_back(LabelledExample({kFeature2}, kTarget1));
  examples.push_back(LabelledExample({kFeature2}, kTarget2));
  examples.push_back(LabelledExample({kFeature1, kFeature2}, kTarget1));
  examples.push_back(LabelledExample({kFeature1, kFeature2}, kTarget2));
  examples.push_back(LabelledExample({kFeature2, kFeature1}, kTarget1));
  examples.push_back(LabelledExample({kFeature2, kFeature1}, kTarget2));

  // Sort, and make sure that it ends up totally ordered.
  std::sort(examples.begin(), examples.end());
  for (auto outer = examples.begin(); outer != examples.end(); outer++) {
    for (auto inner = outer + 1; inner != examples.end(); inner++) {
      EXPECT_TRUE(*outer < *inner);
      EXPECT_FALSE(*inner < *outer);
    }
  }
}

TEST_F(LearnerLabelledExampleTest, UnweightedTrainingDataPushBack) {
  // Test that pushing examples from unweighted storage into TrainingData works.
  TrainingData training_data;
  EXPECT_EQ(training_data.total_weight(), 0u);
  EXPECT_TRUE(training_data.empty());

  LabelledExample example({FeatureValue(123)}, TargetValue(789));
  training_data.push_back(example);
  EXPECT_EQ(training_data.total_weight(), 1u);
  EXPECT_FALSE(training_data.empty());
  EXPECT_TRUE(training_data.is_unweighted());
  EXPECT_EQ(training_data[0], example);
}

TEST_F(LearnerLabelledExampleTest, WeightedTrainingDataPushBack) {
  // Test that pushing examples from weighted storage into TrainingData works.
  TrainingData training_data;
  EXPECT_EQ(training_data.total_weight(), 0u);
  EXPECT_TRUE(training_data.empty());

  LabelledExample example({FeatureValue(123)}, TargetValue(789));
  const WeightType weight(10);
  example.weight = weight;
  training_data.push_back(example);
  training_data.push_back(example);

  EXPECT_EQ(training_data.total_weight(), weight * 2);
  EXPECT_FALSE(training_data.empty());
  EXPECT_FALSE(training_data.is_unweighted());
  EXPECT_EQ(training_data[0], example);
}

TEST_F(LearnerLabelledExampleTest, TrainingDataDeDuplicate) {
  // Make sure that TrainingData::DeDuplicate works properly.

  const WeightType weight_0_a(100);
  const WeightType weight_0_b(200);
  const WeightType weight_1(500);
  LabelledExample example_0({FeatureValue(123)}, TargetValue(789));
  LabelledExample example_1({FeatureValue(456)}, TargetValue(789));

  TrainingData training_data;
  example_0.weight = weight_0_a;
  training_data.push_back(example_0);
  example_1.weight = weight_1;
  training_data.push_back(example_1);
  example_0.weight = weight_0_b;
  training_data.push_back(example_0);

  EXPECT_EQ(training_data.total_weight(), weight_0_a + weight_0_b + weight_1);
  EXPECT_EQ(training_data.size(), 3u);
  EXPECT_EQ(training_data[0].weight, weight_0_a);
  EXPECT_EQ(training_data[1].weight, weight_1);
  EXPECT_EQ(training_data[2].weight, weight_0_b);

  TrainingData dedup = training_data.DeDuplicate();
  EXPECT_EQ(dedup.total_weight(), weight_0_a + weight_0_b + weight_1);
  EXPECT_EQ(dedup.size(), 2u);
  // We don't care which order they're in, so find the index of |example_0|.
  size_t idx_0 = (dedup[0] == example_0) ? 0 : 1;
  EXPECT_EQ(dedup[idx_0], example_0);
  EXPECT_EQ(dedup[idx_0].weight, weight_0_a + weight_0_b);
  EXPECT_EQ(dedup[1u - idx_0], example_1);
  EXPECT_EQ(dedup[1u - idx_0].weight, weight_1);
}

}  // namespace learning
}  // namespace media
