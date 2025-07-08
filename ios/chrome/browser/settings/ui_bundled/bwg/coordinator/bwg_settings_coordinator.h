// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_BWG_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_BWG_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class BWGSettingsCoordinator;

// Delegate that allows to dereference the PrivacyCoordinator.
@protocol BWGSettingsCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)BWGSettingsCoordinatorViewControllerWasRemoved:
    (BWGSettingsCoordinator*)coordinator;

@end

// Coordinator for the BWG settings view.
@interface BWGSettingsCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// `viewController`: navigation controller.
// `browser`: browser.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic, weak) id<BWGSettingsCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_BWG_SETTINGS_COORDINATOR_H_
