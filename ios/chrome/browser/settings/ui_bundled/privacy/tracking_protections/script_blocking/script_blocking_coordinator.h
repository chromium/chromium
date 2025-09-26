// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_SCRIPT_BLOCKING_SCRIPT_BLOCKING_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_SCRIPT_BLOCKING_SCRIPT_BLOCKING_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ScriptBlockingCoordinator;

// Delegate for ScriptBlockingCoordinator.
@protocol ScriptBlockingCoordinatorDelegate

// Called when the view controller is removed from the navigation controller.
- (void)scriptBlockingCoordinatorDidRemove:
    (ScriptBlockingCoordinator*)coordinator;

@end

// Coordinator for the script blocking settings page.
@interface ScriptBlockingCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<ScriptBlockingCoordinatorDelegate> delegate;

// Designated initializer.
// `viewController`: navigation controller.
// `browser`: browser.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_TRACKING_PROTECTIONS_SCRIPT_BLOCKING_SCRIPT_BLOCKING_COORDINATOR_H_
