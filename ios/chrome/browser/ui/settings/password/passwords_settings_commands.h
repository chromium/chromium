// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_SETTINGS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_SETTINGS_COMMANDS_H_

#import <Foundation/Foundation.h>

namespace password_manager {
struct CredentialUIEntry;
}

// Commands relative to the passwords in the Settings.
@protocol PasswordsSettingsCommands <NSObject>

// Shows the screen with password issues.
- (void)showCompromisedPasswords;

// Shows passwords details for blocked passwords.
- (void)showDetailedViewForCredential:
    (const password_manager::CredentialUIEntry&)credential;

// Shows passwords details for saved passwords.
- (void)showDetailedViewForAffiliatedGroup:
    (const password_manager::AffiliatedGroup&)affiliatedGroup;

// Shows form to manually enter new password credentials.
- (void)showAddPasswordSheet;

// Shows promotional view for Passwords In Other Apps.
- (void)showPasswordsInOtherAppsPromo;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_SETTINGS_COMMANDS_H_
