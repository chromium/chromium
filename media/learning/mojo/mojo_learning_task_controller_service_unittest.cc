// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/learning/mojo/mojo_learning_task_controller_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class MojoLearningTaskControllerServiceTest : public ::testing::Test {
 public:
  class FakeLearningTaskController : public LearningTaskController {
   public:
    void BeginObservation(
        base::UnguessableToken id,
        const FeatureVector& features,
        const base::Optional<TargetValue>& default_target) override {
      begin_args_.id_ = id;
      begin_args_.features_ = features;
      begin_args_.default_target_ = std::move(default_target);
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
        const base::Optional<TargetValue>& default_target) override {
      update_default_args_.id_ = id;
      update_default_args_.default_target_ = default_target;
    }

    const LearningTask& GetLearningTask() override {
      return LearningTask::Empty();
    }

    struct {
      base::UnguessableToken id_;
      FeatureVector features_;
      base::Optional<TargetValue> default_target_;
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
      base::Optional<TargetValue> default_target_;
    } update_default_args_;
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
        task_, std::move(controller));
  }

  LearningTask task_;

  // Mojo stuff.
  base::test::TaskEnvironment task_environment_;

  FakeLearningTaskController* controller_raw_ = nullptr;

  // The learner under test.
  std::unique_ptr<MojoLearningTaskControllerService> service_;
};

TEST_F(MojoLearningTaskControllerServiceTest, BeginComplete) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  service_->BeginObservation(id, features, base::nullopt);
  EXPECT_EQ(id, controller_raw_->begin_args_.id_);
  EXPECT_EQ(features, controller_raw_->begin_args_.features_);
  EXPECT_FALSE(controller_raw_->begin_args_.default_target_);

  ObservationCompletion completion(TargetValue(1234));
  service_->CompleteObservation(id, completion);

  EXPECT_EQ(id, controller_raw_->complete_args_.id_);
  EXPECT_EQ(completion.target_value,
            controller_raw_->complete_args_.completion_.target_value);
}

TEST_F(MojoLearningTaskControllerServiceTest, BeginCancel) {
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  service_->BeginObservation(id, features, base::nullopt);
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
}

TEST_F(MojoLearningTaskControllerServiceTest, TooFewFeaturesIsIgnored) {
  // A FeatureVector with too few elements should be ignored.
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector short_features = {FeatureValue(123)};
  service_->BeginObservation(id, short_features, base::nullopt);
  EXPECT_NE(id, controller_raw_->begin_args_.id_);
  EXPECT_EQ(controller_raw_->begin_args_.features_.size(), 0u);
}

TEST_F(MojoLearningTaskControllerServiceTest, TooManyFeaturesIsIgnored) {
  // A FeatureVector with too many elements should be ignored.
  base::UnguessableToken id = base::UnguessableToken::Create();
  FeatureVector long_features = {FeatureValue(123), FeatureValue(456),
                                 FeatureValue(789)};
  service_->BeginObservation(id, long_features, base::nullopt);
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
  service_->BeginObservation(id, features, base::nullopt);
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
  service_->UpdateDefaultTarget(id, base::nullopt);
  EXPECT_EQ(id, controller_raw_->update_default_args_.id_);
  EXPECT_EQ(base::nullopt,
            controller_raw_->update_default_args_.default_target_);
}

}  // namespace learning
}  // namespace media
