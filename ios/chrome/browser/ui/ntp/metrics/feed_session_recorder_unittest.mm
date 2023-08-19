// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/feed_session_recorder.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/ntp/metrics/feed_session_recorder+testing.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Reference of time. Don't use 0 or it will be processed as nullptr instead of
// 0 seconds.
constexpr base::Time kOriginOfTime =
    base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(1));

}  // anonymous namespace

// Subclass used for testing the FeedSessionRecorder.
@interface TestFeedSessionRecorder : FeedSessionRecorder
// Redefining this property in a subclass prevents reading and writing to
// NSUserDefaults during testing.
@property(nonatomic, assign) base::Time previousInteractionDate;
@end

@implementation TestFeedSessionRecorder
@end

using FeedSessionRecorderTest = PlatformTest;

// Tests that the session duration is correctly calculated.
TEST_F(FeedSessionRecorderTest, SessionDuration) {
  TestFeedSessionRecorder* recorder = [[TestFeedSessionRecorder alloc] init];

  const base::Time date_0 = kOriginOfTime;
  const base::Time date_1 = kOriginOfTime + base::Seconds(5);
  [recorder recordUserInteractionOrScrollingAtDate:date_0];
  [recorder recordUserInteractionOrScrollingAtDate:date_1];

  // There should be no session duration yet, since there is no interaction
  // outside of the session timeout (5 mins).
  EXPECT_EQ(base::Seconds(0), recorder.sessionDuration);

  // date_2 must be at least 5 minutes after date_1.
  const base::Time date_2 = kOriginOfTime + base::Minutes(6);
  [recorder recordUserInteractionOrScrollingAtDate:date_2];

  EXPECT_EQ(base::Seconds(5), recorder.sessionDuration);
}

// Tests that the time between session is correctly calculated.
TEST_F(FeedSessionRecorderTest, TimeBetweenSessions) {
  TestFeedSessionRecorder* recorder = [[TestFeedSessionRecorder alloc] init];

  const base::Time date_0 = kOriginOfTime;
  const base::Time date_1 = kOriginOfTime + base::Seconds(5);
  [recorder recordUserInteractionOrScrollingAtDate:date_0];
  [recorder recordUserInteractionOrScrollingAtDate:date_1];

  // A new session hasn't yet begun.
  EXPECT_EQ(base::Seconds(0), recorder.timeBetweenSessions);

  // date_2 must be at least 5 minutes after date_1.
  const base::Time date_2 = kOriginOfTime + base::Minutes(6) + base::Seconds(5);
  [recorder recordUserInteractionOrScrollingAtDate:date_2];

  EXPECT_EQ(base::Minutes(6), recorder.timeBetweenSessions);
}

// Tests that the time between interactions is correctly calculated.
TEST_F(FeedSessionRecorderTest, TimeBetweenInteractions) {
  TestFeedSessionRecorder* recorder = [[TestFeedSessionRecorder alloc] init];

  const base::Time date_0 = kOriginOfTime;
  [recorder recordUserInteractionOrScrollingAtDate:date_0];

  // Cannot compute the time between interactions if there is only one.
  EXPECT_EQ(base::Seconds(0), recorder.timeBetweenInteractions);

  const base::Time date_1 = kOriginOfTime + base::Seconds(5);
  [recorder recordUserInteractionOrScrollingAtDate:date_1];

  EXPECT_EQ(base::Seconds(5), recorder.timeBetweenInteractions);

  const base::Time date_2 = kOriginOfTime + base::Seconds(9);
  [recorder recordUserInteractionOrScrollingAtDate:date_2];

  EXPECT_EQ(base::Seconds(4), recorder.timeBetweenInteractions);
}
