// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/learning_experiment_helper.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "media/learning/common/learning_task_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ::media::learning::FeatureDictionary;
using ::media::learning::FeatureValue;
using ::media::learning::FeatureVector;
using ::media::learning::LearningTask;
using ::media::learning::LearningTaskController;
using ::media::learning::ObservationCompletion;
using ::media::learning::TargetValue;
using ::testing::_;

class MockLearningTaskController : public LearningTaskController {
 public:
  explicit MockLearningTaskController(const LearningTask& task) : task_(task) {}
  MockLearningTaskController(const MockLearningTaskController&) = delete;
  MockLearningTaskController& operator=(const MockLearningTaskController&) =
      delete;
  ~MockLearningTaskController() override = default;

  MOCK_METHOD4(BeginObservation,
               void(base::UnguessableToken id,
                    const FeatureVector& features,
                    const std::optional<TargetValue>& default_value,
                    const std::optional<ukm::SourceId>& source_id));
  MOCK_METHOD2(CompleteObservation,
               void(base::UnguessableToken id,
                    const ObservationCompletion& completion));
  MOCK_METHOD1(CancelObservation, void(base::UnguessableToken id));
  MOCK_METHOD2(UpdateDefaultTarget,
               void(base::UnguessableToken id,
                    const std::optional<TargetValue>& default_target));
  MOCK_METHOD2(PredictDistribution,
               void(const FeatureVector& features, PredictionCB callback));

  const LearningTask& GetLearningTask() override { return task_; }

 private:
  LearningTask task_;
};

class LearningExperimentHelperTest : public testing::Test {
 public:
  void SetUp() override {
    const std::string feature_name_1("feature 1");
    const FeatureValue feature_value_1("feature value 1");

    const std::string feature_name_2("feature 2");
    const FeatureValue feature_value_2("feature value 2");

    const std::string feature_name_3("feature 3");
    const FeatureValue feature_value_3("feature value 3");
    dict_.Add(feature_name_1, feature_value_1);
    dict_.Add(feature_name_2, feature_value_2);
    dict_.Add(feature_name_3, feature_value_3);

    task_.feature_descriptions.push_back({"some other feature"});
    task_.feature_descriptions.push_back({feature_name_3});
    task_.feature_descriptions.push_back({feature_name_1});

    std::unique_ptr<MockLearningTaskController> controller =
        std::make_unique<MockLearningTaskController>(task_);
    controller_raw_ = controller.get();

    helper_ = std::make_unique<LearningExperimentHelper>(std::move(controller));
  }

  LearningTask task_;
  std::unique_ptr<LearningExperimentHelper> helper_;
  raw_ptr<MockLearningTaskController> controller_raw_ = nullptr;

  FeatureDictionary dict_;
};

TEST_F(LearningExperimentHelperTest, BeginComplete) {
  EXPECT_CALL(*controller_raw_, BeginObservation(_, _, _, _));
  helper_->BeginObservation(dict_);
  TargetValue target(123);
  EXPECT_CALL(*controller_raw_,
              CompleteObservation(_, ObservationCompletion(target)))
      .Times(1);
  helper_->CompleteObservationIfNeeded(target);

  // Make sure that a second Complete doesn't send anything.
  testing::Mock::VerifyAndClear(controller_raw_);
  EXPECT_CALL(*controller_raw_,
              CompleteObservation(_, ObservationCompletion(target)))
      .Times(0);
  helper_->CompleteObservationIfNeeded(target);
}

TEST_F(LearningExperimentHelperTest, BeginCancel) {
  EXPECT_CALL(*controller_raw_, BeginObservation(_, _, _, _));
  helper_->BeginObservation(dict_);
  EXPECT_CALL(*controller_raw_, CancelObservation(_));
  helper_->CancelObservationIfNeeded();
}

TEST_F(LearningExperimentHelperTest, CompleteWithoutBeginDoesNothing) {
  EXPECT_CALL(*controller_raw_, BeginObservation(_, _, _, _)).Times(0);
  EXPECT_CALL(*controller_raw_, CompleteObservation(_, _)).Times(0);
  EXPECT_CALL(*controller_raw_, CancelObservation(_)).Times(0);
  helper_->CompleteObservationIfNeeded(TargetValue(123));
}

TEST_F(LearningExperimentHelperTest, CancelWithoutBeginDoesNothing) {
  EXPECT_CALL(*controller_raw_, BeginObservation(_, _, _, _)).Times(0);
  EXPECT_CALL(*controller_raw_, CompleteObservation(_, _)).Times(0);
  EXPECT_CALL(*controller_raw_, CancelObservation(_)).Times(0);
  helper_->CancelObservationIfNeeded();
}

TEST_F(LearningExperimentHelperTest, DoesNothingWithoutController) {
  // Make sure that nothing crashes if there's no controller.
  LearningExperimentHelper helper(nullptr);

  // Begin / complete.
  helper_->BeginObservation(dict_);
  TargetValue target(123);
  helper_->CompleteObservationIfNeeded(target);

  // Begin / cancel.
  helper_->BeginObservation(dict_);
  helper_->CancelObservationIfNeeded();

  // Cancel without begin.
  helper_->CancelObservationIfNeeded();
}

}  // namespace blink
