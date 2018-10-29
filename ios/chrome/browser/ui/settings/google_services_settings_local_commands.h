// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_LOCAL_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_LOCAL_COMMANDS_H_

// Protocol to communicate GoogleServicesSettingsVC actions to its coordinator.
@protocol GoogleServicesSettingsLocalCommands<NSObject>

// Restarts the authentication flow.
- (void)restartAuthenticationFlow;

// Opens the reauth sync dialog.
- (void)openReauthDialogAsSyncIsInAuthError;

// Opens the passphrase dialog.
- (void)openPassphraseDialog;

// Opens the "Google Activity Controls" dialog.
- (void)openGoogleActivityControlsDialog;

// Opens the "Encryption" dialog.
- (void)openEncryptionDialog;

// Opens the "Manage Synced Data" web page.
- (void)openManageSyncedDataWebPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_LOCAL_COMMANDS_H_
