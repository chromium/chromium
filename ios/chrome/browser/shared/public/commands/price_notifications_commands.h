// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PRICE_NOTIFICATIONS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PRICE_NOTIFICATIONS_COMMANDS_H_

// Commands related to Price Notifications
@protocol PriceNotificationsCommands

// Hides the price notifications UI.
- (void)hidePriceNotifications;

// Shows the price notifications UI.
- (void)showPriceNotifications;

// Shows the price notifications IPH.
- (void)presentPriceNotificationsWhileBrowsingIPH;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PRICE_NOTIFICATIONS_COMMANDS_H_
