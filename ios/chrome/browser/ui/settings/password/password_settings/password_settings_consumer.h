// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSUMER_H_

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

// Enables/disables the "Export Passwords..." button based on the current state.
- (void)updateExportPasswordsButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SETTINGS_PASSWORD_SETTINGS_CONSUMER_H_
