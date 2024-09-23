// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_PRESENTER_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_PRESENTER_H_

#import <Foundation/Foundation.h>

namespace syncer {
enum class TrustedVaultUserActionTriggerForUMA;
}  // namespace syncer

// Protocol used to display sync-related UI.
@protocol SyncPresenter

// Asks the presenter to display the reauthenticate the primary account.
// The primary should be available.
- (void)showPrimaryAccountReauth;

// Asks the presenter to display the sync encryption passphrase UI.
- (void)showSyncPassphraseSettings;

// Presents the Google services settings.
- (void)showGoogleServicesSettings;

// Presents the Account settings.
- (void)showAccountSettings;

// Presents the Trusted Vault reauthentication dialog, for sync security domain
// id. `trigger` UI elements where the trusted vault reauth has been triggered.
- (void)showTrustedVaultReauthForFetchKeysWithTrigger:
    (syncer::TrustedVaultUserActionTriggerForUMA)trigger;

// Presents the Trusted Vault degraded recoverability dialog (to enroll
// additional recovery factors), for sync security domain id.
// `trigger` UI elements where the trusted vault reauth has been triggered.
- (void)showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
    (syncer::TrustedVaultUserActionTriggerForUMA)trigger;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_PRESENTER_H_
