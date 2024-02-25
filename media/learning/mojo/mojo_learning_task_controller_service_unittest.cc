// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/learning/mojo/mojo_learning_task_controller_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Meaningless, but non-empty, source id.
ukm::SourceId kSourceId{123};
}  // namespace

namespace media {
namespace learning {

class MojoLearningTaskControllerServiceTest : public ::testing::Test {
 public:
  class FakeLearningTaskController : public LearningTaskController {
   public:
    void BeginObservation(
        base::UnguessableToken id,
        const FeatureVector& features,
        const std::optional<TargetValue>& default_target,
        const std::optional<ukm::SourceId>& source_id) override {
      begin_args_.id_ = id;
      begin_args_.features_ = features;
      begin_args_.default_target_ = default_target;
      begin_args_.source_id_ = source_id;
    }

    void CompleteObservation(base::UnguessableToken id,
                             const ObservationCompletion& completion) override {
      complete_args_.id_ = id;
      complete_args_.completion_ = completion;
    }

    void CancelObservation(base::UnguessableToken id) override {
      cancel_args_.id_ = id;
    }

    void UpdateDefaultTarget(
        base::UnguessableToken id,
        const std::optional<TargetValue>& default_target) override {
      update_default_args_.id_ = id;
      update_default_args_.default_target_ = default_target;
    }

    const LearningTask& GetLearningTask() override {
      return LearningTask::Empty();
    }

    void PredictDistribution(const FeatureVector& features,
                             PredictionCB callback) override {
      predict_distribution_args_.features_ = features;
      predict_distribution_args_.callback_ = std::move(callback);
    }

    struct {
      base::UnguessableToken id_;
      FeatureVector features_;
      std::optional<TargetValue> default_target_;
      std::optional<ukm::SourceId> source_id_;
    } begin_args_;

    struct {
      base::UnguessableToken id_;
      ObservationCompletion completion_;
    } complete_args_;

    struct {
      base::UnguessableToken id_;
    } cancel_args_;

    struct {
      base::UnguessableToken id_;
      std::optional<TargetValue> default_target_;
    } update_default_args_;

    struct {
      FeatureVector features_;
      PredictionCB callback_;
    } predict_distribution_args_;
  };

 public:
  MojoLearningTaskControllerServiceTest() = default;
  ~MojoLearningTaskControllerServiceTest() override = default;

  void SetUp() override {
    std::unique_ptr<FakeLearningTaskController> controller =
        std::make_unique<FakeLearningTaskController>();
    controller_raw_ = controller.get();

    // Add two features.
    task_.feature_descriptions.push_back({});
    task_.feature_descriptions.push_back({});

    // Tell |learning_controller_| to forward to the fake learner impl.
    service_ = std::make_unique<MojoLearningTaskControllerService>(
        task_, kSourceId, std::move(controller));
  }

  LearningTask task_;

  // Mojo stuff.
  base::test::TaskEnvironment task_environment_;

  // The learner under test. Must outlive `controller_raw_`
  std::unique_ptr<MojoLearningTaskControllerService> service_;

  // Raw controller. Owned by `service_`.
  raw_ptr<FakeLearningTaskController> controller_raw_ = nullptr;
};

TEST_F(MojoLearningTaskControllerServiceTest, BeginComplete) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  service_->BeginObservation(id, features, std::nullopt);
  EXPECT_EQ(id, controller_raw_->begin_args_.id_);
  EXPECT_EQ(features, controller_raw_->begin_args_.features_);
  EXPECT_FALSE(controller_raw_->begin_args_.default_target_);
  EXPECT_TRUE(controller_raw_->begin_args_.source_id_);
  EXPECT_EQ(*controller_raw_->begin_args_.source_id_, kSourceId);

  ObservationCompletion completion(TargetValue(1234));
  service_->CompleteObservation(id, completion);

  EXPECT_EQ(id, controller_raw_->complete_args_.id_);
  EXPECT_EQ(completion.target_value,
            controller_raw_->complete_args_.completion_.target_value);
}

TEST_F(MojoLearningTaskControllerServiceTest, BeginCancel) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  service_->BeginObservation(id, features, std::nullopt);
  EXPECT_EQ(id, controller_raw_->begin_args_.id_);
  EXPECT_EQ(features, controller_raw_->begin_args_.features_);
  EXPECT_FALSE(controller_raw_->begin_args_.default_target_);

  service_->CancelObservation(id);

  EXPECT_EQ(id, controller_raw_->cancel_args_.id_);
}

TEST_F(MojoLearningTaskControllerServiceTest, BeginWithDefaultTarget) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  TargetValue default_target(987);
  service_->BeginObservation(id, features, default_target);
  EXPECT_EQ(id, controller_raw_->begin_args_.id_);
  EXPECT_EQ(features, controller_raw_->begin_args_.features_);
  EXPECT_EQ(default_target, controller_raw_->begin_args_.default_target_);
  EXPECT_TRUE(controller_raw_->begin_args_.source_id_);
  EXPECT_EQ(*controller_raw_->begin_args_.source_id_, kSourceId);
}

TEST_F(MojoLearningTaskControllerServiceTest, TooFewFeaturesIsIgnored) {
  // A FeatureVector with too few elements should be ignored.
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector short_features = {FeatureValue(123)};
  service_->BeginObservation(id, short_features, std::nullopt);
  EXPECT_NE(id, controller_raw_->begin_args_.id_);
  EXPECT_EQ(controller_raw_->begin_args_.features_.size(), 0u);
}

TEST_F(MojoLearningTaskControllerServiceTest, TooManyFeaturesIsIgnored) {
  // A FeatureVector with too many elements should be ignored.
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector long_features = {FeatureValue(123), FeatureValue(456),
                                 FeatureValue(789)};
  service_->BeginObservation(id, long_features, std::nullopt);
  EXPECT_NE(id, controller_raw_->begin_args_.id_);
  EXPECT_EQ(controller_raw_->begin_args_.features_.size(), 0u);
}

TEST_F(MojoLearningTaskControllerServiceTest, CompleteWithoutBeginFails) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  ObservationCompletion completion(TargetValue(1234));
  service_->CompleteObservation(id, completion);
  EXPECT_NE(id, controller_raw_->complete_args_.id_);
}

TEST_F(MojoLearningTaskControllerServiceTest, CancelWithoutBeginFails) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  service_->CancelObservation(id);
  EXPECT_NE(id, controller_raw_->cancel_args_.id_);
}

TEST_F(MojoLearningTaskControllerServiceTest, UpdateDefaultTargetToValue) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  service_->BeginObservation(id, features, std::nullopt);
  TargetValue default_target(987);
  service_->UpdateDefaultTarget(id, default_target);
  EXPECT_EQ(id, controller_raw_->update_default_args_.id_);
  EXPECT_EQ(default_target,
            controller_raw_->update_default_args_.default_target_);
}

TEST_F(MojoLearningTaskControllerServiceTest, UpdateDefaultTargetToNoValue) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  TargetValue default_target(987);
  service_->BeginObservation(id, features, default_target);
  service_->UpdateDefaultTarget(id, std::nullopt);
  EXPECT_EQ(id, controller_raw_->update_default_args_.id_);
  EXPECT_EQ(std::nullopt,
            controller_raw_->update_default_args_.default_target_);
}

TEST_F(MojoLearningTaskControllerServiceTest, PredictDistribution) {
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  TargetHistogram observed_prediction;
  service_->PredictDistribution(
      features, base::BindOnce(
                    [](TargetHistogram* test_storage,
                       const std::optional<TargetHistogram>& predicted) {
                      *test_storage = *predicted;
                    },
                    &observed_prediction));
  EXPECT_EQ(features, controller_raw_->predict_distribution_args_.features_);
  EXPECT_FALSE(controller_raw_->predict_distribution_args_.callback_.is_null());

  TargetHistogram expected_prediction;
  expected_prediction[TargetValue(1)] = 1.0;
  expected_prediction[TargetValue(2)] = 2.0;
  expected_prediction[TargetValue(3)] = 3.0;
  std::move(controller_raw_->predict_distribution_args_.callback_)
      .Run(expected_prediction);
  EXPECT_EQ(expected_prediction, observed_prediction);
}

}  // namespace learning
}  // namespace media
