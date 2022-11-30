// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#import <UIKit/UIKit.h>
#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/password_manager/core/browser/password_manager_features_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"
#import "ios/web_view/public/cwv_sync_errors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSErrorDomain const CWVSyncErrorDomain =
    @"org.chromium.chromewebview.SyncErrorDomain";
NSErrorUserInfoKey const CWVSyncErrorDescriptionKey =
    @"org.chromium.chromewebview.SyncErrorDescriptionKey";
NSErrorUserInfoKey const CWVSyncErrorMessageKey =
    @"org.chromium.chromewebview.SyncErrorMessageKey";
NSErrorUserInfoKey const CWVSyncErrorIsTransientKey =
    @"org.chromium.chromewebview.SyncErrorIsTransientKey";

namespace {
CWVSyncError CWVConvertGoogleServiceAuthErrorStateToCWVSyncError(
    GoogleServiceAuthError::State state) {
  switch (state) {
    case GoogleServiceAuthError::NONE:
      return CWVSyncErrorNone;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      return CWVSyncErrorInvalidGAIACredentials;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      return CWVSyncErrorUserNotSignedUp;
    case GoogleServiceAuthError::CONNECTION_FAILED:
      return CWVSyncErrorConnectionFailed;
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      return CWVSyncErrorServiceUnavailable;
    case GoogleServiceAuthError::REQUEST_CANCELED:
      return CWVSyncErrorRequestCanceled;
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
      return CWVSyncErrorUnexpectedServiceResponse;
    // The following errors are unexpected on iOS.
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR:
    case GoogleServiceAuthError::NUM_STATES:
      NOTREACHED();
      return CWVSyncErrorNone;
  }
}
}  // namespace

@interface CWVSyncController ()

// Called by WebViewSyncControllerObserverBridge's |OnSyncShutdown|.
- (void)didShutdownSync;
// Called by WebViewSyncControllerObserverBridge's |OnStateChanged|.
- (void)syncStateDidChange;

// Call to reload accounts from the |dataSource|.
- (void)reloadAccounts;

@end

namespace ios_web_view {

// Bridge that observes syncer::SyncService and calls analagous
// methods on CWVSyncController.
class WebViewSyncControllerObserverBridge : public syncer::SyncServiceObserver {
 public:
  explicit WebViewSyncControllerObserverBridge(
      CWVSyncController* sync_controller)
      : sync_controller_(sync_controller) {}

  void OnStateChanged(syncer::SyncService* sync) override {
    [sync_controller_ syncStateDidChange];
  }

  void OnSyncShutdown(syncer::SyncService* sync) override {
    [sync_controller_ didShutdownSync];
  }

 private:
  __weak CWVSyncController* sync_controller_;
};

}  // namespace ios_web_view

@implementation CWVSyncController {
  syncer::SyncService* _syncService;
  signin::IdentityManager* _identityManager;
  std::unique_ptr<ios_web_view::WebViewSyncControllerObserverBridge> _observer;
  PrefService* _prefService;
  syncer::SyncService::TransportState _lastTransportState;
  GoogleServiceAuthError _lastAuthError;
}

namespace {
// Provider of trusted vault features.
__weak id<CWVTrustedVaultProvider> gTrustedVaultProvider;
// Data source that can provide access tokens.
__weak id<CWVSyncControllerDataSource> gSyncDataSource;
}

+ (void)setTrustedVaultProvider:
    (id<CWVTrustedVaultProvider>)trustedVaultProvider {
  gTrustedVaultProvider = trustedVaultProvider;
}

+ (id<CWVTrustedVaultProvider>)trustedVaultProvider {
  return gTrustedVaultProvider;
}

+ (void)setDataSource:(id<CWVSyncControllerDataSource>)dataSource {
  gSyncDataSource = dataSource;
}

+ (id<CWVSyncControllerDataSource>)dataSource {
  return gSyncDataSource;
}

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
                        prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _syncService = syncService;
    _identityManager = identityManager;
    _prefService = prefService;
    _observer =
        std::make_unique<ios_web_view::WebViewSyncControllerObserverBridge>(
            self);
    _syncService->AddObserver(_observer.get());

    // Refresh access tokens on foreground to extend expiration dates.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(reloadAccounts)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];

    // This allows internals of |_identityManager| to fetch and store the user's
    // info and profile image. This must be called manually *after* all services
    // have been started to avoid issues in https://crbug.com/441399.
    _identityManager->OnNetworkInitialized();
  }
  return self;
}

- (void)dealloc {
  _syncService->RemoveObserver(_observer.get());
}

#pragma mark - Public Methods

- (BOOL)isSyncing {
  return _syncService->GetTransportState() ==
         syncer::SyncService::TransportState::ACTIVE;
}

- (CWVIdentity*)currentIdentity {
  if (_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    CoreAccountInfo accountInfo =
        _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
    return [[CWVIdentity alloc]
        initWithEmail:base::SysUTF8ToNSString(accountInfo.email)
             fullName:nil
               gaiaID:base::SysUTF8ToNSString(accountInfo.gaia)];
  }

  return nil;
}

- (BOOL)isPassphraseNeeded {
  return _syncService->GetUserSettings()
      ->IsPassphraseRequiredForPreferredDataTypes();
}

- (BOOL)isTrustedVaultKeysRequired {
  return _syncService->GetUserSettings()
      ->IsTrustedVaultKeyRequiredForPreferredDataTypes();
}

- (BOOL)isTrustedVaultRecoverabilityDegraded {
  return _syncService->GetUserSettings()
      ->IsTrustedVaultRecoverabilityDegraded();
}

- (void)startSyncWithIdentity:(CWVIdentity*)identity {
  DCHECK(!self.currentIdentity)
      << "Already syncing! Call -stopSyncAndClearIdentity first.";

  _identityManager->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystemWithPrimaryAccount(CoreAccountId());

  const CoreAccountId accountId = _identityManager->PickAccountIdForAccount(
      base::SysNSStringToUTF8(identity.gaiaID),
      base::SysNSStringToUTF8(identity.email));
  CHECK(_identityManager->HasAccountWithRefreshToken(accountId));

  _identityManager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      accountId, signin::ConsentLevel::kSync);
  CHECK_EQ(_identityManager->GetPrimaryAccountId(signin::ConsentLevel::kSync),
           accountId);

  autofill::prefs::SetUserOptedInWalletSyncTransport(_prefService, accountId,
                                                     /*opted_in=*/true);
  password_manager::features_util::SetDefaultPasswordStore(
      _prefService, _syncService,
      password_manager::PasswordForm::Store::kAccountStore);
  password_manager::features_util::OptInToAccountStorage(_prefService,
                                                         _syncService);
}

- (void)stopSyncAndClearIdentity {
  auto* primaryAccountMutator = _identityManager->GetPrimaryAccountMutator();
  primaryAccountMutator->ClearPrimaryAccount(
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS,
      signin_metrics::SignoutDelete::kIgnoreMetric);
}

- (BOOL)unlockWithPassphrase:(NSString*)passphrase {
  return _syncService->GetUserSettings()->SetDecryptionPassphrase(
      base::SysNSStringToUTF8(passphrase));
}

#pragma mark - Private Methods

- (void)didShutdownSync {
  _syncService->RemoveObserver(_observer.get());
}

- (void)syncStateDidChange {
  if (_lastTransportState != _syncService->GetTransportState()) {
    _lastTransportState = _syncService->GetTransportState();

    if (_lastTransportState == syncer::SyncService::TransportState::ACTIVE) {
      if ([_delegate
              respondsToSelector:@selector(syncControllerDidStartSync:)]) {
        [_delegate syncControllerDidStartSync:self];
      }
    } else if (_lastTransportState ==
               syncer::SyncService::TransportState::DISABLED) {
      if ([_delegate
              respondsToSelector:@selector(syncControllerDidStopSync:)]) {
        [_delegate syncControllerDidStopSync:self];
      }
    }
  }

  if (_lastAuthError.state() != _syncService->GetAuthError().state()) {
    _lastAuthError = _syncService->GetAuthError();

    CWVSyncError code = CWVConvertGoogleServiceAuthErrorStateToCWVSyncError(
        _lastAuthError.state());
    if (code != CWVSyncErrorNone &&
        [_delegate respondsToSelector:@selector(syncController:
                                              didFailWithError:)]) {
      NSString* description =
          base::SysUTF8ToNSString(_lastAuthError.ToString());
      NSString* message =
          base::SysUTF8ToNSString(_lastAuthError.error_message());
      BOOL isTransient = _lastAuthError.IsTransientError();
      NSError* error =
          [NSError errorWithDomain:CWVSyncErrorDomain
                              code:code
                          userInfo:@{
                            CWVSyncErrorDescriptionKey : description,
                            CWVSyncErrorMessageKey : message,
                            CWVSyncErrorIsTransientKey : @(isTransient),
                          }];
      [_delegate syncController:self didFailWithError:error];
    }
  }

  if ([_delegate respondsToSelector:@selector(syncControllerDidUpdateState:)]) {
    [_delegate syncControllerDidUpdateState:self];
  }
}

- (void)reloadAccounts {
  _identityManager->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystemWithPrimaryAccount(
          _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
              .account_id);
}

@end
