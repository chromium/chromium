// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/user_model.h"

#include <memory>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class UserModelTest : public testing::Test {
 public:
  UserModelTest() = default;
  ~UserModelTest() override = default;

  void SetUp() override {
    clock_ = std::make_unique<base::SimpleTestTickClock>();
    clock_->Advance(base::Microseconds(5000));

    user_model_ = std::make_unique<UserModel>();
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> clock_;
  std::unique_ptr<UserModel> user_model_;
};

TEST_F(UserModelTest, TimeLeftInContinuousUserGesture_NoInput) {
  EXPECT_EQ(base::TimeDelta(),
            user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, TimeLeftInContinuousUserGesture_ImmediatelyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kTouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  EXPECT_EQ(UserModel::kGestureEstimationLimit,
            user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, TimeLeftInContinuousUserGesture_ShortlyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kTouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);
  EXPECT_EQ(UserModel::kGestureEstimationLimit - delta,
            user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, TimeLeftInContinuousUserGesture_LongAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kTouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  clock_->Advance(UserModel::kGestureEstimationLimit * 2);
  EXPECT_EQ(base::TimeDelta(),
            user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, DidFinishProcessingInputEvent_Delayed) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kTouchStart, clock_->NowTicks());
  clock_->Advance(UserModel::kGestureEstimationLimit * 10);

  EXPECT_EQ(UserModel::kGestureEstimationLimit,
            user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()));

  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);

  EXPECT_EQ(UserModel::kGestureEstimationLimit - delta,
            user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, GestureExpectedSoon_NoRecentInput) {
  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                  &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_ShortlyAfter_GestureScrollBegin) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                  &prediction_valid_duration));
  EXPECT_EQ(UserModel::kMedianGestureDuration - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_LongAfter_GestureScrollBegin) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(UserModel::kMedianGestureDuration * 2);
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                 &prediction_valid_duration));
  EXPECT_EQ(UserModel::kExpectSubsequentGestureDeadline,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_ImmediatelyAfter_GestureScrollEnd) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollEnd, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                 &prediction_valid_duration));
  EXPECT_EQ(UserModel::kExpectSubsequentGestureDeadline,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_ShortlyAfter_GestureScrollEnd) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollEnd, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                 &prediction_valid_duration));
  EXPECT_EQ(UserModel::kExpectSubsequentGestureDeadline - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_LongAfter_GestureScrollEnd) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollEnd, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  clock_->Advance(UserModel::kExpectSubsequentGestureDeadline * 2);

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                  &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_ShortlyAfter_GesturePinchEnd) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGesturePinchEnd, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                 &prediction_valid_duration));
  EXPECT_EQ(UserModel::kExpectSubsequentGestureDeadline - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_ShortlyAfterInput_GestureTap) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureTap, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                  &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest, IsGestureExpectedToContinue_NoGesture) {
  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedToContinue(
      clock_->NowTicks(), &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest, IsGestureExpectedToContinue_GestureJustStarted) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());
  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedToContinue(
      clock_->NowTicks(), &prediction_valid_duration));
  EXPECT_EQ(UserModel::kMedianGestureDuration, prediction_valid_duration);
}

TEST_F(UserModelTest, IsGestureExpectedToContinue_GestureJustEnded) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollEnd, clock_->NowTicks());
  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedToContinue(
      clock_->NowTicks(), &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest, IsGestureExpectedToContinue_ShortlyAfterGestureStarted) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());

  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedToContinue(
      clock_->NowTicks(), &prediction_valid_duration));
  EXPECT_EQ(UserModel::kMedianGestureDuration - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest, IsGestureExpectedToContinue_LongAfterGestureStarted) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());

  base::TimeDelta delta(UserModel::kMedianGestureDuration * 2);
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedToContinue(
      clock_->NowTicks(), &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest, ResetPendingInputCount) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());
  EXPECT_EQ(UserModel::kGestureEstimationLimit,
            user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()));
  user_model_->Reset(clock_->NowTicks());
  EXPECT_EQ(base::TimeDelta(),
            user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, DiscreteInput) {
  user_model_->DidProcessDiscreteInputEvent(clock_->NowTicks());
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            UserModel::kDiscreteInputResponseDeadline);
  user_model_->DidProcessDiscreteInputResponse();
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            base::TimeDelta());

  user_model_->DidProcessDiscreteInputEvent(clock_->NowTicks());
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            UserModel::kDiscreteInputResponseDeadline);

  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);

  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            UserModel::kDiscreteInputResponseDeadline - delta);

  clock_->Advance(UserModel::kDiscreteInputResponseDeadline - delta);
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            base::TimeDelta());

  user_model_->DidProcessDiscreteInputEvent(clock_->NowTicks());
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            UserModel::kDiscreteInputResponseDeadline);
  user_model_->Reset(clock_->NowTicks());
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            base::TimeDelta());
}

TEST_F(UserModelTest, DiscreteAndContinuousInput) {
  EXPECT_EQ(user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()),
            base::TimeDelta());
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            base::TimeDelta());

  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());
  EXPECT_EQ(user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()),
            UserModel::kGestureEstimationLimit);
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            base::TimeDelta());

  user_model_->DidProcessDiscreteInputEvent(clock_->NowTicks());
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            UserModel::kDiscreteInputResponseDeadline);
  EXPECT_EQ(user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()),
            UserModel::kGestureEstimationLimit);

  user_model_->DidProcessDiscreteInputResponse();
  EXPECT_EQ(user_model_->TimeLeftUntilDiscreteInputResponseDeadline(
                clock_->NowTicks()),
            base::TimeDelta());
  EXPECT_EQ(user_model_->TimeLeftInContinuousUserGesture(clock_->NowTicks()),
            UserModel::kGestureEstimationLimit);
}

}  // namespace scheduler
}  // namespace blink
