// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SYNC_ERROR_SETTINGS_COMMAND_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SYNC_ERROR_SETTINGS_COMMAND_HANDLER_H_

@protocol SystemIdentity;

// Protocol to communicate actions following Sync errors from the mediator to
// its coordinator.
@protocol SyncErrorSettingsCommandHandler <NSObject>

// Opens MDM error dialog. This method should be called when there is a MDM
// error.
- (void)openMDMErrodDialogWithSystemIdentity:(id<SystemIdentity>)identity;

// Opens the reauth dialog. This method should be called only when the primary
// account is available.
- (void)openPrimaryAccountReauthDialog;

// Opens the passphrase dialog.
- (void)openPassphraseDialogWithModalPresentation:(BOOL)presentModally;

// Opens the trusted vault reauthentication dialog for fetch keys, for Chrome
// Sync security domain.
- (void)openTrustedVaultReauthForFetchKeys;

// Opens the trusted vault reauthentication degraded recoverability dialog (to
// enroll additional recovery factors), for Chrome Sync security domain.
- (void)openTrustedVaultReauthForDegradedRecoverability;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SYNC_ERROR_SETTINGS_COMMAND_HANDLER_H_
