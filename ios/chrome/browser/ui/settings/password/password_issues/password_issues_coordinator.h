// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/password_manager_reauthentication_delegate.h"

@protocol ApplicationCommands;
class Browser;
@class PasswordIssuesCoordinator;
@protocol ReauthenticationProtocol;

namespace password_manager {
enum class WarningType;
}

// Delegate for PasswordIssuesCoordinator.
@protocol
    PasswordIssuesCoordinatorDelegate <PasswordManagerReauthenticationDelegate>

// Called when the view controller is removed from navigation controller.
- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator;

@end

// This coordinator presents a list of compromised credentials for the user.
@interface PasswordIssuesCoordinator : ChromeCoordinator

- (instancetype)initForWarningType:(password_manager::WarningType)warningType
          baseNavigationController:(UINavigationController*)navigationController
                           browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Reauthentication module used by password details coordinator.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

@property(nonatomic, weak) id<PasswordIssuesCoordinatorDelegate> delegate;

@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// Whether Local Authentication should be skipped when the coordinator is
// started. Defaults to NO. Authentication should be required when starting the
// coordinator unless it was already required by the starting coordinator or
// another ancestor higher in the ancestor chain. This property is most likely
// used only by coordinators for other password manager subpages as the password
// manager requires authentication upon entry.
@property(nonatomic) BOOL skipAuthenticationOnStart;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_COORDINATOR_H_
