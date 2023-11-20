// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/password_manager_reauthentication_delegate.h"

class Browser;
@class PasswordsCoordinator;

// Delegate for PasswordsCoordinator.
@protocol PasswordsCoordinatorDelegate <PasswordManagerReauthenticationDelegate>

// Called when the view controller is removed from navigation controller.
- (void)passwordsCoordinatorDidRemove:(PasswordsCoordinator*)coordinator;

@end

// This coordinator presents a list of saved passwords and some passwords
// related features.
@interface PasswordsCoordinator : ChromeCoordinator

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Starts password check. For example, used by PasswordBreachDialog to
// automatically start the check.
- (void)checkSavedPasswords;

@property(nonatomic, weak) id<PasswordsCoordinatorDelegate> delegate;

@property(nonatomic, strong, readonly) UIViewController* viewController;

// Flag indicating whether the PasswordManagerViewController should be presented
// in search mode.
@property(nonatomic, assign) BOOL openViewControllerForPasswordSearch;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_COORDINATOR_H_
