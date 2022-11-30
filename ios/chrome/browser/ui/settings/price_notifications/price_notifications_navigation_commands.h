// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_NAVIGATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_NAVIGATION_COMMANDS_H_

// Protocol for navigating to different setting pages from Price Notifications
// setting.
@protocol PriceNotificationsNavigationCommands <NSObject>

// Shows tracking price screen.
- (void)showTrackingPrice;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_NAVIGATION_COMMANDS_H_
