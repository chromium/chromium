// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_FEED_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_FEED_COMMANDS_H_

@class FollowedWebChannel;

// Commands related to feed.
@protocol FeedCommands

// Displays the First Follow UI with `followedWebChannel`.
- (void)showFirstFollowUIForWebChannel:(FollowedWebChannel*)followedWebChannel;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_FEED_COMMANDS_H_
