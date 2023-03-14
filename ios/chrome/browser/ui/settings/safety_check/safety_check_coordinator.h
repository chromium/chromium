// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ApplicationCommands;
@class SafetyCheckCoordinator;

// Delegate that allows to dereference the SafetyCheckCoordinator.
@protocol SafetyCheckCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)safetyCheckCoordinatorDidRemove:(SafetyCheckCoordinator*)coordinator;

@end

// The coordinator for the Safety Check screen.
@interface SafetyCheckCoordinator : ChromeCoordinator

// Delegate to pass user interactions to the mediator.
@property(nonatomic, weak) id<SafetyCheckCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// `navigationController`: Handles user movement to check subpages.
// `browser`: browser state for preferences and password check.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

// Start a safety check if it is not currently running.
- (void)startCheckIfNotRunning;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_COORDINATOR_H_
