// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/smoothness_helper.h"

#include "base/run_loop.h"
#include "media/blink/blink_platform_with_task_environment.h"
#include "media/learning/common/labelled_example.h"
#include "media/learning/common/learning_task_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using learning::FeatureValue;
using learning::FeatureVector;
using learning::LearningTask;
using learning::LearningTaskController;
using learning::ObservationCompletion;
using learning::TargetValue;

using testing::_;
using testing::AnyNumber;
using testing::Eq;
using testing::Gt;
using testing::Lt;
using testing::ResultOf;
using testing::Return;

// Helper for EXPECT_CALL argument matching on Optional<TargetValue>.  Applies
// matcher |m| to the TargetValue as a double.  For example:
// void Foo(base::Optional<TargetValue>);
// EXPECT_CALL(..., Foo(OPT_TARGET(Gt(0.9)))) will expect that the value of the
// Optional<TargetValue> passed to Foo() to be greather than 0.9 .
#define OPT_TARGET(m) \
  ResultOf([](const base::Optional<TargetValue>& v) { return (*v).value(); }, m)

// Same as above, but expects an ObservationCompletion.
#define COMPLETION_TARGET(m)                                                 \
  ResultOf(                                                                  \
      [](const ObservationCompletion& x) { return x.target_value.value(); }, \
      m)

class SmoothnessHelperTest : public testing::Test {
  class MockLearningTaskController : public LearningTaskController {
   public:
    MOCK_METHOD3(BeginObservation,
                 void(base::UnguessableToken id,
                      const FeatureVector& features,
                      const base::Optional<TargetValue>& default_target));

    MOCK_METHOD2(CompleteObservation,
                 void(base::UnguessableToken id,
                      const ObservationCompletion& completion));

    MOCK_METHOD1(CancelObservation, void(base::UnguessableToken id));

    MOCK_METHOD2(UpdateDefaultTarget,
                 void(base::UnguessableToken id,
                      const base::Optional<TargetValue>& default_target));

    MOCK_METHOD0(GetLearningTask, const LearningTask&());
  };

  class MockClient : public SmoothnessHelper::Client {
   public:
    ~MockClient() override = default;

    MOCK_CONST_METHOD0(DecodedFrameCount, unsigned(void));
    MOCK_CONST_METHOD0(DroppedFrameCount, unsigned(void));
  };

 public:
  void SetUp() override {
    auto ltc = std::make_unique<MockLearningTaskController>();
    ltc_ = ltc.get();
    features_.push_back(FeatureValue(123));
    helper_ = SmoothnessHelper::Create(std::move(ltc), features_, &client_);
    segment_size_ = SmoothnessHelper::SegmentSizeForTesting();
  }

  // Helper for EXPECT_CALL.
  base::Optional<TargetValue> Opt(double x) {
    return base::Optional<TargetValue>(TargetValue(x));
  }

  void FastForwardBy(base::TimeDelta amount) {
    BlinkPlatformWithTaskEnvironment::GetTaskEnvironment()->FastForwardBy(
        amount);
  }

  // Set the dropped / decoded totals that will be returned by the mock client.
  void SetFrameCounters(int dropped, int decoded) {
    ON_CALL(client_, DroppedFrameCount()).WillByDefault(Return(dropped));
    ON_CALL(client_, DecodedFrameCount()).WillByDefault(Return(decoded));
  }

  // Helper under test
  std::unique_ptr<SmoothnessHelper> helper_;

  MockLearningTaskController* ltc_ = nullptr;
  MockClient client_;
  FeatureVector features_;

  base::TimeDelta segment_size_;
};

TEST_F(SmoothnessHelperTest, PauseWithoutPlayDoesNothing) {
  EXPECT_CALL(*ltc_, BeginObservation(_, _, _)).Times(0);
  helper_->NotifyPlayState(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SmoothnessHelperTest, PlayThenImmediatePauseCancelsObservation) {
  // If not enough time has elapsed, play then pause shouldn't record anything.
  // Note that Begin then Cancel would be okay too, but it's hard to set
  // expectations for either case.  So, we just pick the one that it actually
  // does in this case.
  EXPECT_CALL(*ltc_, BeginObservation(_, _, _)).Times(0);
  helper_->NotifyPlayState(true);
  helper_->NotifyPlayState(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SmoothnessHelperTest, PlayRecordsWorstSegment) {
  // Record three segments, and see if it chooses the worst.
  SetFrameCounters(0, 0);
  helper_->NotifyPlayState(true);
  base::RunLoop().RunUntilIdle();

  // First segment has no dropped frames..
  EXPECT_CALL(*ltc_, BeginObservation(_, _, OPT_TARGET(Eq(0.0)))).Times(1);
  SetFrameCounters(0, 1000);
  FastForwardBy(segment_size_);
  base::RunLoop().RunUntilIdle();

  // Second segment has quite a lot of dropped frames.
  EXPECT_CALL(*ltc_, UpdateDefaultTarget(_, OPT_TARGET(Gt(0.99)))).Times(1);
  SetFrameCounters(999, 2000);
  FastForwardBy(segment_size_);
  base::RunLoop().RunUntilIdle();

  // Third segment has no dropped frames, so the default shouldn't change.
  EXPECT_CALL(*ltc_, UpdateDefaultTarget(_, OPT_TARGET(Gt(0.99)))).Times(1);
  SetFrameCounters(999, 3000);
  FastForwardBy(segment_size_);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*ltc_, CompleteObservation(_, COMPLETION_TARGET(Gt(0.99))))
      .Times(1);
  helper_->NotifyPlayState(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SmoothnessHelperTest, PlayIgnoresTrailingPartialSegments) {
  helper_->NotifyPlayState(true);
  base::RunLoop().RunUntilIdle();

  // First segment has no dropped frames.
  EXPECT_CALL(*ltc_, BeginObservation(_, _, OPT_TARGET(Eq(0.0)))).Times(1);
  SetFrameCounters(0, 1000);
  FastForwardBy(segment_size_);
  base::RunLoop().RunUntilIdle();

  // Second segment has a lot of dropped frames, but isn't a full segment.
  SetFrameCounters(1000, 2000);
  FastForwardBy(segment_size_ / 2);
  base::RunLoop().RunUntilIdle();

  // On completion, we the observation should have no dropped frames.
  EXPECT_CALL(*ltc_, CompleteObservation(_, COMPLETION_TARGET(Lt(0.1))))
      .Times(1);
  helper_->NotifyPlayState(false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SmoothnessHelperTest, DestructionRecordsObservations) {
  // Destroying |helper_| should not send any observation; the last default
  // value should be used.
  helper_->NotifyPlayState(true);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*ltc_, BeginObservation(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*ltc_, UpdateDefaultTarget(_, _)).Times(AnyNumber());
  EXPECT_CALL(*ltc_, CancelObservation(_)).Times(0);
  EXPECT_CALL(*ltc_, CompleteObservation(_, _)).Times(0);

  // Fast forward so that we're sure that there is something to record.
  SetFrameCounters(0, 1000);
  FastForwardBy(segment_size_);
  SetFrameCounters(0, 2000);
  FastForwardBy(segment_size_);
  helper_.reset();

  base::RunLoop().RunUntilIdle();
}

}  // namespace media
