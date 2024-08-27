// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_MENU_COMMANDS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_MENU_COMMANDS_H_

// Protocol for actions relating to the NTP feed top-level control menu.
@protocol FeedMenuCommands

// Creates feed management menu for a `button`.
- (void)configureManagementMenu:(UIButton*)button;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FEED_MENU_COMMANDS_H_
