// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/feed_session_recorder.h"

#import <Foundation/Foundation.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Category for exposing properties and methods for testing.
@interface FeedSessionRecorder (Testing)
// Exposing the recorded session duration.
@property(nonatomic, assign) NSTimeInterval sessionDurationInSeconds;
// Exposing the recorded time between sessions.
@property(nonatomic, assign) NSTimeInterval secondsBetweenSessions;
// Exposing the recorded time between interactions.
@property(nonatomic, assign) NSTimeInterval secondsBetweenInteractions;
// Exposing a private version of the method `recordUserInteractionOrScrolling`
// that takes an argument. `interactionDate` is the date (and time) of the user
// interaction or scrolling event.
- (void)recordUserInteractionOrScrollingAtDate:(NSDate*)interactionDate;
@end

// Subclass used for testing the FeedSessionRecorder.
@interface TestFeedSessionRecorder : FeedSessionRecorder
// Redefining this property in a subclass prevents reading and writing to
// NSUserDefaults during testing.
@property(nonatomic, copy) NSDate* previousInteractionDate;
@end

@implementation TestFeedSessionRecorder
@end

using FeedSessionRecorderTest = PlatformTest;

// Tests that the session duration is correctly calculated.
TEST_F(FeedSessionRecorderTest, SessionDuration) {
  TestFeedSessionRecorder* recorder = [[TestFeedSessionRecorder alloc] init];

  // Note: time intervals are in seconds.
  NSDate* date_0 = [NSDate dateWithTimeIntervalSinceReferenceDate:0];
  NSDate* date_1 = [NSDate dateWithTimeIntervalSinceReferenceDate:5];
  [recorder recordUserInteractionOrScrollingAtDate:date_0];
  [recorder recordUserInteractionOrScrollingAtDate:date_1];

  // There should be no session duration yet, since there is no interaction
  // outside of the session timeout (5 mins).
  EXPECT_EQ(0, recorder.sessionDurationInSeconds);

  // date_2 must be at least 5 minutes after date_1.
  NSDate* date_2 = [NSDate dateWithTimeIntervalSinceReferenceDate:6 * 60];
  [recorder recordUserInteractionOrScrollingAtDate:date_2];

  EXPECT_EQ(5, recorder.sessionDurationInSeconds);
}

// Tests that the time between session is correctly calculated.
TEST_F(FeedSessionRecorderTest, TimeBetweenSessions) {
  TestFeedSessionRecorder* recorder = [[TestFeedSessionRecorder alloc] init];

  // Note: time intervals are in seconds.
  NSDate* date_0 = [NSDate dateWithTimeIntervalSinceReferenceDate:0];
  NSDate* date_1 = [NSDate dateWithTimeIntervalSinceReferenceDate:5];
  [recorder recordUserInteractionOrScrollingAtDate:date_0];
  [recorder recordUserInteractionOrScrollingAtDate:date_1];

  // A new session hasn't yet begun.
  EXPECT_EQ(0, recorder.secondsBetweenSessions);

  // date_2 must be at least 5 minutes after date_1.
  NSDate* date_2 = [NSDate dateWithTimeIntervalSinceReferenceDate:5 + 6 * 60];
  [recorder recordUserInteractionOrScrollingAtDate:date_2];

  EXPECT_EQ(6 * 60, recorder.secondsBetweenSessions);
}

// Tests that the time between interactions is correctly calculated.
TEST_F(FeedSessionRecorderTest, TimeBetweenInteractions) {
  TestFeedSessionRecorder* recorder = [[TestFeedSessionRecorder alloc] init];

  // Note: time intervals are in seconds.
  NSDate* date_0 = [NSDate dateWithTimeIntervalSinceReferenceDate:0];
  [recorder recordUserInteractionOrScrollingAtDate:date_0];

  // Cannot compute the time between interactions if there is only one.
  EXPECT_EQ(0, recorder.secondsBetweenInteractions);

  NSDate* date_1 = [NSDate dateWithTimeIntervalSinceReferenceDate:5];
  [recorder recordUserInteractionOrScrollingAtDate:date_1];

  EXPECT_EQ(5, recorder.secondsBetweenInteractions);

  NSDate* date_2 = [NSDate dateWithTimeIntervalSinceReferenceDate:9];
  [recorder recordUserInteractionOrScrollingAtDate:date_2];

  EXPECT_EQ(4, recorder.secondsBetweenInteractions);
}
