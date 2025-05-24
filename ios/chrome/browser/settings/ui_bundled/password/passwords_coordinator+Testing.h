// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORDS_COORDINATOR_TESTING_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORDS_COORDINATOR_TESTING_H_

@class TrustedVaultReauthenticationCoordinator;

// Testing category exposing a private property of PasswordsCoordinator for
// tests.
// TODO(crbug.com/417667093): Remove this after adding EarlGrey tests of the
// Trusted Vault GPM management UI widget.
@interface PasswordsCoordinator (Testing)

// Returns a coordinator that displays the Trusted Vault reauthentication
// dialog.
- (TrustedVaultReauthenticationCoordinator*)
    trustedVaultReauthenticationCoordinator;

// Sets a coordinator for displaying the Trusted Vault reauthentication
// dialog.
- (void)setTrustedVaultReauthenticationCoordinator:
    (TrustedVaultReauthenticationCoordinator*)
        trustedVaultReauthenticationCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PASSWORD_PASSWORDS_COORDINATOR_TESTING_H_
