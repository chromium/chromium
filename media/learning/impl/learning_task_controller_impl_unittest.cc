// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/learning/impl/learning_task_controller_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "media/learning/impl/distribution_reporter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearningTaskControllerImplTest : public testing::Test {
 public:
  class FakeDistributionReporter : public DistributionReporter {
   public:
    FakeDistributionReporter(const LearningTask& task)
        : DistributionReporter(task) {}

    // protected => public
    const std::optional<std::set<int>>& feature_indices() const {
      return DistributionReporter::feature_indices();
    }

   protected:
    void OnPrediction(const PredictionInfo& info,
                      TargetHistogram predicted) override {
      num_reported_++;
      TargetHistogram dist;
      dist += info.observed;
      if (dist == predicted)
        num_correct_++;
      most_recent_source_id_ = info.source_id;
    }

   public:
    int num_reported_ = 0;
    int num_correct_ = 0;
    ukm::SourceId most_recent_source_id_;
  };

  // Model that always predicts a constant.
  class FakeModel : public Model {
   public:
    FakeModel(TargetValue target) : target_(target) {}

    // Model
    TargetHistogram PredictDistribution(
        const FeatureVector& features) override {
      TargetHistogram dist;
      dist += target_;
      return dist;
    }

   private:
    // The value we predict.
    TargetValue target_;
  };

  class FakeTrainer : public TrainingAlgorithm {
   public:
    // |num_models| is where we'll record how many models we've trained.
    // |target_value| is the prediction that our trained model will make.
    FakeTrainer(int* num_models, TargetValue target_value)
        : num_models_(num_models), target_value_(target_value) {}
    ~FakeTrainer() override {}

    void Train(const LearningTask& task,
               const TrainingData& training_data,
               TrainedModelCB model_cb) override {
      task_ = task;
      (*num_models_)++;
      training_data_ = training_data;
      std::move(model_cb).Run(std::make_unique<FakeModel>(target_value_));
    }

    const LearningTask& task() const { return task_; }

    const TrainingData& training_data() const { return training_data_; }

   private:
    LearningTask task_;
    raw_ptr<int> num_models_ = nullptr;
    TargetValue target_value_;

    // Most recently provided training data.
    TrainingData training_data_;
  };

  // Increments feature 0.
  class FakeFeatureProvider : public FeatureProvider {
   public:
    void AddFeatures(FeatureVector features, FeatureVectorCB cb) override {
      features[0] = FeatureValue(features[0].value() + 1);
      std::move(cb).Run(features);
    }
  };

  LearningTaskControllerImplTest()
      : predicted_target_(123), not_predicted_target_(456) {
    // Set the name so that we can check it later.
    task_.name = "TestTask";
    // Don't require too many training examples per report.
    task_.max_data_set_size = 20;
    task_.min_new_data_fraction = 0.1;
  }

  ~LearningTaskControllerImplTest() override {
    // To prevent a memory leak, reset the controller.  This may post
    // destruction of other objects, so RunUntilIdle().
    controller_.reset();
    task_environment_.RunUntilIdle();
  }

  void CreateController(SequenceBoundFeatureProvider feature_provider =
                            SequenceBoundFeatureProvider()) {
    std::unique_ptr<FakeDistributionReporter> reporter =
        std::make_unique<FakeDistributionReporter>(task_);
    reporter_raw_ = reporter.get();

    controller_ = std::make_unique<LearningTaskControllerImpl>(
        task_, std::move(reporter), std::move(feature_provider));

    auto fake_trainer =
        std::make_unique<FakeTrainer>(&num_models_, predicted_target_);
    trainer_raw_ = fake_trainer.get();
    controller_->SetTrainerForTesting(std::move(fake_trainer));
  }

  void AddExample(const LabelledExample& example,
                  std::optional<ukm::SourceId> source_id = std::nullopt) {
    base::UnguessableToken id = base::UnguessableToken::Create();
    controller_->BeginObservation(id, example.features, std::nullopt,
                                  source_id);
    controller_->CompleteObservation(
        id, ObservationCompletion(example.target_value, example.weight));
  }

  void VerifyPrediction(const FeatureVector& features,
                        std::optional<TargetHistogram> expectation) {
    std::optional<TargetHistogram> observed_prediction;
    controller_->PredictDistribution(
        features, base::BindOnce(
                      [](std::optional<TargetHistogram>* test_storage,
                         const std::optional<TargetHistogram>& predicted) {
                        *test_storage = predicted;
                      },
                      &observed_prediction));
    task_environment_.RunUntilIdle();
    EXPECT_EQ(observed_prediction, expectation);
  }

  base::test::TaskEnvironment task_environment_;

  // Number of models that we trained.
  int num_models_ = 0;

  // Two distinct targets.
  const TargetValue predicted_target_;
  const TargetValue not_predicted_target_;

  raw_ptr<FakeDistributionReporter, DanglingUntriaged> reporter_raw_ = nullptr;
  raw_ptr<FakeTrainer, DanglingUntriaged> trainer_raw_ = nullptr;

  LearningTask task_;
  std::unique_ptr<LearningTaskControllerImpl> controller_;
};

TEST_F(LearningTaskControllerImplTest, AddingExamplesTrainsModelAndReports) {
  CreateController();

  LabelledExample example;

  // Up to the first 1/training_fraction examples should train on each example.
  // Make each of the examples agree on |predicted_target_|.
  example.target_value = predicted_target_;
  int count = static_cast<int>(1.0 / task_.min_new_data_fraction);
  for (int i = 0; i < count; i++) {
    AddExample(example);
    EXPECT_EQ(num_models_, i + 1);
    // All examples except the first should be reported as correct.  For the
    // first, there's no model to test again.
    EXPECT_EQ(reporter_raw_->num_reported_, i);
    EXPECT_EQ(reporter_raw_->num_correct_, i);
  }
  // The next |count| should train every other one.
  for (int i = 0; i < count; i++) {
    AddExample(example);
    EXPECT_EQ(num_models_, count + (i + 1) / 2);
  }

  // The next |count| should be the same, since we've reached the max training
  // set size.
  for (int i = 0; i < count; i++) {
    AddExample(example);
    EXPECT_EQ(num_models_, count + count / 2 + (i + 1) / 2);
  }

  // We should have reported results for each except the first.  All of them
  // should be correct, since there's only one target so far.
  EXPECT_EQ(reporter_raw_->num_reported_, count * 3 - 1);
  EXPECT_EQ(reporter_raw_->num_correct_, count * 3 - 1);

  // Adding a value that doesn't match should report one more attempt, with an
  // incorrect prediction.
  example.target_value = not_predicted_target_;
  AddExample(example);
  EXPECT_EQ(reporter_raw_->num_reported_, count * 3);
  EXPECT_EQ(reporter_raw_->num_correct_, count * 3 - 1);  // Unchanged.
}

TEST_F(LearningTaskControllerImplTest, FeatureProviderIsUsed) {
  // If a FeatureProvider factory is provided, make sure that it's used to
  // adjust new examples.
  task_.feature_descriptions.push_back({"AddedByFeatureProvider"});
  SequenceBoundFeatureProvider feature_provider =
      base::SequenceBound<FakeFeatureProvider>(
          base::SequencedTaskRunner::GetCurrentDefault());
  CreateController(std::move(feature_provider));
  LabelledExample example;
  example.features.push_back(FeatureValue(123));
  example.weight = 321u;
  AddExample(example);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(trainer_raw_->training_data()[0].features[0], FeatureValue(124));
  EXPECT_EQ(trainer_raw_->training_data()[0].weight, example.weight);
}

TEST_F(LearningTaskControllerImplTest, FeatureSubsetsWork) {
  const char* feature_names[] = {
      "feature0", "feature1", "feature2", "feature3", "feature4",  "feature5",
      "feature6", "feature7", "feature8", "feature9", "feature10", "feature11",
  };
  const int num_features = sizeof(feature_names) / sizeof(feature_names[0]);
  for (int i = 0; i < num_features; i++)
    task_.feature_descriptions.push_back({feature_names[i]});
  const size_t subset_size = 4;
  task_.feature_subset_size = subset_size;
  CreateController();

  // Verify that the reporter is given a subset of the features.
  auto subset = *reporter_raw_->feature_indices();
  EXPECT_EQ(subset.size(), subset_size);

  // Train a model.  Each feature will have a unique value.
  LabelledExample example;
  for (int i = 0; i < num_features; i++)
    example.features.push_back(FeatureValue(i));
  AddExample(example);

  // Verify that all feature names in |subset| are present in the task.
  FeatureVector expected_features;
  expected_features.resize(subset_size);
  EXPECT_EQ(trainer_raw_->task().feature_descriptions.size(), subset_size);
  for (auto& iter : subset) {
    bool found = false;
    for (size_t i = 0; i < subset_size; i++) {
      if (trainer_raw_->task().feature_descriptions[i].name ==
          feature_names[iter]) {
        // Also build a vector with the features in the expected order.
        expected_features[i] = example.features[iter];
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }

  // Verify that the training data has the adjusted features.
  EXPECT_EQ(trainer_raw_->training_data().size(), 1u);
  EXPECT_EQ(trainer_raw_->training_data()[0].features, expected_features);
}

TEST_F(LearningTaskControllerImplTest, PredictDistribution) {
  CreateController();

  // Predictions should be std::nullopt until we have a model.
  LabelledExample example;
  VerifyPrediction(example.features, std::nullopt);

  AddExample(example);
  TargetHistogram expected_histogram;
  expected_histogram += predicted_target_;
  VerifyPrediction(example.features, expected_histogram);
}

TEST_F(LearningTaskControllerImplTest,
       SourceIdIsProvidedToDistributionReporter) {
  CreateController();
  LabelledExample example;
  ukm::SourceId source_id(123);
  // Add two examples, so that the second causes a prediction to be reported.
  AddExample(example, source_id);
  AddExample(example, source_id);
  EXPECT_EQ(reporter_raw_->most_recent_source_id_, source_id);
}

}  // namespace learning
}  // namespace media
