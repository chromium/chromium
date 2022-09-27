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
  static base::TimeDelta priority_escalation_after_input_duration() {
    return base::Milliseconds(UserModel::kGestureEstimationLimitMillis);
  }

  static base::TimeDelta subsequent_input_expected_after_input_duration() {
    return base::Milliseconds(UserModel::kExpectSubsequentGestureMillis);
  }

  std::unique_ptr<base::SimpleTestTickClock> clock_;
  std::unique_ptr<UserModel> user_model_;
};

TEST_F(UserModelTest, TimeLeftInUserGesture_NoInput) {
  EXPECT_EQ(base::TimeDelta(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, TimeLeftInUserGesture_ImmediatelyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kTouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  EXPECT_EQ(priority_escalation_after_input_duration(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, TimeLeftInUserGesture_ShortlyAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kTouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);
  EXPECT_EQ(priority_escalation_after_input_duration() - delta,
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, TimeLeftInUserGesture_LongAfterInput) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kTouchStart, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  clock_->Advance(priority_escalation_after_input_duration() * 2);
  EXPECT_EQ(base::TimeDelta(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

TEST_F(UserModelTest, DidFinishProcessingInputEvent_Delayed) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kTouchStart, clock_->NowTicks());
  clock_->Advance(priority_escalation_after_input_duration() * 10);

  EXPECT_EQ(priority_escalation_after_input_duration(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));

  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  base::TimeDelta delta(base::Milliseconds(10));
  clock_->Advance(delta);

  EXPECT_EQ(priority_escalation_after_input_duration() - delta,
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
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
  EXPECT_EQ(base::Milliseconds(UserModel::kMedianGestureDurationMillis) - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_LongAfter_GestureScrollBegin) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta delta(
      base::Milliseconds(UserModel::kMedianGestureDurationMillis * 2));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                 &prediction_valid_duration));
  EXPECT_EQ(base::Milliseconds(UserModel::kExpectSubsequentGestureMillis),
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_ImmediatelyAfter_GestureScrollEnd) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollEnd, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());

  base::TimeDelta prediction_valid_duration;
  EXPECT_TRUE(user_model_->IsGestureExpectedSoon(clock_->NowTicks(),
                                                 &prediction_valid_duration));
  EXPECT_EQ(subsequent_input_expected_after_input_duration(),
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
  EXPECT_EQ(subsequent_input_expected_after_input_duration() - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest, GestureExpectedSoon_LongAfter_GestureScrollEnd) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollEnd, clock_->NowTicks());
  user_model_->DidFinishProcessingInputEvent(clock_->NowTicks());
  clock_->Advance(subsequent_input_expected_after_input_duration() * 2);

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
  EXPECT_EQ(subsequent_input_expected_after_input_duration() - delta,
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
  EXPECT_EQ(base::Milliseconds(UserModel::kMedianGestureDurationMillis),
            prediction_valid_duration);
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
  EXPECT_EQ(base::Milliseconds(UserModel::kMedianGestureDurationMillis) - delta,
            prediction_valid_duration);
}

TEST_F(UserModelTest, IsGestureExpectedToContinue_LongAfterGestureStarted) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());

  base::TimeDelta delta(
      base::Milliseconds(UserModel::kMedianGestureDurationMillis * 2));
  clock_->Advance(delta);

  base::TimeDelta prediction_valid_duration;
  EXPECT_FALSE(user_model_->IsGestureExpectedToContinue(
      clock_->NowTicks(), &prediction_valid_duration));
  EXPECT_EQ(base::TimeDelta(), prediction_valid_duration);
}

TEST_F(UserModelTest, ResetPendingInputCount) {
  user_model_->DidStartProcessingInputEvent(
      blink::WebInputEvent::Type::kGestureScrollBegin, clock_->NowTicks());
  EXPECT_EQ(priority_escalation_after_input_duration(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
  user_model_->Reset(clock_->NowTicks());
  EXPECT_EQ(base::TimeDelta(),
            user_model_->TimeLeftInUserGesture(clock_->NowTicks()));
}

}  // namespace scheduler
}  // namespace blink
