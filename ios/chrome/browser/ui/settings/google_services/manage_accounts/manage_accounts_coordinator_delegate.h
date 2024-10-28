// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
@class ManageAccountsCoordinator;

@protocol ManageAccountsCoordinatorDelegate <NSObject>

// Requests the delegate to stop the manage accounts coordinator.
- (void)manageAccountsCoordinatorWantsToBeStopped:
    (ManageAccountsCoordinator*)coordinator;

@optional

// Asks the delegate to handle offering the user to add a new account.
// It must be implemented when a signin coordinator is already displayed by the
// scene controller.
- (void)manageAccountsCoordinator:
            (ManageAccountsCoordinator*)manageAccountsCoordinator
    didRequestAddAccountWithBaseViewController:(UIViewController*)viewController
                                    completion:
                                        (ShowSigninCommandCompletionCallback)
                                            completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_COORDINATOR_DELEGATE_H_
