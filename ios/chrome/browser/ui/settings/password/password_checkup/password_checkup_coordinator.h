// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_COORDINATOR_H_

#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/password_manager_reauthentication_delegate.h"

@protocol ApplicationCommands;
@class PasswordCheckupCoordinator;
namespace password_manager {
enum class WarningType;
}
@protocol ReauthenticationProtocol;

// Delegate for PasswordCheckupCoordinator.
@protocol
    PasswordCheckupCoordinatorDelegate <PasswordManagerReauthenticationDelegate>

// Called when the view controller is removed from navigation controller.
- (void)passwordCheckupCoordinatorDidRemove:
    (PasswordCheckupCoordinator*)coordinator;

@end

// This coordinator presents the Password Checkup homepage.
@interface PasswordCheckupCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                        reauthModule:(id<ReauthenticationProtocol>)reauthModule
                            referrer:(password_manager::PasswordCheckReferrer)
                                         referrer NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak) id<PasswordCheckupCoordinatorDelegate> delegate;

@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// Show the Password Issues page for `warningType`.
- (void)showPasswordIssuesWithWarningType:
    (password_manager::WarningType)warningType;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_CHECKUP_PASSWORD_CHECKUP_COORDINATOR_H_
