// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_MENU_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_MENU_COMMANDS_H_

// TODO(crbug.com/1261554): Remove this after moving the header.
// Protocol for actions relating to the Discover feed top-level control menu.
@protocol DiscoverFeedMenuCommands

// Opens Discover feed control menu.
- (void)openDiscoverFeedMenu;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_DISCOVER_FEED_MENU_COMMANDS_H_
