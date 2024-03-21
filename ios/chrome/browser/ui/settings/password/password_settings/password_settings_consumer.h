// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSUMER_H_

// State of on-device encryption.
typedef NS_ENUM(NSInteger, PasswordSettingsOnDeviceEncryptionState) {
  // User can not opt-in in their current state, so the section should not be
  // shown.
  PasswordSettingsOnDeviceEncryptionStateNotShown = 0,
  // On-device encryption is on.
  PasswordSettingsOnDeviceEncryptionStateOptedIn,
  // User can opt-in to on-device encryption.
  PasswordSettingsOnDeviceEncryptionStateOfferOptIn,
};

// Interface for passing state from the mediator to the ViewController showing
// the password settings submenu.
@protocol PasswordSettingsConsumer

// Indicates whether the export flow can be started. Should be NO when an
// export is already in progress, and YES when idle.
- (void)setCanExportPasswords:(BOOL)canExportPasswords;

// Indicates whether or not the Password Manager is managed by enterprise
// policy.
- (void)setManagedByPolicy:(BOOL)managedByPolicy;

// Indicates whether or not the "Offer to Save Passwords" feature is enabled.
- (void)setSavePasswordsEnabled:(BOOL)enabled;

// The count of local passwords passed along with the user eligibility to see
// the move passwords to account section.
- (void)setLocalPasswordsCount:(int)count withUserEligibility:(BOOL)eligibility;

// Indicates the signed-in account.
- (void)setSignedInAccount:(NSString*)account;

// Indicates whether or not Chromium has been enabled as a credential provider
// at the iOS level.
- (void)setPasswordsInOtherAppsEnabled:(BOOL)enabled;

// Indicates the on-device encryption state according to the sync service.
- (void)setOnDeviceEncryptionState:
    (PasswordSettingsOnDeviceEncryptionState)onDeviceEncryptionState;

// Enables/disables the "Export Passwords..." button based on the current state.
- (void)updateExportPasswordsButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSUMER_H_
