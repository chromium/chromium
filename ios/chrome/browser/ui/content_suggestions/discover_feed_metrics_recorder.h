// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_METRICS_RECORDER_H_

#import <UIKit/UIKit.h>

// Records different metrics for the NTP's Discover feed.
@interface DiscoverFeedMetricsRecorder : NSObject

// Record metrics for when the user has scrolled |scrollDistance| in the Feed.
- (void)recordFeedScrolled:(int)scrollDistance;

// Record metrics for when the user has reached the bottom of their current
// feed.
- (void)recordInfiniteFeedTriggered;

// Record metrics for when the user selects the 'Learn More' item in the feed
// header menu.
- (void)recordHeaderMenuLearnMoreTapped;

// Record metrics for when the user selects the 'Manage Activity' item in the
// feed header menu.
- (void)recordHeaderMenuManageActivityTapped;

// Record metrics for when the user selects the 'Manage Interests' item in the
// feed header menu.
- (void)recordHeaderMenuManageInterestsTapped;

// Record metrics for when the user toggles the feed visibility from the feed
// header menu.
- (void)recordDiscoverFeedVisibilityChanged:(BOOL)visible;

// Records metrics for when a user opens an article in the same tab.
- (void)recordOpenURLInSameTab;

// Records metrics for when a user opens an article in a new tab.
- (void)recordOpenURLInNewTab;

// Records metrics for when a user opens an article in an incognito tab.
- (void)recordOpenURLInIncognitoTab;

// Records metrics for when a user adds an article to Read Later.
- (void)recordAddURLToReadLater;

// Records metrics for when a user opens the Send Feedback form.
- (void)recordTapSendFeedback;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_METRICS_RECORDER_H_
