// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_COORDINATOR_DELEGATE_H_

#import "ios/chrome/browser/ui/settings/password/reauthentication/password_manager_reauthentication_delegate.h"

@class PasswordSettingsCoordinator;

// Delegate for PasswordSettingsCoordinator.
@protocol PasswordSettingsCoordinatorDelegate <
    PasswordManagerReauthenticationDelegate>

// Called when the view controller was removed from navigation controller.
- (void)passwordSettingsCoordinatorDidRemove:
    (PasswordSettingsCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_COORDINATOR_DELEGATE_H_
