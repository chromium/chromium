// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_DELEGATE_H_

// Actions taken on the feed management UI.
@protocol FeedManagementDelegate

// User tapped to see the WebChannels they are following.
- (void)followingTapped;

// User tapped to see topics they are interested in.
- (void)interestsTapped;

// User tapped to see their hidden topics
- (void)hiddenTapped;

// User tapped to see their feed activity.
- (void)activityTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FEED_MANAGEMENT_DELEGATE_H_
