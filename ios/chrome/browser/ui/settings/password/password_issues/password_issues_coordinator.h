// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ApplicationCommands;
class Browser;
@class PasswordIssuesCoordinator;
@class ReauthenticationModule;

namespace password_manager {
enum class WarningType;
}

// Delegate for PasswordIssuesCoordinator.
@protocol PasswordIssuesCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator;

// Called by a PasswordIssuesCoordinator child to tell its
// PasswordIssuesCoordinator parent to dismiss its own
// PasswordIssuesTableViewController when all password issues are gone. This
// happens when the PasswordIssuesCoordinator child gets notified by its
// PasswordDetailsCoordinator that a password will be deleted from the password
// details page. A PasswordIssuesCoordinator should dismiss its
// PasswordIssuesTableViewController immediately after being notified that all
// issues are gone only when its last issue was resolved from a password
// deletion from the details page.
- (void)setShouldDismissOnAllIssuesGone;

@end

// This coordinator presents a list of compromised credentials for the user.
@interface PasswordIssuesCoordinator : ChromeCoordinator

- (instancetype)initForWarningType:(password_manager::WarningType)warningType
          baseNavigationController:(UINavigationController*)navigationController
                           browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Reauthentication module used by password details coordinator.
@property(nonatomic, strong) ReauthenticationModule* reauthModule;

@property(nonatomic, weak) id<PasswordIssuesCoordinatorDelegate> delegate;

@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_COORDINATOR_H_
