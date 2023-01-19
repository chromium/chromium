// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <vector>

namespace password_manager {
struct CredentialUIEntry;
class AffiliatedGroup;
}  // namespace password_manager

// Enum with all possible UI states of password check.
typedef NS_ENUM(NSInteger, PasswordCheckUIState) {
  // When no insecure passwords were detected.
  PasswordCheckStateSafe,
  // When user has unmuted compromised passwords.
  PasswordCheckStateUnmutedCompromisedPasswords,
  // When user has reused passwords.
  PasswordCheckStateReusedPasswords,
  // When user has weak passwords.
  PasswordCheckStateWeakPasswords,
  // When user has dismissed warnings.
  PasswordCheckStateDismissedWarnings,
  // When check was not perfect and state is unclear.
  PasswordCheckStateDefault,
  // When password check is running.
  PasswordCheckStateRunning,
  // When user has no passwords and check can't be performed.
  PasswordCheckStateDisabled,
  // When password check failed due to network issues, quota limit or others.
  PasswordCheckStateError,
};

// Consumer for the Passwords Screen.
@protocol PasswordsConsumer <NSObject>

// Displays current password check UI state on screen for insecure credentials.
- (void)setPasswordCheckUIState:(PasswordCheckUIState)state
         insecurePasswordsCount:(NSInteger)count;

// Displays password and blocked forms.
// TODO(crbug.com/1359392): Remove this.
- (void)setPasswords:(std::vector<password_manager::CredentialUIEntry>)passwords
        blockedSites:
            (std::vector<password_manager::CredentialUIEntry>)blockedSites;

// Displays affiliated groups for the Password Manager.
// This method relates to the -setPasswords method above. This will eventually
// replace it when the feature is done.
- (void)setAffiliatedGroups:
            (const std::vector<password_manager::AffiliatedGroup>&)
                affiliatedGroups
               blockedSites:
                   (const std::vector<password_manager::CredentialUIEntry>&)
                       blockedSites;

// Updates "On/Off" state for Passwords In Other Apps item.
- (void)updatePasswordsInOtherAppsDetailedText;

// Updates "on-device encryption" related UI.
- (void)updateOnDeviceEncryptionSessionAndUpdateTableView;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_CONSUMER_H_
