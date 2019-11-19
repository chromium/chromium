// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#import <UIKit/UIKit.h>
#include <memory>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/password_manager/core/browser/password_store_default.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSErrorDomain const CWVSyncErrorDomain =
    @"org.chromium.chromewebview.SyncErrorDomain";

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
    case GoogleServiceAuthError::NUM_STATES:
      NOTREACHED();
      return CWVSyncErrorNone;
  }
}
}  // namespace

@interface CWVSyncController ()

// Called by WebViewSyncControllerObserverBridge's
// |OnSyncConfigurationCompleted|.
- (void)didCompleteSyncConfiguration;
// Called by WebViewSyncControllerObserverBridge's |OnSyncShutdown|.
- (void)didShutdownSync;
// Called by WebViewSyncControllerObserverBridge's |OnErrorChanged|.
- (void)didUpdateAuthError;
// Called by WebViewSyncControllerObserverBridge's |OnPrimaryAccountCleared|.
- (void)didClearPrimaryAccount;

// Call to refresh access tokens for |currentIdentity|.
- (void)reloadCredentials;

@end

namespace ios_web_view {

// Bridge that observes syncer::SyncService and calls analagous
// methods on CWVSyncController.
class WebViewSyncControllerObserverBridge
    : public syncer::SyncServiceObserver,
      public signin::IdentityManager::Observer,
      public SigninErrorController::Observer {
 public:
  explicit WebViewSyncControllerObserverBridge(
      CWVSyncController* sync_controller)
      : sync_controller_(sync_controller) {}

  // syncer::SyncServiceObserver:
  void OnSyncConfigurationCompleted(syncer::SyncService* sync) override {
    [sync_controller_ didCompleteSyncConfiguration];
  }

  void OnSyncShutdown(syncer::SyncService* sync) override {
    [sync_controller_ didShutdownSync];
  }

  // signin::IdentityManager::Observer
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override {
    [sync_controller_ didClearPrimaryAccount];
  }

  // SigninErrorController::Observer:
  void OnErrorChanged() override { [sync_controller_ didUpdateAuthError]; }

 private:
  __weak CWVSyncController* sync_controller_;
};

}  // namespace ios_web_view

@implementation CWVSyncController {
  syncer::SyncService* _syncService;
  signin::IdentityManager* _identityManager;
  SigninErrorController* _signinErrorController;
  std::unique_ptr<ios_web_view::WebViewSyncControllerObserverBridge> _observer;
  autofill::PersonalDataManager* _personalDataManager;
  password_manager::PasswordStore* _passwordStore;
}

@synthesize currentIdentity = _currentIdentity;
@synthesize delegate = _delegate;

namespace {
// Data source that can provide access tokens.
__weak id<CWVSyncControllerDataSource> gSyncDataSource;
}

+ (void)setDataSource:(id<CWVSyncControllerDataSource>)dataSource {
  gSyncDataSource = dataSource;
}

+ (id<CWVSyncControllerDataSource>)dataSource {
  return gSyncDataSource;
}

- (instancetype)
      initWithSyncService:(syncer::SyncService*)syncService
          identityManager:(signin::IdentityManager*)identityManager
    signinErrorController:(SigninErrorController*)signinErrorController
      personalDataManager:(autofill::PersonalDataManager*)personalDataManager
            passwordStore:(password_manager::PasswordStore*)passwordStore {
  self = [super init];
  if (self) {
    _syncService = syncService;
    _identityManager = identityManager;
    _signinErrorController = signinErrorController;
    _personalDataManager = personalDataManager;
    _passwordStore = passwordStore;
    _observer =
        std::make_unique<ios_web_view::WebViewSyncControllerObserverBridge>(
            self);
    _syncService->AddObserver(_observer.get());
    _identityManager->AddObserver(_observer.get());
    _signinErrorController->AddObserver(_observer.get());

    if (_identityManager->HasPrimaryAccount()) {
      CoreAccountInfo accountInfo = _identityManager->GetPrimaryAccountInfo();
      _currentIdentity = [[CWVIdentity alloc]
          initWithEmail:base::SysUTF8ToNSString(accountInfo.email)
               fullName:nil
                 gaiaID:base::SysUTF8ToNSString(accountInfo.gaia)];
    }

    // Refresh access tokens on foreground to extend expiration dates.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(reloadCredentials)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  _syncService->RemoveObserver(_observer.get());
  _identityManager->RemoveObserver(_observer.get());
  _signinErrorController->RemoveObserver(_observer.get());
}

#pragma mark - Public Methods

- (BOOL)isPassphraseNeeded {
  return _syncService->GetUserSettings()
      ->IsPassphraseRequiredForPreferredDataTypes();
}

- (void)startSyncWithIdentity:(CWVIdentity*)identity {
  DCHECK(!_currentIdentity)
      << "Already syncing! Call -stopSyncAndClearIdentity first.";

  _currentIdentity = identity;

  const CoreAccountId accountId = _identityManager->PickAccountIdForAccount(
      base::SysNSStringToUTF8(identity.gaiaID),
      base::SysNSStringToUTF8(identity.email));

  _identityManager->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystem();
  CHECK(_identityManager->HasAccountWithRefreshToken(accountId));

  _identityManager->GetPrimaryAccountMutator()->SetPrimaryAccount(accountId);
  CHECK_EQ(_identityManager->GetPrimaryAccountId(), accountId);

  _syncService->GetUserSettings()->SetSyncRequested(true);
  _syncService->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
}

- (void)stopSyncAndClearIdentity {
  _syncService->StopAndClear();

  auto* primaryAccountMutator = _identityManager->GetPrimaryAccountMutator();
  primaryAccountMutator->ClearPrimaryAccount(
      signin::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS,
      signin_metrics::SignoutDelete::IGNORE_METRIC);

  // Clear all data because we do not support data migration.
  // It is important that this happens post logging out so that the deletions
  // are only local to the device.
  _personalDataManager->ClearAllLocalData();
  // Clearing server data would usually result in data being deleted from the
  // user's data on sync servers, but because this is called after the user has
  // been logged out, this merely clears the left over, local copies.
  _personalDataManager->ClearAllServerData();
  _passwordStore->RemoveLoginsCreatedBetween(base::Time(), base::Time::Max(),
                                             base::Closure());

  _currentIdentity = nil;
}

- (BOOL)unlockWithPassphrase:(NSString*)passphrase {
  return _syncService->GetUserSettings()->SetDecryptionPassphrase(
      base::SysNSStringToUTF8(passphrase));
}

#pragma mark - Private Methods

- (void)didCompleteSyncConfiguration {
  if ([_delegate respondsToSelector:@selector(syncControllerDidStartSync:)]) {
    [_delegate syncControllerDidStartSync:self];
  }
}

- (void)didShutdownSync {
  _syncService->RemoveObserver(_observer.get());
  _signinErrorController->RemoveObserver(_observer.get());
}

- (void)reloadCredentials {
  _identityManager->GetDeviceAccountsSynchronizer()
      ->ReloadAllAccountsFromSystem();
}

#pragma mark - Internal Methods

- (void)didClearPrimaryAccount {
  if (![_delegate respondsToSelector:@selector(syncControllerDidStopSync:)]) {
    return;
  }
  [_delegate syncControllerDidStopSync:self];
}

- (void)didUpdateAuthError {
  GoogleServiceAuthError authError = _signinErrorController->auth_error();
  CWVSyncError code =
      CWVConvertGoogleServiceAuthErrorStateToCWVSyncError(authError.state());
  if (code != CWVSyncErrorNone) {
    if ([_delegate
            respondsToSelector:@selector(syncController:didFailWithError:)]) {
      NSError* error =
          [NSError errorWithDomain:CWVSyncErrorDomain code:code userInfo:nil];
      [_delegate syncController:self didFailWithError:error];
    }
  }
}

@end
