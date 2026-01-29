// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SYNC_PRESENTER_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SYNC_PRESENTER_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

namespace trusted_vault {
enum class TrustedVaultUserActionTriggerForUMA;
}  // namespace trusted_vault

// Called when the presented UI is dismissed, irrespective of whether it was
// interrupted, cancelled or successful.
using SyncPresenterCompletionCallback = ProceduralBlock;

// Protocol used to display sync-related UI.
@protocol SyncPresenterCommands

// Asks the presenter to display the reauthenticate the primary account.
// The primary should be available.
- (void)showPrimaryAccountReauth;

// Asks the presenter to display the reauthenticate the primary account.
// The primary should be available.
// `completion` is executed after the UI is dismissed.
- (void)showPrimaryAccountReauthWithDismissalCompletion:
    (SyncPresenterCompletionCallback)completion;

// Asks the presenter to display the sync encryption passphrase UI.
- (void)showSyncPassphraseSettings;

// Presents the Google services settings.
- (void)showGoogleServicesSettings;

// Presents the Account settings.
- (void)showAccountSettings;

// Presents the Trusted Vault reauthentication dialog, for sync security domain
// id.
// `trigger` UI elements where the trusted vault reauth has been triggered.
// `completion` is executed after the UI is dismissed.
- (void)showTrustedVaultReauthForFetchKeysWithTrigger:
            (trusted_vault::TrustedVaultUserActionTriggerForUMA)trigger
                                           completion:
                                               (SyncPresenterCompletionCallback)
                                                   completion;

// Presents the Trusted Vault degraded recoverability dialog (to enroll
// additional recovery factors), for sync security domain id.
// `trigger` UI elements where the trusted vault reauth has been triggered.
// `completion` is executed after the UI is dismissed.
- (void)
    showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
        (trusted_vault::TrustedVaultUserActionTriggerForUMA)trigger
                                                    completion:
                                                        (SyncPresenterCompletionCallback)
                                                            completion;
;

// Presents the help center article for the bookmarks limit exceeded error.
- (void)showBookmarksLimitExceededHelp;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SYNC_PRESENTER_COMMANDS_H_
