// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SYNC_ERROR_SETTINGS_COMMAND_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SYNC_ERROR_SETTINGS_COMMAND_HANDLER_H_

// Protocol to communicate actions following Sync errors from the mediator to
// its coordinator.
@protocol SyncErrorSettingsCommandHandler <NSObject>

// Opens the reauth sync dialog.
- (void)openReauthDialogAsSyncIsInAuthError;

// Opens the passphrase dialog.
- (void)openPassphraseDialog;

// Opens the trusted vault reauthentication dialog for fetch keys.
- (void)openTrustedVaultReauthForFetchKeys;

// Opens the trusted vault reauthentication degraded recoverability dialog (to
// enroll additional recovery factors).
- (void)openTrustedVaultReauthForDegradedRecoverability;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SYNC_ERROR_SETTINGS_COMMAND_HANDLER_H_
