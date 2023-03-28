// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_REFRESH_STATE_TRACKER_H_
#define IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_REFRESH_STATE_TRACKER_H_

// Tracks state of the feed with regards to refreshing the feed, such as whether
// the feed is user visible, or if the user has engaged with the latest refresh
// content.
@protocol FeedRefreshStateTracker

// Returns YES if the user has engaged with the latest refreshed content. The
// term "engaged" is an implementation detail of the receiver.
- (BOOL)hasEngagedWithLatestRefreshedContent;

// Returns YES if the NTP and feed is visible to the user. Returns NO if the NTP
// is not visible or if the feed is toggled off in the feed header menu.
// Deprecated.
- (BOOL)isNTPAndFeedVisible;

// Returns YES if the NTP is visible to the user.
- (BOOL)isNTPVisible;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_METRICS_FEED_REFRESH_STATE_TRACKER_H_
