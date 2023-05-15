// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_alert_presenter.h"

@class TrackingPriceCoordinator;

// Delegate that allows to dereference the TrackingPriceCoordinator.
@protocol TrackingPriceCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)trackingPriceCoordinatorDidRemove:
    (TrackingPriceCoordinator*)coordinator;

@end

// The coordinator for the Tracking Price screen.
@interface TrackingPriceCoordinator
    : ChromeCoordinator <TrackingPriceAlertPresenter>

@property(nonatomic, weak) id<TrackingPriceCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_TRACKING_PRICE_TRACKING_PRICE_COORDINATOR_H_
