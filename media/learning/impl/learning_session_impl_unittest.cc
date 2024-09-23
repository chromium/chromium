// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "media/learning/common/learning_task_controller.h"
#include "media/learning/impl/learning_session_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearningSessionImplTest : public testing::Test {
 public:
  class FakeLearningTaskController;
  using ControllerVector =
      std::vector<raw_ptr<FakeLearningTaskController, VectorExperimental>>;
  using TaskRunnerVector =
      std::vector<raw_ptr<base::SequencedTaskRunner, VectorExperimental>>;

  class FakeLearningTaskController : public LearningTaskController {
   public:
    // Send ControllerVector* as void*, else it complains that args can't be
    // forwarded.  Adding base::Unretained() doesn't help.
    FakeLearningTaskController(void* controllers,
                               const LearningTask& task,
                               SequenceBoundFeatureProvider feature_provider)
        : feature_provider_(std::move(feature_provider)) {
      static_cast<ControllerVector*>(controllers)->push_back(this);
      // As a complete hack, call the only public method on fp so that
      // we can verify that it was given to us by the session.
      if (!feature_provider_.is_null()) {
        feature_provider_.AsyncCall(&FeatureProvider::AddFeatures)
            .WithArgs(FeatureVector(), FeatureProvider::FeatureVectorCB());
      }
    }

    void BeginObservation(
        base::UnguessableToken id,
        const FeatureVector& features,
        const std::optional<TargetValue>& default_target,
        const std::optional<ukm::SourceId>& source_id) override {
      id_ = id;
      observation_features_ = features;
      default_target_ = default_target;
      source_id_ = source_id;
    }

    void CompleteObservation(base::UnguessableToken id,
                             const ObservationCompletion& completion) override {
      EXPECT_EQ(id_, id);
      example_.features = std::move(observation_features_);
      example_.target_value = completion.target_value;
      example_.weight = completion.weight;
    }

    void CancelObservation(base::UnguessableToken id) override {
      cancelled_id_ = id;
    }

    void UpdateDefaultTarget(
        base::UnguessableToken id,
        const std::optional<TargetValue>& default_target) override {
      // Should not be called, since LearningTaskControllerImpl doesn't support
      // default values.
      updated_id_ = id;
    }

    const LearningTask& GetLearningTask() override { NOTREACHED(); }

    void PredictDistribution(const FeatureVector& features,
                             PredictionCB callback) override {
      predict_features_ = features;
      predict_cb_ = std::move(callback);
    }

    SequenceBoundFeatureProvider feature_provider_;
    base::UnguessableToken id_;
    FeatureVector observation_features_;
    FeatureVector predict_features_;
    PredictionCB predict_cb_;
    std::optional<TargetValue> default_target_;
    std::optional<ukm::SourceId> source_id_;
    LabelledExample example_;

    // Most recently cancelled id.
    base::UnguessableToken cancelled_id_;

    // Id of most recently changed default target value.
    std::optional<base::UnguessableToken> updated_id_;
  };

  class FakeFeatureProvider : public FeatureProvider {
   public:
    FakeFeatureProvider(bool* flag_ptr) : flag_ptr_(flag_ptr) {}

    // Do nothing, except note that we were called.
    void AddFeatures(FeatureVector features,
                     FeatureProvider::FeatureVectorCB cb) override {
      *flag_ptr_ = true;
    }

    raw_ptr<bool> flag_ptr_ = nullptr;
  };

  LearningSessionImplTest() {
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    session_ = std::make_unique<LearningSessionImpl>(task_runner_);
    session_->SetTaskControllerFactoryCBForTesting(base::BindRepeating(
        [](ControllerVector* controllers, TaskRunnerVector* task_runners,
           scoped_refptr<base::SequencedTaskRunner> task_runner,
           const LearningTask& task,
           SequenceBoundFeatureProvider feature_provider)
            -> base::SequenceBound<LearningTaskController> {
          task_runners->push_back(task_runner.get());
          return base::SequenceBound<FakeLearningTaskController>(
              task_runner, static_cast<void*>(controllers), task,
              std::move(feature_provider));
        },
        &task_controllers_, &task_runners_));

    task_0_.name = "task_0";
    task_1_.name = "task_1";
  }

  ~LearningSessionImplTest() override {
    // To prevent a memory leak, reset the session.  This will post destruction
    // of other objects, so RunUntilIdle().
    session_.reset();
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<LearningSessionImpl> session_;

  LearningTask task_0_;
  LearningTask task_1_;

  ControllerVector task_controllers_;
  TaskRunnerVector task_runners_;
};

TEST_F(LearningSessionImplTest, RegisteringTasksCreatesControllers) {
  EXPECT_EQ(task_controllers_.size(), 0u);
  EXPECT_EQ(task_runners_.size(), 0u);

  session_->RegisterTask(task_0_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_.size(), 1u);
  EXPECT_EQ(task_runners_.size(), 1u);
  EXPECT_EQ(task_runners_[0], task_runner_.get());

  session_->RegisterTask(task_1_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_.size(), 2u);
  EXPECT_EQ(task_runners_.size(), 2u);
  EXPECT_EQ(task_runners_[1], task_runner_.get());

  // Make sure controllers are being returned for the right tasks.
  // Note: this test passes because LearningSessionController::GetController()
  // returns a wrapper around a FakeLTC, instead of the FakeLTC itself. The
  // wrapper internally built by LearningSessionImpl has a proper implementation
  // of GetLearningTask(), whereas the FakeLTC does not.
  std::unique_ptr<LearningTaskController> ltc_0 =
      session_->GetController(task_0_.name);
  EXPECT_EQ(ltc_0->GetLearningTask().name, task_0_.name);

  std::unique_ptr<LearningTaskController> ltc_1 =
      session_->GetController(task_1_.name);
  EXPECT_EQ(ltc_1->GetLearningTask().name, task_1_.name);
}

TEST_F(LearningSessionImplTest, ExamplesAreForwardedToCorrectTask) {
  session_->RegisterTask(task_0_);
  session_->RegisterTask(task_1_);

  base::UnguessableToken id = base::UnguessableToken::Create();

  LabelledExample example_0({FeatureValue(123), FeatureValue(456)},
                            TargetValue(1234));
  std::unique_ptr<LearningTaskController> ltc_0 =
      session_->GetController(task_0_.name);
  ukm::SourceId source_id(123);
  ltc_0->BeginObservation(id, example_0.features, std::nullopt, source_id);
  ltc_0->CompleteObservation(
      id, ObservationCompletion(example_0.target_value, example_0.weight));

  LabelledExample example_1({FeatureValue(321), FeatureValue(654)},
                            TargetValue(4321));

  std::unique_ptr<LearningTaskController> ltc_1 =
      session_->GetController(task_1_.name);
  ltc_1->BeginObservation(id, example_1.features);
  ltc_1->CompleteObservation(
      id, ObservationCompletion(example_1.target_value, example_1.weight));

  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->example_, example_0);
  EXPECT_EQ(task_controllers_[0]->source_id_, source_id);
  EXPECT_EQ(task_controllers_[1]->example_, example_1);
}

TEST_F(LearningSessionImplTest, ControllerLifetimeScopedToSession) {
  session_->RegisterTask(task_0_);

  std::unique_ptr<LearningTaskController> controller =
      session_->GetController(task_0_.name);

  // Destroy the session.  |controller| should still be usable, though it won't
  // forward requests anymore.
  session_.reset();
  task_environment_.RunUntilIdle();

  // Should not crash.
  controller->BeginObservation(base::UnguessableToken::Create(),
                               FeatureVector());
}

TEST_F(LearningSessionImplTest, FeatureProviderIsForwarded) {
  // Verify that a FeatureProvider actually gets forwarded to the LTC.
  bool flag = false;
  session_->RegisterTask(
      task_0_, base::SequenceBound<FakeFeatureProvider>(task_runner_, &flag));
  task_environment_.RunUntilIdle();
  // Registering the task should create a FakeLearningTaskController, which will
  // call AddFeatures on the fake FeatureProvider.
  EXPECT_TRUE(flag);
}

TEST_F(LearningSessionImplTest, DestroyingControllerCancelsObservations) {
  session_->RegisterTask(task_0_);

  std::unique_ptr<LearningTaskController> controller =
      session_->GetController(task_0_.name);
  task_environment_.RunUntilIdle();

  // Start an observation and verify that it starts.
  base::UnguessableToken id = base::UnguessableToken::Create();
  controller->BeginObservation(id, FeatureVector());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->id_, id);
  EXPECT_NE(task_controllers_[0]->cancelled_id_, id);

  // Should result in cancelling the observation.
  controller.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->cancelled_id_, id);
}

TEST_F(LearningSessionImplTest,
       DestroyingControllerCompletesObservationsWithDefaultValues) {
  // Also verifies that we don't send the default to the underlying controller,
  // because LearningTaskControllerImpl doesn't support it.
  session_->RegisterTask(task_0_);

  std::unique_ptr<LearningTaskController> controller =
      session_->GetController(task_0_.name);
  task_environment_.RunUntilIdle();

  // Start an observation and verify that it doesn't forward the default target.
  base::UnguessableToken id = base::UnguessableToken::Create();
  TargetValue default_target(123);
  controller->BeginObservation(id, FeatureVector(), default_target);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->id_, id);
  EXPECT_FALSE(task_controllers_[0]->default_target_);

  // Should complete the observation.
  controller.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->example_.target_value, default_target);
}

TEST_F(LearningSessionImplTest, ChangeDefaultTargetToValue) {
  session_->RegisterTask(task_0_);

  std::unique_ptr<LearningTaskController> controller =
      session_->GetController(task_0_.name);
  task_environment_.RunUntilIdle();

  // Start an observation without a default, then add one.
  base::UnguessableToken id = base::UnguessableToken::Create();
  controller->BeginObservation(id, FeatureVector(), std::nullopt);
  TargetValue default_target(123);
  controller->UpdateDefaultTarget(id, default_target);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->id_, id);

  // Should complete the observation.
  controller.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->example_.target_value, default_target);

  // Shouldn't notify the underlying controller.
  EXPECT_FALSE(task_controllers_[0]->updated_id_);
}

TEST_F(LearningSessionImplTest, ChangeDefaultTargetToNoValue) {
  session_->RegisterTask(task_0_);

  std::unique_ptr<LearningTaskController> controller =
      session_->GetController(task_0_.name);
  task_environment_.RunUntilIdle();

  // Start an observation with a default, then remove it.
  base::UnguessableToken id = base::UnguessableToken::Create();
  TargetValue default_target(123);
  controller->BeginObservation(id, FeatureVector(), default_target);
  controller->UpdateDefaultTarget(id, std::nullopt);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->id_, id);

  // Should cancel the observation.
  controller.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(task_controllers_[0]->cancelled_id_, id);

  // Shouldn't notify the underlying controller.
  EXPECT_FALSE(task_controllers_[0]->updated_id_);
}

TEST_F(LearningSessionImplTest, PredictDistribution) {
  session_->RegisterTask(task_0_);

  std::unique_ptr<LearningTaskController> controller =
      session_->GetController(task_0_.name);
  task_environment_.RunUntilIdle();

  FeatureVector features = {FeatureValue(123), FeatureValue(456)};
  TargetHistogram observed_prediction;
  controller->PredictDistribution(
      features, base::BindOnce(
                    [](TargetHistogram* test_storage,
                       const std::optional<TargetHistogram>& predicted) {
                      *test_storage = *predicted;
                    },
                    &observed_prediction));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(features, task_controllers_[0]->predict_features_);
  EXPECT_FALSE(task_controllers_[0]->predict_cb_.is_null());

  TargetHistogram expected_prediction;
  expected_prediction[TargetValue(1)] = 1.0;
  expected_prediction[TargetValue(2)] = 2.0;
  expected_prediction[TargetValue(3)] = 3.0;
  std::move(task_controllers_[0]->predict_cb_).Run(expected_prediction);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_prediction, observed_prediction);
}

}  // namespace learning
}  // namespace media
