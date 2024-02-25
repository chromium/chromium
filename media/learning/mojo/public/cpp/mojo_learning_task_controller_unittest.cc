// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/learning/mojo/public/cpp/mojo_learning_task_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class MojoLearningTaskControllerTest : public ::testing::Test {
 public:
  // Impl of a mojom::LearningTaskController that remembers call arguments.
  class FakeMojoLearningTaskController : public mojom::LearningTaskController {
   public:
    void BeginObservation(
        const base::UnguessableToken& id,
        const FeatureVector& features,
        const std::optional<TargetValue>& default_target) override {
      begin_args_.id_ = id;
      begin_args_.features_ = features;
      begin_args_.default_target_ = default_target;
    }

    void CompleteObservation(const base::UnguessableToken& id,
                             const ObservationCompletion& completion) override {
      complete_args_.id_ = id;
      complete_args_.completion_ = completion;
    }

    void CancelObservation(const base::UnguessableToken& id) override {
      cancel_args_.id_ = id;
    }

    void UpdateDefaultTarget(
        const base::UnguessableToken& id,
        const std::optional<TargetValue>& default_target) override {
      update_default_args_.id_ = id;
      update_default_args_.default_target_ = default_target;
    }

    void PredictDistribution(const FeatureVector& features,
                             PredictDistributionCallback callback) override {
      predict_args_.features_ = features;
      predict_args_.callback_ = std::move(callback);
    }

    struct {
      base::UnguessableToken id_;
      FeatureVector features_;
      std::optional<TargetValue> default_target_;
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
      PredictDistributionCallback callback_;
    } predict_args_;
  };

 public:
  MojoLearningTaskControllerTest()
      : learning_controller_receiver_(&fake_learning_controller_) {}
  ~MojoLearningTaskControllerTest() override = default;

  void SetUp() override {
    // Create a LearningTask.
    task_.name = "MyLearningTask";

    // Tell |learning_controller_| to forward to the fake learner impl.
    mojo::Remote<media::learning::mojom::LearningTaskController> remote(
        learning_controller_receiver_.BindNewPipeAndPassRemote());
    learning_controller_ =
        std::make_unique<MojoLearningTaskController>(task_, std::move(remote));
  }

  // Mojo stuff.
  base::test::TaskEnvironment task_environment_;

  LearningTask task_;
  FakeMojoLearningTaskController fake_learning_controller_;
  mojo::Receiver<mojom::LearningTaskController> learning_controller_receiver_;

  // The learner under test.
  std::unique_ptr<MojoLearningTaskController> learning_controller_;
};

TEST_F(MojoLearningTaskControllerTest, GetLearningTask) {
  EXPECT_EQ(learning_controller_->GetLearningTask().name, task_.name);
}

TEST_F(MojoLearningTaskControllerTest, BeginWithoutDefaultTarget) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  learning_controller_->BeginObservation(id, features, std::nullopt,
                                         std::nullopt);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(id, fake_learning_controller_.begin_args_.id_);
  EXPECT_EQ(features, fake_learning_controller_.begin_args_.features_);
  EXPECT_FALSE(fake_learning_controller_.begin_args_.default_target_);
}

TEST_F(MojoLearningTaskControllerTest, BeginWithDefaultTarget) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  TargetValue default_target(987);
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  learning_controller_->BeginObservation(id, features, default_target,
                                         std::nullopt);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(id, fake_learning_controller_.begin_args_.id_);
  EXPECT_EQ(features, fake_learning_controller_.begin_args_.features_);
  EXPECT_EQ(default_target,
            fake_learning_controller_.begin_args_.default_target_);
}

TEST_F(MojoLearningTaskControllerTest, UpdateDefaultTargetToValue) {
  // Test if we can update the default target to a non-nullopt.
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  learning_controller_->BeginObservation(id, features, std::nullopt,
                                         std::nullopt);
  TargetValue default_target(987);
  learning_controller_->UpdateDefaultTarget(id, default_target);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(id, fake_learning_controller_.update_default_args_.id_);
  EXPECT_EQ(features, fake_learning_controller_.begin_args_.features_);
  EXPECT_EQ(default_target,
            fake_learning_controller_.update_default_args_.default_target_);
}

TEST_F(MojoLearningTaskControllerTest, UpdateDefaultTargetToNoValue) {
  // Test if we can update the default target to nullopt.
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  TargetValue default_target(987);
  learning_controller_->BeginObservation(id, features, default_target,
                                         std::nullopt);
  learning_controller_->UpdateDefaultTarget(id, std::nullopt);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(id, fake_learning_controller_.update_default_args_.id_);
  EXPECT_EQ(features, fake_learning_controller_.begin_args_.features_);
  EXPECT_EQ(std::nullopt,
            fake_learning_controller_.update_default_args_.default_target_);
}

TEST_F(MojoLearningTaskControllerTest, Complete) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  ObservationCompletion completion(TargetValue(1234));
  learning_controller_->CompleteObservation(id, completion);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(id, fake_learning_controller_.complete_args_.id_);
  EXPECT_EQ(completion.target_value,
            fake_learning_controller_.complete_args_.completion_.target_value);
}

TEST_F(MojoLearningTaskControllerTest, Cancel) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  learning_controller_->CancelObservation(id);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(id, fake_learning_controller_.cancel_args_.id_);
}

TEST_F(MojoLearningTaskControllerTest, PredictDistribution) {
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};

  TargetHistogram observed_prediction;
  learning_controller_->PredictDistribution(
      features, base::BindOnce(
                    [](TargetHistogram* test_storage,
                       const std::optional<TargetHistogram>& predicted) {
                      *test_storage = *predicted;
                    },
                    &observed_prediction));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(features, fake_learning_controller_.predict_args_.features_);
  EXPECT_FALSE(fake_learning_controller_.predict_args_.callback_.is_null());

  TargetHistogram expected_prediction;
  expected_prediction[TargetValue(1)] = 1.0;
  expected_prediction[TargetValue(2)] = 2.0;
  expected_prediction[TargetValue(3)] = 3.0;
  std::move(fake_learning_controller_.predict_args_.callback_)
      .Run(expected_prediction);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(observed_prediction, expected_prediction);
}

}  // namespace learning
}  // namespace media
