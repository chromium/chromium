// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_FEED_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_FEED_COMMANDS_H_

// Commands related to feed.
@protocol FeedCommands

// Displays the First Follow modal with |webChannelTitle|.
- (void)showFirstFollowModalWithWebChannelTitle:(NSString*)webChannelTitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_FEED_COMMANDS_H_
