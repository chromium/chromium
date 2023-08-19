// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class LockdownModeCoordinator;

// Delegate for LockdownModeCoordinator.
@protocol LockdownModeCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)lockdownModeCoordinatorDidRemove:(LockdownModeCoordinator*)coordinator;

@end

// Coordinator for the lockdown mode settings page view.
@interface LockdownModeCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate.
@property(nonatomic, weak) id<LockdownModeCoordinatorDelegate> delegate;

// Designated initializer.
// `viewController`: navigation controller.
// `browser`: browser.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_LOCKDOWN_MODE_LOCKDOWN_MODE_COORDINATOR_H_
