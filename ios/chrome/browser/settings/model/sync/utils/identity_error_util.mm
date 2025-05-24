// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"

#import "components/signin/public/base/signin_switches.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Gets the AccountErrorUIInfo data representing the kSignInNeedsUpdate error.
AccountErrorUIInfo* GetUIInfoForAuthenticationError() {
  AccountErrorUIInfo* error_info = [[AccountErrorUIInfo alloc]
       initWithErrorType:syncer::SyncService::UserActionableError::
                             kSignInNeedsUpdate
      userActionableType:AccountErrorUserActionableType::
                             kReauthToResolveSigninError
               messageID:IDS_IOS_ACCOUNT_TABLE_ERROR_VERIFY_ITS_YOU_MESSAGE
           buttonLabelID:IDS_IOS_ACCOUNT_TABLE_ERROR_VERIFY_ITS_YOU_BUTTON];

  return error_info;
}

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

  switch (sync_service->GetUserActionableError()) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      if (base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError)) {
        return GetUIInfoForAuthenticationError();
      }
      break;
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
      break;
  }

  return nil;
}
