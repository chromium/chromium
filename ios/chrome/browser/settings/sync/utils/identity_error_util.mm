// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/sync/utils/identity_error_util.h"

#import "base/feature_list.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/settings/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/sync/utils/sync_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Gets the AccountErrorUIInfo data representing the kEnterPassphrase error.
AccountErrorUIInfo* GetUIInfoForPassphraseError() {
  AccountErrorUIInfo* error_info = [[AccountErrorUIInfo alloc]
       initWithErrorType:syncer::SyncService::UserActionableError::
                             kNeedsPassphrase
      userActionableType:AccountErrorUserActionableType::kEnterPassphrase
               messageID:IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_MESSAGE
           buttonLabelID:IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON];

  return error_info;
}

// Gets the AccountErrorUIInfo data representing the
// kNeedsTrustedVaultKeyForPasswords error.
AccountErrorUIInfo* GetUIInfoForTrustedVaultKeyErrorForPasswords() {
  AccountErrorUIInfo* errorInfo = [[AccountErrorUIInfo alloc]
       initWithErrorType:syncer::SyncService::UserActionableError::
                             kNeedsTrustedVaultKeyForPasswords
      userActionableType:AccountErrorUserActionableType::kReauthForFetchKeys
               messageID:
                   IDS_IOS_ACCOUNT_TABLE_ERROR_NEEDS_TRUSTED_VAULT_KEY_FOR_PASSWORDS_MESSAGE
           buttonLabelID:IDS_IOS_ACCOUNT_TABLE_ERROR_VERIFY_ITS_YOU_BUTTON];
  return errorInfo;
}

// Gets the AccountErrorUIInfo data representing the
// kNeedsTrustedVaultKeyForEverything error.
AccountErrorUIInfo* GetUIInfoForTrustedVaultKeyErrorForEverything() {
  AccountErrorUIInfo* errorInfo = [[AccountErrorUIInfo alloc]
       initWithErrorType:syncer::SyncService::UserActionableError::
                             kNeedsTrustedVaultKeyForEverything
      userActionableType:AccountErrorUserActionableType::kReauthForFetchKeys
               messageID:
                   IDS_IOS_ACCOUNT_TABLE_ERROR_NEEDS_TRUSTED_VAULT_KEY_FOR_EVERYTHING_MESSAGE
           buttonLabelID:IDS_IOS_ACCOUNT_TABLE_ERROR_VERIFY_ITS_YOU_BUTTON];
  return errorInfo;
}

// Gets the AccountErrorUIInfo data representing the
// kTrustedVaultRecoverabilityDegradedForPasswords error.
AccountErrorUIInfo*
GetUIInfoForTrustedVaultRecoverabilityDegradedErrorForPasswords() {
  AccountErrorUIInfo* errorInfo = [[AccountErrorUIInfo alloc]
       initWithErrorType:syncer::SyncService::UserActionableError::
                             kTrustedVaultRecoverabilityDegradedForPasswords
      userActionableType:AccountErrorUserActionableType::
                             kReauthForDegradedRecoverability
               messageID:
                   IDS_IOS_ACCOUNT_TABLE_ERROR_HAS_TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_MESSAGE
           buttonLabelID:IDS_IOS_ACCOUNT_TABLE_ERROR_VERIFY_ITS_YOU_BUTTON];

  return errorInfo;
}

// Gets the AccountErrorUIInfo data representing the
// kTrustedVaultRecoverabilityDegradedForEverything error.
AccountErrorUIInfo*
GetUIInfoForTrustedVaultRecoverabilityDegradedErrorForEverything() {
  AccountErrorUIInfo* errorInfo = [[AccountErrorUIInfo alloc]
       initWithErrorType:syncer::SyncService::UserActionableError::
                             kTrustedVaultRecoverabilityDegradedForEverything
      userActionableType:AccountErrorUserActionableType::
                             kReauthForDegradedRecoverability
               messageID:
                   IDS_IOS_ACCOUNT_TABLE_ERROR_HAS_TRUSTED_VAULT_RECOVERABILITY_DEGRADED_FOR_EVERYTHING_MESSAGE
           buttonLabelID:IDS_IOS_ACCOUNT_TABLE_ERROR_VERIFY_ITS_YOU_BUTTON];

  return errorInfo;
}

}  // namespace

AccountErrorUIInfo* GetAccountErrorUIInfo(syncer::SyncService* sync_service) {
  DCHECK(sync_service);

  if (sync_service->IsSyncFeatureEnabled()) {
    // Don't indicate account errors when Sync is enabled.
    return nil;
  }

  switch (sync_service->GetUserActionableError()) {
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return GetUIInfoForPassphraseError();
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      return GetUIInfoForTrustedVaultKeyErrorForPasswords();
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return GetUIInfoForTrustedVaultKeyErrorForEverything();
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      return GetUIInfoForTrustedVaultRecoverabilityDegradedErrorForPasswords();
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return GetUIInfoForTrustedVaultRecoverabilityDegradedErrorForEverything();
    case syncer::SyncService::UserActionableError::kNone:
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
    case syncer::SyncService::UserActionableError::kGenericUnrecoverableError:
      break;
  }

  return nil;
}

SyncState GetSyncState(syncer::SyncService* sync_service) {
  syncer::SyncService::UserActionableError error_state =
      sync_service->GetUserActionableError();
  if (sync_service->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    // Sync is disabled by administrator policy.
    return SyncState::kSyncDisabledByAdministrator;
  } else if (!sync_service->GetUserSettings()
                  ->IsInitialSyncFeatureSetupComplete()) {
    // User has not completed Sync setup in sign-in flow.
    return SyncState::kSyncConsentOff;
  } else if (!sync_service->CanSyncFeatureStart()) {
    // Sync engine is off.
    return SyncState::kSyncOff;
  } else if (sync_service->GetUserSettings()->GetSelectedTypes().Empty()) {
    // User has deselected all sync data types.
    // With pre-MICE, the sync status should be SyncState::kSyncEnabled to show
    // the same value than the sync toggle.
    return SyncState::kSyncEnabledWithNoSelectedTypes;
  } else if (error_state != syncer::SyncService::UserActionableError::kNone) {
    // Sync error.
    return SyncState::kSyncEnabledWithError;
  }
  return SyncState::kSyncEnabled;
}

bool ShouldIndicateIdentityErrorInOverflowMenu(
    syncer::SyncService* sync_service) {
  DCHECK(sync_service);

  return GetAccountErrorUIInfo(sync_service) != nil ||
         GetSyncState(sync_service) == SyncState::kSyncEnabledWithError;
}
