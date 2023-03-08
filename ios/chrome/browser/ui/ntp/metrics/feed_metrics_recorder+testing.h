// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_METRICS_RECORDER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_METRICS_RECORDER_TESTING_H_

#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"

// Category for exposing properties and methods for testing.
@interface FeedMetricsRecorder (Testing)

// Exposing the timeSpentInFeed property to check if time is properly recorded.
@property(nonatomic, assign) base::TimeDelta timeSpentInFeed;

// Exposing resetGoodVisitSession to mimic session expiration.
- (void)resetGoodVisitSession;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_METRICS_RECORDER_TESTING_H_
