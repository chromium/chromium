// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSUMER_H_

// State of on-device encryption.
typedef NS_ENUM(NSInteger, PasswordSettingsOnDeviceEncryptionState) {
  // User can not opt-in in their current state.
  PasswordSettingsOnDeviceEncryptionStateNotShown = 0,
  // On-device encryption is on.
  PasswordSettingsOnDeviceEncryptionStateOptedIn,
  // User can opt-in to on-device encryption.
  PasswordSettingsOnDeviceEncryptionStateOfferOptIn,
};

// Interface for passing state from the mediator to the ViewController showing
// the password settings submenu.
@protocol PasswordSettingsConsumer

// User-modifiable preferences.

- (void)setSavingPasswordsEnabled:(BOOL)enabled
                  managedByPolicy:(BOOL)managedByPolicy;

- (void)setAutomaticPasskeyUpgradesEnabled:(BOOL)enabled;

// Read-only data.

- (void)setUserEmail:(NSString*)userEmail;

// This is enabled by default and can only be modified by an enterprise policy.
- (void)setSavingPasskeysEnabled:(BOOL)enabled;

// State machines triggering UI changes.

- (void)setCanChangeGPMPin:(BOOL)canChangeGPMPin;

- (void)setCanDeleteAllCredentials:(BOOL)canDeleteAllCredentials;

- (void)setCanExportPasswords:(BOOL)canExportPasswords;

- (void)setCanBulkMove:(BOOL)canBulkMove localPasswordsCount:(int)count;

- (void)setOnDeviceEncryptionState:
    (PasswordSettingsOnDeviceEncryptionState)onDeviceEncryptionState;

- (void)setPasswordsInOtherAppsEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSUMER_H_
