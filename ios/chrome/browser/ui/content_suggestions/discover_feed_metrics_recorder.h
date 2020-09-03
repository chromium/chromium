// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_METRICS_RECORDER_H_

#import <UIKit/UIKit.h>

// Records different metrics for the NTP's Discover feed.
@interface DiscoverFeedMetricsRecorder : NSObject

// Record metrics for when the user has reached the bottom of their current
// feed.
- (void)recordInfiniteFeedTriggered;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_METRICS_RECORDER_H_
