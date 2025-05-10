// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_NOTIFICATIONS_NOTIFICATIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_NOTIFICATIONS_NOTIFICATIONS_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/push_notification/ui_bundled/notifications_alert_presenter.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class NotificationsCoordinator;

// Delegate that allows to dereference the NotificationsCoordinator.
@protocol NotificationsCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)notificationsCoordinatorDidRemove:
    (NotificationsCoordinator*)coordinator;

@end

// The coordinator for the Notifications screen.
@interface NotificationsCoordinator
    : ChromeCoordinator <NotificationsAlertPresenter>

@property(nonatomic, weak) id<NotificationsCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

// Show Price Trackinhg Notifications settings.
- (void)showTrackingPrice;

// Visually highlights the table view row for the given `clientID`.
- (void)highlightClient:(PushNotificationClientId)clientID;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_NOTIFICATIONS_NOTIFICATIONS_COORDINATOR_H_
