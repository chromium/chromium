// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
@class ManageAccountsCoordinator;

@protocol ManageAccountsCoordinatorDelegate <NSObject>

// Requests the delegate to stop the manage accounts coordinator.
- (void)manageAccountsCoordinatorWantsToBeStopped:
    (ManageAccountsCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_MANAGE_ACCOUNTS_COORDINATOR_DELEGATE_H_
