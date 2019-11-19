// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"

#include "base/test/test_mock_time_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

TEST(UserGestureIndicatorTest, InitialState) {
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentTokenForTest());
}

TEST(UserGestureIndicatorTest, ConstructedWithNewUserGesture) {
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(nullptr);
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentTokenForTest());
}

// Check that after UserGestureIndicator destruction state will be cleared.
TEST(UserGestureIndicatorTest, DestructUserGestureIndicator) {
  {
    std::unique_ptr<UserGestureIndicator> user_gesture_scope =
        LocalFrame::NotifyUserActivation(nullptr);
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentTokenForTest());
  }
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentTokenForTest());
}

// Tests creation of scoped UserGestureIndicator objects.
TEST(UserGestureIndicatorTest, ScopedNewUserGestureIndicators) {
  // Root GestureIndicator and GestureToken.
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(nullptr);

  EXPECT_NE(nullptr, UserGestureIndicator::CurrentTokenForTest());
  {
    // Construct inner UserGestureIndicator.
    // It should share GestureToken with the root indicator.
    std::unique_ptr<UserGestureIndicator> inner_user_gesture =
        LocalFrame::NotifyUserActivation(nullptr);
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentTokenForTest());
  }

  EXPECT_NE(nullptr, UserGestureIndicator::CurrentTokenForTest());
}

TEST(UserGestureIndicatorTest, MultipleGesturesWithTheSameToken) {
  std::unique_ptr<UserGestureIndicator> indicator =
      LocalFrame::NotifyUserActivation(nullptr);
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentTokenForTest());
  {
    // Construct an inner indicator that shares the same token.
    UserGestureIndicator inner_indicator(
        UserGestureIndicator::CurrentTokenForTest());
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentTokenForTest());
  }
  // Though the inner indicator was destroyed, the outer is still present (and
  // the gesture hasn't been consumed), so it should still be processing a user
  // gesture.
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentTokenForTest());
}

TEST(UserGestureIndicatorTest, Timeouts) {
  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  {
    // Token times out after 1 second.
    std::unique_ptr<UserGestureIndicator> user_gesture_scope =
        LocalFrame::NotifyUserActivation(nullptr);
    scoped_refptr<UserGestureToken> token =
        user_gesture_scope->CurrentTokenForTest();
    token->SetClockForTesting(test_task_runner->GetMockClock());
    // Timestamp is initialized to Now() in constructor using the default clock,
    // reset it so it gets the Now() of the mock clock.
    token->ResetTimestampForTesting();
    EXPECT_TRUE(token->HasGestures());
    test_task_runner->FastForwardBy(base::TimeDelta::FromSecondsD(0.75));
    EXPECT_TRUE(token->HasGestures());
    test_task_runner->FastForwardBy(base::TimeDelta::FromSecondsD(0.75));
    EXPECT_FALSE(token->HasGestures());
  }

  {
    // Timestamp is reset when a token is put in a new UserGestureIndicator.
    scoped_refptr<UserGestureToken> token;

    {
      std::unique_ptr<UserGestureIndicator> user_gesture_scope =
          LocalFrame::NotifyUserActivation(nullptr);
      token = user_gesture_scope->CurrentTokenForTest();
      token->SetClockForTesting(test_task_runner->GetMockClock());
      // Timestamp is initialized to Now() in constructor using the default
      // clock, reset it so it gets the Now() of the mock clock.
      token->ResetTimestampForTesting();
      EXPECT_TRUE(token->HasGestures());
      test_task_runner->FastForwardBy(base::TimeDelta::FromSecondsD(0.75));
      EXPECT_TRUE(token->HasGestures());
    }

    {
      UserGestureIndicator user_gesture_scope(token.get());
      test_task_runner->FastForwardBy(base::TimeDelta::FromSecondsD(0.75));
      EXPECT_TRUE(token->HasGestures());
      test_task_runner->FastForwardBy(base::TimeDelta::FromSecondsD(0.75));
      EXPECT_FALSE(token->HasGestures());
    }
  }
}

}  // namespace blink
