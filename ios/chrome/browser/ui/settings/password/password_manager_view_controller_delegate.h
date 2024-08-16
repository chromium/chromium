// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

#include <vector>

namespace password_manager {
class AffiliatedGroup;
struct CredentialUIEntry;
}  // namespace password_manager

// State of on-device encryption used for
// ItemTypeOnDeviceEncryptionOptInDescription, ItemTypeOnDeviceEncryptionSetUp
// and ItemTypeOnDeviceEncryptionSetUp.
typedef NS_ENUM(NSInteger, OnDeviceEncryptionState) {
  // On device encryption is on.
  // ItemTypeOnDeviceEncryptionOptInDescription is shown.
  OnDeviceEncryptionStateOptedIn,
  // User can opt-in on device encryption.
  // ItemTypeOnDeviceEncryptionOptInDescription and
  // ItemTypeOnDeviceEncryptionSetUp are shown.
  OnDeviceEncryptionStateOfferOptIn,
  // User can not opt-in in their current state.
  // Currently it is either because:
  // * User is not signed-in,
  // * User hasn’t opted in to or disabled Sync for passwords (or equivalent
  // enterprise policies),
  // * User has a custom passphrase.
  // SectionIdentifierOnDeviceEncryption is hidden.
  OnDeviceEncryptionStateNotShown,
};

namespace password_manager {
struct CredentialUIEntry;
}

// Delegate for `PasswordManagerViewController`.
@protocol PasswordManagerViewControllerDelegate

// Deletes credentials from the store.
- (void)deleteCredentials:
    (const std::vector<password_manager::CredentialUIEntry>&)credentials;

// Starts password check.
- (void)startPasswordCheck;

// Returns string containing the timestamp of the last password check. If the
// check finished less than 1 minute ago string will look "Last check just
// now", otherwise "Last check X minutes/hours... ago.". If check never run,
// string will be "Check never run".
- (NSString*)formattedElapsedTimeSinceLastCheck;

// Returns detailed information about Password Check error if applicable.
- (NSAttributedString*)passwordCheckErrorInfo;

// Returns the on-device encryption state according to the sync service.
- (OnDeviceEncryptionState)onDeviceEncryptionState;

// Returns whether a special icon should be shown next to `credential` that
// indicates it's not backed up to any account.
- (BOOL)shouldShowLocalOnlyIconForCredential:
    (const password_manager::CredentialUIEntry&)credential;

// Similar to above but for an affiliated group.
- (BOOL)shouldShowLocalOnlyIconForGroup:
    (const password_manager::AffiliatedGroup&)group;

// Tells the delegate that the user has dismissed the Password Manager widget
// promo. Used to notify the Feature Engagement Tracker of the dismissal.
- (void)notifyFETOfPasswordManagerWidgetPromoDismissal;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_MANAGER_VIEW_CONTROLLER_DELEGATE_H_
