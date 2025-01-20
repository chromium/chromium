// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_NOTIFICATIONS_NOTIFICATIONS_EARL_GREY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_NOTIFICATIONS_NOTIFICATIONS_EARL_GREY_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// The app interface for notifications, facilitating test setup.
@interface NotificationsEarlGreyAppInterface : NSObject

// Replaces the shopping service with one where all features are enabled.
+ (void)setUpMockShoppingService;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_NOTIFICATIONS_NOTIFICATIONS_EARL_GREY_APP_INTERFACE_H_
