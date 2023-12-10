// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/password_manager_reauthentication_delegate.h"

class Browser;
@class PasswordsInOtherAppsCoordinator;

// Delegate for PasswordsInOtherAppsCoordinator.
@protocol PasswordsInOtherAppsCoordinatorDelegate <
    PasswordManagerReauthenticationDelegate>

// Called when the view controller is removed from navigation controller.
- (void)passwordsInOtherAppsCoordinatorDidRemove:
    (PasswordsInOtherAppsCoordinator*)coordinator;

@end

// This coordinator presents passwords in other apps promotion to the user.
@interface PasswordsInOtherAppsCoordinator : ChromeCoordinator

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Delegate.
@property(nonatomic, weak) id<PasswordsInOtherAppsCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_IN_OTHER_APPS_PASSWORDS_IN_OTHER_APPS_COORDINATOR_H_
