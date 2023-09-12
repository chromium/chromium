// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/user_activation_state.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class UserActivationStateTest : public testing::Test {
 public:
  void SetUp() override {
    time_overrides_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        nullptr, &UserActivationStateTest::Now, nullptr);
  }

  static base::TimeTicks Now() {
    now_ticks_ += base::Microseconds(1);
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

  user_activation_state.Activate(mojom::UserActivationNotificationType::kTest);

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

// MSan changes the timing of user activations, so skip this test.  We could
// memorize the changes, but they're arbitrary and not worth enforcing.  We
// could also move the timeouts into a header, but there's value in having
// them hardcoded here in case of accidental changes to the timeout.
#if !defined(MEMORY_SANITIZER)
TEST_F(UserActivationStateTest, ExpirationTest) {
  UserActivationState user_activation_state;

  user_activation_state.Activate(mojom::UserActivationNotificationType::kTest);

  // Right before activation expiry, both bits remain set.
  AdvanceClock(base::Milliseconds(4995));
  EXPECT_TRUE(user_activation_state.HasBeenActive());
  EXPECT_TRUE(user_activation_state.IsActive());

  // Right after activation expiry, only the transient bit gets reset.
  AdvanceClock(base::Milliseconds(10));
  EXPECT_TRUE(user_activation_state.HasBeenActive());
  EXPECT_FALSE(user_activation_state.IsActive());
}
#endif  // !MEMORY_SANITIZER

TEST_F(UserActivationStateTest, ClearingTest) {
  UserActivationState user_activation_state;

  user_activation_state.Activate(mojom::UserActivationNotificationType::kTest);

  EXPECT_TRUE(user_activation_state.HasBeenActive());
  EXPECT_TRUE(user_activation_state.IsActive());

  user_activation_state.Clear();

  EXPECT_FALSE(user_activation_state.HasBeenActive());
  EXPECT_FALSE(user_activation_state.IsActive());
}

// MSan changes the timing of user activations, so skip this test.  We could
// memorize the changes, but they're arbitrary and not worth enforcing.  We
// could also move the timeouts into a header, but there's value in having
// them hardcoded here in case of accidental changes to the timeout.
#if !defined(MEMORY_SANITIZER)
TEST_F(UserActivationStateTest, ConsumptionPlusExpirationTest) {
  UserActivationState user_activation_state;

  // An activation is consumable before expiry.
  user_activation_state.Activate(mojom::UserActivationNotificationType::kTest);
  AdvanceClock(base::Milliseconds(900));
  EXPECT_TRUE(user_activation_state.ConsumeIfActive());

  // An activation is not consumable after expiry.
  user_activation_state.Activate(mojom::UserActivationNotificationType::kTest);
  AdvanceClock(base::Seconds(5));
  EXPECT_FALSE(user_activation_state.ConsumeIfActive());

  // Consecutive activations within expiry is consumable only once.
  user_activation_state.Activate(mojom::UserActivationNotificationType::kTest);
  AdvanceClock(base::Milliseconds(900));
  user_activation_state.Activate(mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(user_activation_state.ConsumeIfActive());
  EXPECT_FALSE(user_activation_state.ConsumeIfActive());

  // Non-consecutive activations within expiry is consumable separately.
  user_activation_state.Activate(mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(user_activation_state.ConsumeIfActive());
  AdvanceClock(base::Seconds(900));
  user_activation_state.Activate(mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(user_activation_state.ConsumeIfActive());
}
#endif  // !MEMORY_SANITIZER

}  // namespace blink
