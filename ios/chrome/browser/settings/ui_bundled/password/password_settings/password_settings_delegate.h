// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_DELEGATE_H_

// Interface for updating the mediator in response to changes in the Password
// Settings UI.
@protocol PasswordSettingsDelegate

// Indicates whether or not "Offer to save passwords and passkeys" is set to
// enabled.
- (void)savedPasswordSwitchDidChange:(BOOL)enabled;

// Indicates that the app was set as a credential provider through an in-app
// prompt.
- (void)passwordAutoFillWasTurnedOn;

// Indicates that the bulk move passwords to account button was clicked.
- (void)bulkMovePasswordsToAccountButtonClicked;

// Indicates whether or not "Allow automatic passkey upgrades" is set to
// enabled.
- (void)automaticPasskeyUpgradesSwitchDidChange:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_DELEGATE_H_
