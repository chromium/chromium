// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/browser/settings/manage_sync/public/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Records `account_menu_action_name` when `surface` is the Account Menu.
void MaybeRecordErrorButtonTapUserAction(
    syncer::IdentityErrorDisplaySurface surface,
    const char* account_menu_action_name) {
  switch (surface) {
    case syncer::IdentityErrorDisplaySurface::kAccountMenu:
      base::RecordAction(base::UserMetricsAction(account_menu_action_name));
      break;
    case syncer::IdentityErrorDisplaySurface::kSyncSettings:
      // TODO(crbug.com/576654541): record a UMA
      break;
  }
}

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

// Gets the AccountErrorUIInfo data representing the kBookmarksLimitExceeded
// error.
AccountErrorUIInfo* GetUIInfoForBookmarksLimitExceededError() {
  AccountErrorUIInfo* error_info = [[AccountErrorUIInfo alloc]
       initWithErrorType:syncer::SyncService::UserActionableError::
                             kBookmarksLimitExceeded
      userActionableType:AccountErrorUserActionableType::
                             kAcknowledgeBookmarkError
               messageID:IDS_IOS_SYNC_ERROR_BOOKMARKS_LIMIT_EXCEEDED_MESSAGE
           buttonLabelID:IDS_IOS_SYNC_ERROR_BOOKMARKS_LIMIT_EXCEEDED_BUTTON];

  return error_info;
}

// Gets the AccountErrorUIInfo data representing the DEVICE_MANAGEMENT_ERROR.
AccountErrorUIInfo* GetUIInfoForDeviceManagementError(
    const syncer::SyncService* sync_service) {
  GoogleServiceAuthError error = sync_service->GetAuthError();
  CHECK_EQ(error.state(),
           GoogleServiceAuthError::State::DEVICE_MANAGEMENT_ERROR);
  int buttonLabelID =
      error.IsDeviceManagementErrorUserActionable()
          ? IDS_IOS_IDENTITY_ERROR_INFOBAR_DEVICE_MANAGEMENT_REQUIRED_FIX_NOW_BUTTON
          : IDS_IOS_IDENTITY_ERROR_INFOBAR_DEVICE_MANAGEMENT_REQUIRED_LEARN_MORE_BUTTON;
  AccountErrorUIInfo* error_info = [[AccountErrorUIInfo alloc]
       initWithErrorType:syncer::SyncService::UserActionableError::
                             kDeviceManagementError
      userActionableType:AccountErrorUserActionableType::kResolveMdmError
               messageID:
                   IDS_IOS_IDENTITY_ERROR_INFOBAR_DEVICE_MANAGEMENT_SERVICES_UNAVAILABLE_MESSAGE
           buttonLabelID:buttonLabelID];
  return error_info;
}

}  // namespace

AccountErrorUIInfo* GetAccountErrorUIInfo(syncer::SyncService* sync_service) {
  DCHECK(sync_service);

  switch (sync_service->GetUserActionableError()) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return GetUIInfoForAuthenticationError();
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
    case syncer::SyncService::UserActionableError::kBookmarksLimitExceeded:
      return GetUIInfoForBookmarksLimitExceededError();
    case syncer::SyncService::UserActionableError::kDeviceManagementError:
      return GetUIInfoForDeviceManagementError(sync_service);
    case syncer::SyncService::UserActionableError::kNone:
      break;

    case syncer::SyncService::UserActionableError::kNeedsClientUpgrade:
      // TODO(crbug.com/370026230): Implement UI support for this case.
      break;
  }

  return nil;
}

bool HandleIdentityErrorButtonTap(
    syncer::SyncService::UserActionableError error_type,
    AuthenticationService* authentication_service,
    id<SyncErrorSettingsCommandHandler> command_handler,
    syncer::IdentityErrorDisplaySurface surface) {
  CHECK(authentication_service);
  id<SystemIdentity> primary_identity =
      authentication_service->GetPrimaryIdentity();
  CHECK(primary_identity);

  switch (error_type) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate: {
      BOOL is_mdm_error = NO;
      if (!base::FeatureList::IsEnabled(
              switches::kHandleMdmErrorsForDasherAccounts)) {
        is_mdm_error = authentication_service->HasCachedMDMErrorForIdentity(
            primary_identity);
      }
      if (!is_mdm_error) {
        MaybeRecordErrorButtonTapUserAction(
            surface, "Signin_AccountMenu_ErrorButton_Reauth");
        [command_handler openPrimaryAccountReauthDialog];
      } else {
        MaybeRecordErrorButtonTapUserAction(
            surface, "Signin_AccountMenu_ErrorButton_MDM");
        [command_handler openMDMErrorDialogWithSystemIdentity:primary_identity];
      }
      return true;
    }
    case syncer::SyncService::UserActionableError::kDeviceManagementError:
      MaybeRecordErrorButtonTapUserAction(surface,
                                          "Signin_AccountMenu_ErrorButton_MDM");
      [command_handler openMDMErrorDialogWithSystemIdentity:primary_identity];
      return true;
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      MaybeRecordErrorButtonTapUserAction(
          surface, "Signin_AccountMenu_ErrorButton_Passphrase");
      [command_handler openPassphraseDialogWithModalPresentation:YES];
      return true;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      MaybeRecordErrorButtonTapUserAction(
          surface, "Signin_AccountMenu_ErrorButton_TrustedVaultForPasswords");
      [command_handler openTrustedVaultReauthForFetchKeys];
      return false;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      MaybeRecordErrorButtonTapUserAction(
          surface, "Signin_AccountMenu_ErrorButton_TrustedVaultForEverything");
      [command_handler openTrustedVaultReauthForFetchKeys];
      return false;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      MaybeRecordErrorButtonTapUserAction(
          surface,
          "Signin_AccountMenu_ErrorButton_TrustedVaultDegradedForPasswords");
      [command_handler openTrustedVaultReauthForDegradedRecoverability];
      return false;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      MaybeRecordErrorButtonTapUserAction(
          surface,
          "Signin_AccountMenu_ErrorButton_TrustedVaultDegradedForEverything");
      [command_handler openTrustedVaultReauthForDegradedRecoverability];
      return false;
    case syncer::SyncService::UserActionableError::kBookmarksLimitExceeded:
      MaybeRecordErrorButtonTapUserAction(
          surface, "Signin_AccountMenu_ErrorButton_BookmarksLimitExceeded");
      [command_handler openBookmarksLimitExceededHelp];
      return false;
    case syncer::SyncService::UserActionableError::kNone:
    // TODO(crbug.com/370026230): Update this case once GetAccountErrorUIInfo()
    // returns a non-nil value for it.
    case syncer::SyncService::UserActionableError::kNeedsClientUpgrade:
      NOTREACHED();
  }
}
