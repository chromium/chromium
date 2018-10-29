// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/testing/wtf/scoped_mock_clock.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

// Checks for the initial state of UserGestureIndicator.
TEST(UserGestureIndicatorTest, InitialState) {
  EXPECT_FALSE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentToken());
  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNewUserGesture) {
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(nullptr, UserGestureToken::kNewGesture);

  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

  EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithUserGesture) {
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(nullptr);

  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

  EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNoUserGesture) {
  UserGestureIndicator user_gesture_scope(nullptr);

  EXPECT_FALSE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentToken());

  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
}

// Check that after UserGestureIndicator destruction state will be cleared.
TEST(UserGestureIndicatorTest, DestructUserGestureIndicator) {
  {
    std::unique_ptr<UserGestureIndicator> user_gesture_scope =
        LocalFrame::NotifyUserActivation(nullptr);

    EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  }

  EXPECT_FALSE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentToken());
  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
}

// Tests creation of scoped UserGestureIndicator objects.
TEST(UserGestureIndicatorTest, ScopedNewUserGestureIndicators) {
  // Root GestureIndicator and GestureToken.
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      LocalFrame::NotifyUserActivation(nullptr, UserGestureToken::kNewGesture);

  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  {
    // Construct inner UserGestureIndicator.
    // It should share GestureToken with the root indicator.
    std::unique_ptr<UserGestureIndicator> inner_user_gesture =
        LocalFrame::NotifyUserActivation(nullptr,
                                         UserGestureToken::kNewGesture);

    EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

    // Consume inner gesture.
    EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
  }

  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

  // Consume root gesture.
  EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
  EXPECT_FALSE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
}

TEST(UserGestureIndicatorTest, MultipleGesturesWithTheSameToken) {
  std::unique_ptr<UserGestureIndicator> indicator =
      LocalFrame::NotifyUserActivation(nullptr, UserGestureToken::kNewGesture);
  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  {
    // Construct an inner indicator that shares the same token.
    UserGestureIndicator inner_indicator(UserGestureIndicator::CurrentToken());
    EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  }
  // Though the inner indicator was destroyed, the outer is still present (and
  // the gesture hasn't been consumed), so it should still be processing a user
  // gesture.
  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
}

TEST(UserGestureIndicatorTest, Timeouts) {
  WTF::ScopedMockClock clock;

  {
    // Token times out after 1 second.
    std::unique_ptr<UserGestureIndicator> user_gesture_scope =
        LocalFrame::NotifyUserActivation(nullptr);
    scoped_refptr<UserGestureToken> token = user_gesture_scope->CurrentToken();
    EXPECT_TRUE(token->HasGestures());
    clock.Advance(TimeDelta::FromSecondsD(0.75));
    EXPECT_TRUE(token->HasGestures());
    clock.Advance(TimeDelta::FromSecondsD(0.75));
    EXPECT_FALSE(token->HasGestures());
  }

  {
    // Timestamp is reset when a token is put in a new UserGestureIndicator.
    scoped_refptr<UserGestureToken> token;

    {
      std::unique_ptr<UserGestureIndicator> user_gesture_scope =
          LocalFrame::NotifyUserActivation(nullptr);
      token = user_gesture_scope->CurrentToken();
      EXPECT_TRUE(token->HasGestures());
      clock.Advance(TimeDelta::FromSecondsD(0.75));
      EXPECT_TRUE(token->HasGestures());
    }

    {
      UserGestureIndicator user_gesture_scope(token.get());
      clock.Advance(TimeDelta::FromSecondsD(0.75));
      EXPECT_TRUE(token->HasGestures());
      clock.Advance(TimeDelta::FromSecondsD(0.75));
      EXPECT_FALSE(token->HasGestures());
    }
  }
}

}  // namespace blink
