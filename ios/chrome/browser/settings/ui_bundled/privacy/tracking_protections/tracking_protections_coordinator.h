// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_TRACKING_PROTECTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_TRACKING_PROTECTIONS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class TrackingProtectionsCoordinator;

// Delegate for TrackingProtectionsCoordinator.
@protocol TrackingProtectionsCoordinatorDelegate

// Called when the view controller is removed from the navigation controller.
- (void)trackingProtectionsCoordinatorDidRemove:
    (TrackingProtectionsCoordinator*)coordinator;

@end

// Coordinator for the tracking protection settings page.
@interface TrackingProtectionsCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<TrackingProtectionsCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// `viewController`: navigation controller.
// `browser`: browser.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_TRACKING_PROTECTIONS_COORDINATOR_H_
