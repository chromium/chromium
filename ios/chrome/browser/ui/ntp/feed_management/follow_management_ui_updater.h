// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_UI_UPDATER_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_UI_UPDATER_H_

@class FollowedWebChannel;

// Protocol for updating the follow management UI.
@protocol FollowManagementUIUpdater

// Removes the web channel from the followed web channels list corresponding
// to `channel`.
- (void)removeFollowedWebChannel:(FollowedWebChannel*)channel;

// Adds the web channel from the followed web channels list corresponding to
// `channel`.
- (void)addFollowedWebChannel:(FollowedWebChannel*)channel;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_MANAGEMENT_FOLLOW_MANAGEMENT_UI_UPDATER_H_
