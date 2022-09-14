// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace password_manager {
struct CredentialUIEntry;
}  // namespace password_manager

@protocol ApplicationCommands;
class Browser;
class IOSChromePasswordCheckManager;
@class PasswordIssuesCoordinator;
@class ReauthenticationModule;

// Delegate for PasswordIssuesCoordinator.
@protocol PasswordIssuesCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator;

// Called when the user deleted password. Returns whether presenter will
// handle it or not.
- (BOOL)willHandlePasswordDeletion:
    (const password_manager::CredentialUIEntry&)credential;

@end

// This coordinator presents a list of compromised credentials for the user.
@interface PasswordIssuesCoordinator : ChromeCoordinator

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                            passwordCheckManager:
                                (IOSChromePasswordCheckManager*)manager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Reauthentication module used by password details coordinator.
@property(nonatomic, strong) ReauthenticationModule* reauthModule;

@property(nonatomic, weak) id<PasswordIssuesCoordinatorDelegate> delegate;

@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_COORDINATOR_H_
