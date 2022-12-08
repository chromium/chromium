// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_SESSION_RECORDER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_SESSION_RECORDER_TESTING_H_

#import "base/time/time.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_session_recorder.h"

// Category for exposing properties and methods for testing.
@interface FeedSessionRecorder (Testing)
// Exposing the recorded session duration.
@property(nonatomic, assign) base::TimeDelta sessionDuration;
// Exposing the recorded time between sessions.
@property(nonatomic, assign) base::TimeDelta timeBetweenSessions;
// Exposing the recorded time between interactions.
@property(nonatomic, assign) base::TimeDelta timeBetweenInteractions;
// Exposing previousTimeInFeedForGoodVisitSession
@property(nonatomic, assign)
    NSTimeInterval previousTimeInFeedForGoodVisitSession;
// Exposing a private version of the method `recordUserInteractionOrScrolling`
// that takes an argument. `interactionDate` is the date (and time) of the user
// interaction or scrolling event.
- (void)recordUserInteractionOrScrollingAtDate:(base::Time)interactionDate;
@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_SESSION_RECORDER_TESTING_H_
