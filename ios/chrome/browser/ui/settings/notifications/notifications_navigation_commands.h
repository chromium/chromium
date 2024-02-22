// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_NAVIGATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_NAVIGATION_COMMANDS_H_

// Protocol for navigating to different setting pages from Notifications
// setting.
@protocol NotificationsNavigationCommands <NSObject>

// Shows tracking price screen.
- (void)showTrackingPrice;

// Shows the content screen.
- (void)showContent;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_NAVIGATION_COMMANDS_H_
