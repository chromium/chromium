// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_UI_BUNDLED_NOTIFICATIONS_OPT_IN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_UI_BUNDLED_NOTIFICATIONS_OPT_IN_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol NotificationsOptInCoordinatorDelegate;
enum class NotificationOptInAccessPoint;

// Coordinator that manages the presentation of the
// NotificationsOptInViewController.
@interface NotificationsOptInCoordinator : ChromeCoordinator

// The delegate that receives events from this coordinator.
@property(nonatomic, weak) id<NotificationsOptInCoordinatorDelegate> delegate;

// The access point that triggered the opt-in flow.
@property(nonatomic, assign) NotificationOptInAccessPoint accessPoint;

@end

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_UI_BUNDLED_NOTIFICATIONS_OPT_IN_COORDINATOR_H_
