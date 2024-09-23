// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_DELEGATE_H_

// Interface for updating the mediator in response to changes in the Password
// Settings UI.
@protocol PasswordSettingsDelegate

// Indicates whether or not "Offer to Save Passwords" is set to enabled.
- (void)savedPasswordSwitchDidChange:(BOOL)enabled;

// Indicates that the bulk move passwords to account button was clicked.
- (void)bulkMovePasswordsToAccountButtonClicked;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_DELEGATE_H_
