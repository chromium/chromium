// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_error_infobar_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kSyncErrorInfobarDisplayedHistogramName[] =
    "Sync.SyncErrorInfobarDisplayed2";

// Enumerated constants for logging when a sign-in error infobar was shown
// to the user. This was added for crbug/265352 to quantify how often this
// bug shows up in the wild. The logged histogram count should be interpreted
// as a ratio of the number of active sync users.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SyncErrorInfobarTypes)
enum InfobarSyncError : uint8_t {
  SYNC_SIGN_IN_NEEDS_UPDATE = 1,
  // DEPRECATED. No longer recorded.
  // SYNC_SERVICE_UNAVAILABLE = 2
  SYNC_NEEDS_PASSPHRASE = 3,
  // SYNC_UNRECOVERABLE_ERROR = 4, (deprecated)
  SYNC_SYNC_SETTINGS_NOT_CONFIRMED = 5,
  SYNC_NEEDS_TRUSTED_VAULT_KEY = 6,
  SYNC_TRUSTED_VAULT_RECOVERABILITY_DEGRADED = 7,
  kMaxValue = SYNC_TRUSTED_VAULT_RECOVERABILITY_DEGRADED,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncErrorInfobarTypes)

// Converts `syncer::SyncService::UserActionableError` to `InfobarSyncError`.
// Returns `std::nullopt` when the error is `kNone`.
std::optional<InfobarSyncError> InfobarSyncErrorFromUserActionableError(
    syncer::SyncService::UserActionableError error) {
  switch (error) {
    case syncer::SyncService::UserActionableError::kNone:
      return std::nullopt;
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return SYNC_SIGN_IN_NEEDS_UPDATE;
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return SYNC_NEEDS_PASSPHRASE;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return SYNC_NEEDS_TRUSTED_VAULT_KEY;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return SYNC_TRUSTED_VAULT_RECOVERABILITY_DEGRADED;
  }
  NOTREACHED();
}

// Gets the the title of the identity error info bar for the given `error`.
std::u16string GetIdentityErrorInfoBarTitle(
    syncer::SyncService::UserActionableError error) {
  switch (error) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return l10n_util::GetStringUTF16(
          IDS_IOS_IDENTITY_ERROR_INFOBAR_VERIFY_ITS_YOU_TITLE);
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return l10n_util::GetStringUTF16(
          IDS_IOS_IDENTITY_ERROR_INFOBAR_ENTER_PASSPHRASE_TITLE);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      return base::FeatureList::IsEnabled(
                 syncer::kSyncTrustedVaultInfobarMessageImprovements)
                 ? l10n_util::GetStringUTF16(
                       IDS_IOS_IDENTITY_ERROR_INFOBAR_GET_ALL_YOUR_PASSWORDS_TITLE)
                 : l10n_util::GetStringUTF16(
                       IDS_IOS_IDENTITY_ERROR_INFOBAR_VERIFY_ITS_YOU_TITLE);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return l10n_util::GetStringUTF16(
          IDS_IOS_IDENTITY_ERROR_INFOBAR_VERIFY_ITS_YOU_TITLE);
    case syncer::SyncService::UserActionableError::kNone:
      NOTREACHED();
  }
}

// Gets the message of the identity error info bar.
NSString* GetIdentityErrorInfoBarMessage(
    syncer::SyncService::UserActionableError error,
    const std::u16string& email) {
  switch (error) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return l10n_util::GetNSString(
          IDS_IOS_IDENTITY_ERROR_INFOBAR_KEEP_USING_YOUR_CHROME_DATA_MESSAGE);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords: {
      if (base::FeatureList::IsEnabled(
              syncer::kSyncTrustedVaultInfobarImprovements)) {
        return base::FeatureList::IsEnabled(
                   syncer::kSyncTrustedVaultInfobarMessageImprovements)
                   ? l10n_util::GetNSString(
                         IDS_IOS_IDENTITY_ERROR_INFOBAR_GET_ALL_YOUR_PASSWORDS_ON_THIS_DEVICE)
                   : l10n_util::GetNSStringF(
                         IDS_IOS_IDENTITY_ERROR_INFOBAR_KEEP_USING_PASSWORDS_MESSAGE_WITH_EMAIL,
                         email);
      } else {
        return base::FeatureList::IsEnabled(
                   syncer::kSyncTrustedVaultInfobarMessageImprovements)
                   ? l10n_util::GetNSString(
                         IDS_IOS_IDENTITY_ERROR_INFOBAR_GET_ALL_YOUR_PASSWORDS_ON_THIS_DEVICE)
                   : l10n_util::GetNSString(
                         IDS_IOS_IDENTITY_ERROR_INFOBAR_KEEP_USING_PASSWORDS_MESSAGE);
      }
    }
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      return l10n_util::GetNSString(
          IDS_IOS_IDENTITY_ERROR_INFOBAR_MAKE_SURE_YOU_CAN_ALWAYS_USE_PASSWORDS_MESSAGE);
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return l10n_util::GetNSString(
          IDS_IOS_IDENTITY_ERROR_INFOBAR_MAKE_SURE_YOU_CAN_ALWAYS_USE_CHROME_DATA_MESSAGE);
    case syncer::SyncService::UserActionableError::kNone:
      NOTREACHED();
  }
}

NSString* GetIdentityErrorInfoBarButtonLabel(
    syncer::SyncService::UserActionableError error) {
  switch (error) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_UPDATE_CREDENTIALS_BUTTON);
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return l10n_util::GetNSString(
          IDS_IOS_IDENTITY_ERROR_INFOBAR_ENTER_BUTTON_LABEL);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      return base::FeatureList::IsEnabled(
                 syncer::kSyncTrustedVaultInfobarMessageImprovements)
                 ? l10n_util::GetNSString(
                       IDS_IOS_IDENTITY_ERROR_INFOBAR_OKAY_BUTTON_LABEL)
                 : l10n_util::GetNSString(
                       IDS_IOS_IDENTITY_ERROR_INFOBAR_VERIFY_BUTTON_LABEL);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return l10n_util::GetNSString(
          IDS_IOS_IDENTITY_ERROR_INFOBAR_VERIFY_BUTTON_LABEL);
    case syncer::SyncService::UserActionableError::kNone:
      NOTREACHED();
  }
}

std::string GetSyncErrorInfobarHistogramSuffix(
    SyncErrorInfoBarTrigger trigger) {
  switch (trigger) {
    case SyncErrorInfoBarTrigger::kNewTabOpened:
      return ".NewTab";
    case SyncErrorInfoBarTrigger::kPasswordFormParsed:
      return ".PasswordForm";
  }
  NOTREACHED();
}

}  // namespace

NSString* GetSyncErrorDescriptionForSyncService(
    syncer::SyncService* syncService) {
  DCHECK(syncService);
  switch (syncService->GetUserActionableError()) {
    case syncer::SyncService::UserActionableError::kNone:
      return nil;
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_LOGIN_INFO_OUT_OF_DATE);
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_DESCRIPTION);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_DESCRIPTION);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      // The encryption error affects passwords only as per
      // syncer::AlwaysEncryptedUserTypes().
      return l10n_util::GetNSString(IDS_IOS_SYNC_PASSWORDS_ERROR_DESCRIPTION);
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return l10n_util::GetNSString(
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_FIX_RECOVERABILITY_DEGRADED_FOR_EVERYTHING);
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      // The encryption error affects passwords only as per
      // syncer::AlwaysEncryptedUserTypes().
      return l10n_util::GetNSString(
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_FIX_RECOVERABILITY_DEGRADED_FOR_PASSWORDS);
  }
}

std::u16string GetSyncErrorInfoBarTitleForProfile(ProfileIOS* profile) {
  DCHECK(profile);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  DCHECK(sync_service);

  if (GetAccountErrorUIInfo(sync_service) != nil) {
    return GetIdentityErrorInfoBarTitle(sync_service->GetUserActionableError());
  } else {
    // There is no title in Sync error info bar.
    return std::u16string();
  }
}

NSString* GetSyncErrorMessageForProfile(ProfileIOS* profile) {
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  DCHECK(syncService);

  const syncer::SyncService::UserActionableError error =
      syncService->GetUserActionableError();

  if (GetAccountErrorUIInfo(syncService) != nil) {
    return GetIdentityErrorInfoBarMessage(
        error, base::UTF8ToUTF16(syncService->GetAccountInfo().email));
  }

  switch (error) {
    case syncer::SyncService::UserActionableError::kNone:
      return nil;
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_INFO_OUT_OF_DATE);
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return l10n_util::GetNSString(IDS_IOS_SYNC_CONFIGURE_ENCRYPTION);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return GetSyncErrorDescriptionForSyncService(syncService);
  }
}

NSString* GetSyncErrorButtonTitleForProfile(ProfileIOS* profile) {
  DCHECK(profile);

  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  DCHECK(syncService);

  const syncer::SyncService::UserActionableError error =
      syncService->GetUserActionableError();

  if (GetAccountErrorUIInfo(syncService) != nil) {
    return GetIdentityErrorInfoBarButtonLabel(error);
  }

  switch (error) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_UPDATE_CREDENTIALS_BUTTON);
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ENTER_PASSPHRASE_BUTTON);
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return l10n_util::GetNSString(IDS_IOS_SYNC_VERIFY_ITS_YOU_BUTTON);
    case syncer::SyncService::UserActionableError::kNone:
      return nil;
  }
}

bool ShouldShowSyncSettings(syncer::SyncService::UserActionableError error) {
  switch (error) {
    case syncer::SyncService::UserActionableError::kNone:
      return true;
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return false;
  }
}

bool DisplaySyncErrors(ProfileIOS* profile,
                       web::WebState* web_state,
                       id<SyncPresenter> presenter,
                       SyncErrorInfoBarTrigger trigger) {
  // Avoid displaying sync errors on incognito tabs.
  if (profile->IsOffTheRecord()) {
    return false;
  }

  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  if (!syncService) {
    return false;
  }

  if (GetAccountErrorUIInfo(syncService) == nil) {
    // If the identity error info bar isn't used, fallback to the Sync error
    // info bar.

    // Avoid showing the sync error info bar when sync changes are still
    // pending. This is particularely requires during first run when the
    // advanced sign-in settings are being presented on the NTP before sync
    // changes being committed.
    if (syncService->IsSetupInProgress()) {
      return false;
    }

    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(profile);
    if (!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      return false;
    }
  }

  std::optional<InfobarSyncError> infobarSyncError =
      InfobarSyncErrorFromUserActionableError(
          syncService->GetUserActionableError());
  if (!infobarSyncError.has_value()) {
    return false;
  }

  DCHECK(web_state);
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(web_state);
  DCHECK(infoBarManager);
  bool infobar_displayed = SyncErrorInfoBarDelegate::Create(
      infoBarManager, profile, presenter, trigger);
  if (infobar_displayed) {
    // Logs when an infobar is shown to user. See crbug.com/265352.
    base::UmaHistogramEnumeration(kSyncErrorInfobarDisplayedHistogramName,
                                  *infobarSyncError);
    base::UmaHistogramEnumeration(
        base::StrCat({kSyncErrorInfobarDisplayedHistogramName,
                      GetSyncErrorInfobarHistogramSuffix(trigger)}),
        *infobarSyncError);
  }
  return infobar_displayed;
}

void LogSyncErrorInfobarDismissed(
    syncer::SyncService::UserActionableError error) {
  std::optional<InfobarSyncError> infobarSyncError =
      InfobarSyncErrorFromUserActionableError(error);
  if (infobarSyncError.has_value()) {
    base::UmaHistogramEnumeration("Sync.SyncErrorInfobarDismissed",
                                  *infobarSyncError);
  }
}
