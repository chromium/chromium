// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
@protocol ManageAccountsCoordinatorDelegate;

// Coordinator for the Accounts TableView Controller.
@interface ManageAccountsCoordinator : ChromeCoordinator

// If YES, ManageAccountsTableViewController should not dismiss itself only for
// a sign-out reason. The parent coordinator is responsible to dismiss this
// coordinator when a sign-out happens.
@property(nonatomic, assign) BOOL signoutDismissalByParentCoordinator;

// If YES, the view will offer a sign-out button.
@property(nonatomic, assign) BOOL showSignoutButton;

// The delegate for the coordinator.
@property(nonatomic, weak) id<ManageAccountsCoordinatorDelegate> delegate;

// Initializes ManageAccountsCoordinator to view its controller by pushing it on
// top of the navigation stack.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                       closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount
                                  showDoneButton:(BOOL)showDoneButton;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_COORDINATOR_H_
