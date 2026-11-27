// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NOTIFICATIONS_OPT_IN_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NOTIFICATIONS_OPT_IN_COMMANDS_H_

#import <UIKit/UIKit.h>

enum class NotificationOptInAccessPoint;

// Commands related to the notifications opt-in screen.
@protocol NotificationsOptInCommands

// Shows the notifications opt-in view from `accessPoint`, presented on top of
// `baseViewController`.
- (void)showNotificationsOptInFromAccessPoint:
            (NotificationOptInAccessPoint)accessPoint
                           baseViewController:
                               (UIViewController*)baseViewController;

// Hides the notifications opt-in view.
- (void)hideNotificationsOptIn;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_NOTIFICATIONS_OPT_IN_COMMANDS_H_
