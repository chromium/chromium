// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/user_activation_state.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class UserActivationStateTest : public testing::Test {
 public:
  void SetUp() override {
    time_overrides_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        nullptr, &UserActivationStateTest::Now, nullptr);
  }

  static base::TimeTicks Now() {
    now_ticks_ += base::TimeDelta::FromMilliseconds(1);
    return now_ticks_;
  }

  static void AdvanceClock(base::TimeDelta time_delta) {
    now_ticks_ += time_delta;
  }

 private:
  static base::TimeTicks now_ticks_;
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_overrides_;
};

// static
base::TimeTicks UserActivationStateTest::now_ticks_;

TEST_F(UserActivationStateTest, ConsumptionTest) {
  UserActivationState user_activation_state;

  // Initially both sticky and transient bits are unset, and consumption
  // attempts fail.
  EXPECT_FALSE(user_activation_state.HasBeenActive());
  EXPECT_FALSE(user_activation_state.IsActive());
  EXPECT_FALSE(user_activation_state.ConsumeIfActive());
  EXPECT_FALSE(user_activation_state.ConsumeIfActive());

  user_activation_state.Activate();

  // After activation, both sticky and transient bits are set, and consumption
  // attempt succeeds once.
  EXPECT_TRUE(user_activation_state.HasBeenActive());
  EXPECT_TRUE(user_activation_state.IsActive());
  EXPECT_TRUE(user_activation_state.ConsumeIfActive());
  EXPECT_FALSE(user_activation_state.ConsumeIfActive());

  // After successful consumption, only the transient bit gets reset, and
  // further consumption attempts fail.
  EXPECT_TRUE(user_activation_state.HasBeenActive());
  EXPECT_FALSE(user_activation_state.IsActive());
  EXPECT_FALSE(user_activation_state.ConsumeIfActive());
  EXPECT_FALSE(user_activation_state.ConsumeIfActive());
}

TEST_F(UserActivationStateTest, ExpirationTest) {
  UserActivationState user_activation_state;

  user_activation_state.Activate();

  // Right before activation expiry, both bits remain set.
  AdvanceClock(base::TimeDelta::FromMilliseconds(4995));
  EXPECT_TRUE(user_activation_state.HasBeenActive());
  EXPECT_TRUE(user_activation_state.IsActive());

  // Right after activation expiry, only the transient bit gets reset.
  AdvanceClock(base::TimeDelta::FromMilliseconds(10));
  EXPECT_TRUE(user_activation_state.HasBeenActive());
  EXPECT_FALSE(user_activation_state.IsActive());
}

TEST_F(UserActivationStateTest, ClearingTest) {
  UserActivationState user_activation_state;

  user_activation_state.Activate();

  EXPECT_TRUE(user_activation_state.HasBeenActive());
  EXPECT_TRUE(user_activation_state.IsActive());

  user_activation_state.Clear();

  EXPECT_FALSE(user_activation_state.HasBeenActive());
  EXPECT_FALSE(user_activation_state.IsActive());
}

TEST_F(UserActivationStateTest, ConsumptionPlusExpirationTest) {
  UserActivationState user_activation_state;

  // An activation is consumable before expiry.
  user_activation_state.Activate();
  AdvanceClock(base::TimeDelta::FromMilliseconds(900));
  EXPECT_TRUE(user_activation_state.ConsumeIfActive());

  // An activation is not consumable after expiry.
  user_activation_state.Activate();
  AdvanceClock(base::TimeDelta::FromSeconds(5));
  EXPECT_FALSE(user_activation_state.ConsumeIfActive());

  // Consecutive activations within expiry is consumable only once.
  user_activation_state.Activate();
  AdvanceClock(base::TimeDelta::FromMilliseconds(900));
  user_activation_state.Activate();
  EXPECT_TRUE(user_activation_state.ConsumeIfActive());
  EXPECT_FALSE(user_activation_state.ConsumeIfActive());

  // Non-consecutive activations within expiry is consumable separately.
  user_activation_state.Activate();
  EXPECT_TRUE(user_activation_state.ConsumeIfActive());
  AdvanceClock(base::TimeDelta::FromSeconds(900));
  user_activation_state.Activate();
  EXPECT_TRUE(user_activation_state.ConsumeIfActive());
}

TEST_F(UserActivationStateTest, TransferBooleanTest) {
  UserActivationState source;
  UserActivationState target;

  // Transfer from inactive source to inactive target.
  source.Clear();
  target.Clear();
  target.TransferFrom(source);

  EXPECT_FALSE(source.HasBeenActive());
  EXPECT_FALSE(source.IsActive());
  EXPECT_FALSE(target.HasBeenActive());
  EXPECT_FALSE(target.IsActive());

  // Transfer from inactive source to active target.
  source.Clear();
  target.Activate();
  target.TransferFrom(source);

  EXPECT_FALSE(source.HasBeenActive());
  EXPECT_FALSE(source.IsActive());
  EXPECT_TRUE(target.HasBeenActive());
  EXPECT_TRUE(target.IsActive());

  // Transfer from active source to inactive target.
  source.Activate();
  target.Clear();
  target.TransferFrom(source);

  EXPECT_FALSE(source.HasBeenActive());
  EXPECT_FALSE(source.IsActive());
  EXPECT_TRUE(target.HasBeenActive());
  EXPECT_TRUE(target.IsActive());

  // Transfer from active source to active target.
  source.Activate();
  target.Activate();
  target.TransferFrom(source);

  EXPECT_FALSE(source.HasBeenActive());
  EXPECT_FALSE(source.IsActive());
  EXPECT_TRUE(target.HasBeenActive());
  EXPECT_TRUE(target.IsActive());
}

TEST_F(UserActivationStateTest, TransferExpirationTest) {
  UserActivationState source;
  UserActivationState target;

  // Source activated before target.
  source.Activate();
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  target.Activate();
  target.TransferFrom(source);

  AdvanceClock(base::TimeDelta::FromMilliseconds(4995));
  EXPECT_TRUE(target.IsActive());
  AdvanceClock(base::TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(target.IsActive());

  // Source activated after target.
  target.Activate();
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  source.Activate();
  target.TransferFrom(source);

  AdvanceClock(base::TimeDelta::FromMilliseconds(4995));
  EXPECT_TRUE(target.IsActive());
  AdvanceClock(base::TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(target.IsActive());

  // Source and target activated at the same time.
  source.Activate();
  target.Activate();
  target.TransferFrom(source);

  AdvanceClock(base::TimeDelta::FromMilliseconds(4995));
  EXPECT_TRUE(target.IsActive());
  AdvanceClock(base::TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(target.IsActive());

  // Inactive target received transfer from active source after a delay.
  source.Activate();
  target.Clear();
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  target.TransferFrom(source);

  AdvanceClock(base::TimeDelta::FromMilliseconds(3995));
  EXPECT_TRUE(target.IsActive());
  AdvanceClock(base::TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(target.IsActive());

  // Active target received transfer from inactive source after a delay.
  source.Clear();
  target.Activate();
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  target.TransferFrom(source);

  AdvanceClock(base::TimeDelta::FromMilliseconds(3995));
  EXPECT_TRUE(target.IsActive());
  AdvanceClock(base::TimeDelta::FromMilliseconds(10));
  EXPECT_FALSE(target.IsActive());
}

}  // namespace blink
