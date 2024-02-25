// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/push_notification/notifications_alert_presenter.h"

@class ContentNotificationsCoordinator;

// Delegate that allows to dereference the ContentNotificationsCoordinator.
@protocol ContentNotificationsCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)contentNotificationsCoordinatorDidRemove:
    (ContentNotificationsCoordinator*)coordinator;

@end

// The coordinator for the Content Notifications screen.
@interface ContentNotificationsCoordinator
    : ChromeCoordinator <NotificationsAlertPresenter>

@property(nonatomic, weak) id<ContentNotificationsCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_COORDINATOR_H_
