// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"

#import "base/metrics/histogram_macros.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_error_infobar_delegate.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Enumerated constants for logging when a sign-in error infobar was shown
// to the user. This was added for crbug/265352 to quantify how often this
// bug shows up in the wild. The logged histogram count should be interpreted
// as a ratio of the number of active sync users.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum InfobarSyncError : uint8_t {
  SYNC_SIGN_IN_NEEDS_UPDATE = 1,
  // DEPRECATED. No longer recorded.
  // SYNC_SERVICE_UNAVAILABLE = 2
  SYNC_NEEDS_PASSPHRASE = 3,
  SYNC_UNRECOVERABLE_ERROR = 4,
  SYNC_SYNC_SETTINGS_NOT_CONFIRMED = 5,
  SYNC_NEEDS_TRUSTED_VAULT_KEY = 6,
  SYNC_TRUSTED_VAULT_RECOVERABILITY_DEGRADED = 7,
  kMaxValue = SYNC_TRUSTED_VAULT_RECOVERABILITY_DEGRADED,
};

}  // namespace

NSString* GetSyncErrorDescriptionForSyncSetupService(
    SyncSetupService* syncSetupService) {
  DCHECK(syncSetupService);
  SyncSetupService::SyncServiceState syncState =
      syncSetupService->GetSyncServiceState();
  switch (syncState) {
    case SyncSetupService::kNoSyncServiceError:
      return nil;
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_LOGIN_INFO_OUT_OF_DATE);
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_DESCRIPTION);
    case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
      if (syncSetupService->IsEncryptEverythingEnabled())
        return l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_DESCRIPTION);
      // The encryption error affects passwords only as per
      // syncer::AlwaysEncryptedUserTypes().
      return l10n_util::GetNSString(IDS_IOS_SYNC_PASSWORDS_ERROR_DESCRIPTION);
    case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
      if (syncSetupService->IsEncryptEverythingEnabled())
        return l10n_util::GetNSString(
            IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_FIX_RECOVERABILITY_DEGRADED_FOR_EVERYTHING);
      // The encryption error affects passwords only as per
      // syncer::AlwaysEncryptedUserTypes().
      return l10n_util::GetNSString(
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_SYNC_FIX_RECOVERABILITY_DEGRADED_FOR_PASSWORDS);
    case SyncSetupService::kSyncServiceServiceUnavailable:
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return l10n_util::GetNSString(IDS_IOS_SYNC_STATUS_UNRECOVERABLE_ERROR);
  }
}

NSString* GetSyncErrorMessageForBrowserState(ChromeBrowserState* browserState) {
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  DCHECK(syncSetupService);
  SyncSetupService::SyncServiceState syncState =
      syncSetupService->GetSyncServiceState();
  switch (syncState) {
    case SyncSetupService::kNoSyncServiceError:
      return nil;
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_INFO_OUT_OF_DATE);
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      return l10n_util::GetNSString(IDS_IOS_SYNC_CONFIGURE_ENCRYPTION);
    case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
    case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
      return GetSyncErrorDescriptionForSyncSetupService(syncSetupService);
    case SyncSetupService::kSyncServiceServiceUnavailable:
      return l10n_util::GetNSString(IDS_SYNC_SERVICE_UNAVAILABLE);
    case SyncSetupService::kSyncServiceCouldNotConnect:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_COULD_NOT_CONNECT);
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ERROR_UNRECOVERABLE);
  }
}

NSString* GetSyncErrorButtonTitleForBrowserState(
    ChromeBrowserState* browserState) {
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  DCHECK(syncSetupService);
  SyncSetupService::SyncServiceState syncState =
      syncSetupService->GetSyncServiceState();
  switch (syncState) {
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      return l10n_util::GetNSString(IDS_IOS_SYNC_UPDATE_CREDENTIALS_BUTTON);
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      return l10n_util::GetNSString(IDS_IOS_SYNC_ENTER_PASSPHRASE_BUTTON);
    case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
    case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
      return l10n_util::GetNSString(IDS_IOS_SYNC_VERIFY_ITS_YOU_BUTTON);
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return l10n_util::GetNSString(IDS_IOS_SYNC_SIGN_IN_AGAIN_BUTTON);
    case SyncSetupService::kNoSyncServiceError:
    case SyncSetupService::kSyncServiceServiceUnavailable:
    case SyncSetupService::kSyncServiceCouldNotConnect:
      return nil;
  }
}

SyncSetupService::SyncServiceState GetSyncStateForBrowserState(
    ChromeBrowserState* browserState) {
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  DCHECK(syncSetupService);
  return syncSetupService->GetSyncServiceState();
}

bool ShouldShowSyncSettings(SyncSetupService::SyncServiceState syncState) {
  switch (syncState) {
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
    case SyncSetupService::kSyncServiceUnrecoverableError:
    case SyncSetupService::kNoSyncServiceError:
      return true;
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
    case SyncSetupService::kSyncServiceNeedsPassphrase:
    case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
    case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
      return false;
  }
}

bool DisplaySyncErrors(ChromeBrowserState* browser_state,
                       web::WebState* web_state,
                       id<SyncPresenter> presenter) {
  // Avoid displaying sync errors on incognito tabs.
  if (browser_state->IsOffTheRecord())
    return false;

  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browser_state);
  if (!syncSetupService)
    return false;

  // Avoid showing the sync error inforbar when sync changes are still pending.
  // This is particularely requires during first run when the advanced sign-in
  // settings are being presented on the NTP before sync changes being
  // committed.
  if (syncSetupService->HasUncommittedChanges())
    return false;

  SyncSetupService::SyncServiceState errorState =
      syncSetupService->GetSyncServiceState();
  if (IsTransientSyncError(errorState))
    return false;

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  if (!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync))
    return false;
  // Logs when an infobar is shown to user. See crbug/265352.
  InfobarSyncError loggedErrorState;
  switch (errorState) {
    case SyncSetupService::kNoSyncServiceError:
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
      loggedErrorState = kMaxValue;
      NOTREACHED();
      break;
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
      loggedErrorState = SYNC_SIGN_IN_NEEDS_UPDATE;
      break;
    case SyncSetupService::kSyncServiceNeedsPassphrase:
      loggedErrorState = SYNC_NEEDS_PASSPHRASE;
      break;
    case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
      loggedErrorState = SYNC_NEEDS_TRUSTED_VAULT_KEY;
      break;
    case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
      loggedErrorState = SYNC_TRUSTED_VAULT_RECOVERABILITY_DEGRADED;
      break;
    case SyncSetupService::kSyncServiceUnrecoverableError:
      loggedErrorState = SYNC_UNRECOVERABLE_ERROR;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Sync.SyncErrorInfobarDisplayed", loggedErrorState);

  DCHECK(web_state);
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(web_state);
  DCHECK(infoBarManager);
  return SyncErrorInfoBarDelegate::Create(infoBarManager, browser_state,
                                          presenter);
}

bool IsTransientSyncError(SyncSetupService::SyncServiceState errorState) {
  switch (errorState) {
    case SyncSetupService::kNoSyncServiceError:
    case SyncSetupService::kSyncServiceCouldNotConnect:
    case SyncSetupService::kSyncServiceServiceUnavailable:
      return true;
    case SyncSetupService::kSyncServiceSignInNeedsUpdate:
    case SyncSetupService::kSyncServiceNeedsPassphrase:
    case SyncSetupService::kSyncServiceNeedsTrustedVaultKey:
    case SyncSetupService::kSyncServiceTrustedVaultRecoverabilityDegraded:
    case SyncSetupService::kSyncServiceUnrecoverableError:
      return false;
  }
}
