// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/impl/lookup_table_trainer.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LookupTableTrainerTest : public testing::Test {
 public:
  std::unique_ptr<Model> Train(const LearningTask& task,
                               const TrainingData& data) {
    std::unique_ptr<Model> model;
    trainer_.Train(
        task_, data,
        base::BindOnce(
            [](std::unique_ptr<Model>* model_out,
               std::unique_ptr<Model> model) { *model_out = std::move(model); },
            &model));
    task_environment_.RunUntilIdle();
    return model;
  }

  base::test::TaskEnvironment task_environment_;

  LookupTableTrainer trainer_;
  LearningTask task_;
};

TEST_F(LookupTableTrainerTest, EmptyTrainingDataWorks) {
  TrainingData empty;
  std::unique_ptr<Model> model = Train(task_, empty);
  EXPECT_NE(model.get(), nullptr);
  EXPECT_EQ(model->PredictDistribution(FeatureVector()), TargetHistogram());
}

TEST_F(LookupTableTrainerTest, UniformTrainingDataWorks) {
  LabelledExample example({FeatureValue(123), FeatureValue(456)},
                          TargetValue(789));
  TrainingData training_data;
  const size_t n_examples = 10;
  for (size_t i = 0; i < n_examples; i++)
    training_data.push_back(example);
  std::unique_ptr<Model> model = Train(task_, training_data);

  // The tree should produce a distribution for one value (our target), which
  // has |n_examples| counts.
  TargetHistogram distribution = model->PredictDistribution(example.features);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example.target_value], n_examples);
}

TEST_F(LookupTableTrainerTest, SimpleSeparableTrainingData) {
  LabelledExample example_1({FeatureValue(123)}, TargetValue(1));
  LabelledExample example_2({FeatureValue(456)}, TargetValue(2));
  TrainingData training_data;
  training_data.push_back(example_1);
  training_data.push_back(example_2);
  std::unique_ptr<Model> model = Train(task_, training_data);

  // Each value should have a distribution with one target value with one count.
  TargetHistogram distribution = model->PredictDistribution(example_1.features);
  EXPECT_NE(model.get(), nullptr);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example_1.target_value], 1u);

  distribution = model->PredictDistribution(example_2.features);
  EXPECT_EQ(distribution.size(), 1u);
  EXPECT_EQ(distribution[example_2.target_value], 1u);
}

TEST_F(LookupTableTrainerTest, ComplexSeparableTrainingData) {
  // Build a four-feature training set that's completely separable, but one
  // needs all four features to do it.
  TrainingData training_data;
  for (int f1 = 0; f1 < 2; f1++) {
    for (int f2 = 0; f2 < 2; f2++) {
      for (int f3 = 0; f3 < 2; f3++) {
        for (int f4 = 0; f4 < 2; f4++) {
          // Add two copies of each example.
          training_data.push_back(
              LabelledExample({FeatureValue(f1), FeatureValue(f2),
                               FeatureValue(f3), FeatureValue(f4)},
                              TargetValue(f1 * 1 + f2 * 2 + f3 * 4 + f4 * 8)));
          training_data.push_back(
              LabelledExample({FeatureValue(f1), FeatureValue(f2),
                               FeatureValue(f3), FeatureValue(f4)},
                              TargetValue(f1 * 1 + f2 * 2 + f3 * 4 + f4 * 8)));
        }
      }
    }
  }

  std::unique_ptr<Model> model = Train(task_, training_data);
  EXPECT_NE(model.get(), nullptr);

  // Each example should have a distribution that selects the right value.
  for (const auto& example : training_data) {
    TargetHistogram distribution = model->PredictDistribution(example.features);
    TargetValue singular_max;
    EXPECT_TRUE(distribution.FindSingularMax(&singular_max));
    EXPECT_EQ(singular_max, example.target_value);
  }
}

TEST_F(LookupTableTrainerTest, UnseparableTrainingData) {
  LabelledExample example_1({FeatureValue(123)}, TargetValue(1));
  LabelledExample example_2({FeatureValue(123)}, TargetValue(2));
  TrainingData training_data;
  training_data.push_back(example_1);
  training_data.push_back(example_2);
  std::unique_ptr<Model> model = Train(task_, training_data);
  EXPECT_NE(model.get(), nullptr);

  // Each value should have a distribution with two targets with one count each.
  TargetHistogram distribution = model->PredictDistribution(example_1.features);
  EXPECT_EQ(distribution.size(), 2u);
  EXPECT_EQ(distribution[example_1.target_value], 1u);
  EXPECT_EQ(distribution[example_2.target_value], 1u);

  distribution = model->PredictDistribution(example_2.features);
  EXPECT_EQ(distribution.size(), 2u);
  EXPECT_EQ(distribution[example_1.target_value], 1u);
  EXPECT_EQ(distribution[example_2.target_value], 1u);
}

TEST_F(LookupTableTrainerTest, UnknownFeatureValueHandling) {
  // Verify how a previously unseen feature value is handled.
  LabelledExample example_1({FeatureValue(123)}, TargetValue(1));
  LabelledExample example_2({FeatureValue(456)}, TargetValue(2));
  TrainingData training_data;
  training_data.push_back(example_1);
  training_data.push_back(example_2);

  std::unique_ptr<Model> model = Train(task_, training_data);
  TargetHistogram distribution =
      model->PredictDistribution(FeatureVector({FeatureValue(789)}));
  // OOV data should return an empty distribution (nominal).
  EXPECT_EQ(distribution.size(), 0u);
}

TEST_F(LookupTableTrainerTest, RegressionWithWeightedExamplesWorks) {
  // Verify that regression results are sane.
  LabelledExample example_1({FeatureValue(123)}, TargetValue(1));
  example_1.weight = 50;
  LabelledExample example_2({FeatureValue(123)}, TargetValue(2));
  example_2.weight = 200;
  TrainingData training_data;
  training_data.push_back(example_1);
  training_data.push_back(example_2);

  std::unique_ptr<Model> model = Train(task_, training_data);
  TargetHistogram distribution =
      model->PredictDistribution(FeatureVector({FeatureValue(123)}));
  double avg = distribution.Average();
  const double expected =
      static_cast<double>(
          ((example_1.target_value.value() * example_1.weight) +
           (example_2.target_value.value() * example_2.weight))) /
      (example_1.weight + example_2.weight);
  EXPECT_GT(avg, expected * 0.99);
  EXPECT_LT(avg, expected * 1.01);
}

}  // namespace learning
}  // namespace media
