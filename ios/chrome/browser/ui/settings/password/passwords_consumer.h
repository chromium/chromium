// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <vector>

#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"

namespace password_manager {
struct CredentialUIEntry;
class AffiliatedGroup;
}  // namespace password_manager

// Consumer for the Passwords Screen.
@protocol PasswordsConsumer <NSObject>

// Displays current password check UI state on screen for insecure credentials.
- (void)setPasswordCheckUIState:(PasswordCheckUIState)state
         insecurePasswordsCount:(NSInteger)insecureCount;

// Displays password and blocked forms.
// TODO(crbug.com/1359392): Remove this.
- (void)setPasswords:(std::vector<password_manager::CredentialUIEntry>)passwords
        blockedSites:
            (std::vector<password_manager::CredentialUIEntry>)blockedSites;

// Updates whether passwords are being saved to an account or only locally.
- (void)setSavingPasswordsToAccount:(BOOL)savingPasswordsToAccount;

// Displays affiliated groups for the Password Manager.
// This method relates to the -setPasswords method above. This will eventually
// replace it when the feature is done.
- (void)setAffiliatedGroups:
            (const std::vector<password_manager::AffiliatedGroup>&)
                affiliatedGroups
               blockedSites:
                   (const std::vector<password_manager::CredentialUIEntry>&)
                       blockedSites;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_CONSUMER_H_
