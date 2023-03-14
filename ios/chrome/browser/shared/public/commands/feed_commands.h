// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FEED_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FEED_COMMANDS_H_

@class FollowedWebSite;

// Commands related to feed.
@protocol FeedCommands

// Displays the First Follow UI with `followedWebSite`.
- (void)showFirstFollowUIForWebSite:(FollowedWebSite*)followedWebSite;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FEED_COMMANDS_H_
