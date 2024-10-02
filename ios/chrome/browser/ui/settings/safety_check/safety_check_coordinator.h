// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace password_manager {
enum class PasswordCheckReferrer;
}  // namespace password_manager

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
// `browser`: profile for preferences and password check.
// `referrer`: Where in the app the Safety Check is being requested from.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                            referrer:(password_manager::PasswordCheckReferrer)
                                         referrer NS_DESIGNATED_INITIALIZER;

// Start a safety check if it is not currently running.
- (void)startCheckIfNotRunning;

// Updates the UI to display the appropriate notifications button
// (either to "Turn on" or "Turn off" notifications) based on the provided
// `enabled` parameter.
//
// If `enabled` is YES, the button will prompt users to "Turn off" notifications
// and, when tapped, will terminate the ongoing process of sending Safety Check
// push notifications and may present a toast confirming the change.
//
// If 'enabled' is NO, the button will prompt users to "Turn on" notifications
// and, when tapped, may present an opt-in prompt if the user has not yet
// granted notification permission. If permission is granted or already exists,
// it will initiate the process of sending Safety Check push notifications.
- (void)updateNotificationsButton:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_COORDINATOR_H_
