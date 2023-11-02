// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_FOLLOW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_FOLLOW_DELEGATE_H_

@class FollowedWebChannel;

// Actions taken on the list of followed web channels.
@protocol FollowManagementFollowDelegate

// User asked to unfollow the web channel `followedWebChannel`.
- (void)unfollowFollowedWebChannel:(FollowedWebChannel*)followedWebChannel;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_FOLLOW_DELEGATE_H_
