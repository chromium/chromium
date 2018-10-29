// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"

#import <UIKit/UIKit.h>
#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "ios/web/public/web_thread.h"
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
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED_DEPRECATED:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::WEB_LOGIN_REQUIRED:
    case GoogleServiceAuthError::NUM_STATES:
      NOTREACHED();
      return CWVSyncErrorNone;
  }
}
}  // namespace

@interface CWVSyncController ()

// Called by WebViewSyncServiceObserverBridge's |OnSyncConfigurationCompleted|.
- (void)didCompleteSyncConfiguration;
// Called by WebViewSyncServiceObserverBridge's |OnSyncShutdown|.
- (void)didShutdownSync;

// Call to refresh access tokens for |currentIdentity|.
- (void)reloadCredentials;

@end

namespace ios_web_view {

// Bridge that observes browser_sync::ProfileSyncService and calls analagous
// methods on CWVSyncController.
class WebViewSyncServiceObserverBridge : public syncer::SyncServiceObserver {
 public:
  explicit WebViewSyncServiceObserverBridge(CWVSyncController* sync_controller)
      : sync_controller_(sync_controller) {}
  void OnStateChanged(syncer::SyncService* sync) override {
    // No op.
  }

  void OnSyncCycleCompleted(syncer::SyncService* sync) override {
    // No op.
  }

  void OnSyncConfigurationCompleted(syncer::SyncService* sync) override {
    [sync_controller_ didCompleteSyncConfiguration];
  }

  void OnForeignSessionUpdated(syncer::SyncService* sync) override {
    // No op.
  }

  void OnSyncShutdown(syncer::SyncService* sync) override {
    [sync_controller_ didShutdownSync];
  }

 private:
  __weak CWVSyncController* sync_controller_;
};

}  // namespace ios_web_view

@implementation CWVSyncController {
  browser_sync::ProfileSyncService* _profileSyncService;
  AccountTrackerService* _accountTrackerService;
  SigninManager* _signinManager;
  IOSWebViewSigninClient* _signinClient;
  ProfileOAuth2TokenService* _tokenService;
  std::unique_ptr<ios_web_view::WebViewSyncServiceObserverBridge> _observer;

  // Data source that can provide access tokens.
  __weak id<CWVSyncControllerDataSource> _dataSource;
}

@synthesize delegate = _delegate;

- (instancetype)
initWithProfileSyncService:(browser_sync::ProfileSyncService*)profileSyncService
     accountTrackerService:(AccountTrackerService*)accountTrackerService
             signinManager:(SigninManager*)signinManager
              tokenService:(ProfileOAuth2TokenService*)tokenService {
  self = [super init];
  if (self) {
    _profileSyncService = profileSyncService;
    _accountTrackerService = accountTrackerService;
    _signinManager = signinManager;
    _tokenService = tokenService;
    _observer =
        std::make_unique<ios_web_view::WebViewSyncServiceObserverBridge>(self);
    _profileSyncService->AddObserver(_observer.get());

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
  _profileSyncService->RemoveObserver(_observer.get());
}

#pragma mark - Public Methods

- (CWVIdentity*)currentIdentity {
  std::string authenticatedID = _signinManager->GetAuthenticatedAccountId();
  if (authenticatedID.empty()) {
    return nil;
  }
  AccountInfo accountInfo =
      _accountTrackerService->GetAccountInfo(authenticatedID);
  NSString* email = base::SysUTF8ToNSString(accountInfo.email);
  NSString* fullName = base::SysUTF8ToNSString(accountInfo.full_name);
  NSString* gaiaID = base::SysUTF8ToNSString(accountInfo.gaia);
  return
      [[CWVIdentity alloc] initWithEmail:email fullName:fullName gaiaID:gaiaID];
}

- (BOOL)isPassphraseNeeded {
  return _profileSyncService->IsPassphraseRequiredForDecryption();
}

- (void)startSyncWithIdentity:(CWVIdentity*)identity
                   dataSource:
                       (__weak id<CWVSyncControllerDataSource>)dataSource {
  DCHECK(!_dataSource);

  _dataSource = dataSource;

  AccountInfo info;
  info.gaia = base::SysNSStringToUTF8(identity.gaiaID);
  info.email = base::SysNSStringToUTF8(identity.email);
  info.full_name = base::SysNSStringToUTF8(identity.fullName);
  std::string newAuthenticatedAccountID =
      _accountTrackerService->SeedAccountInfo(info);
  _signinManager->OnExternalSigninCompleted(info.email);

  [self reloadCredentials];
  _profileSyncService->RequestStart();
  _profileSyncService->SetFirstSetupComplete();
}

- (void)stopSyncAndClearIdentity {
  _profileSyncService->RequestStop(syncer::SyncService::CLEAR_DATA);
  _signinManager->SignOut(
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  _dataSource = nil;
}

- (BOOL)unlockWithPassphrase:(NSString*)passphrase {
  return _profileSyncService->SetDecryptionPassphrase(
      base::SysNSStringToUTF8(passphrase));
}

#pragma mark - Private Methods

- (void)didCompleteSyncConfiguration {
  if ([_delegate respondsToSelector:@selector(syncControllerDidStartSync:)]) {
    [_delegate syncControllerDidStartSync:self];
  }
}

- (void)didShutdownSync {
  _profileSyncService->RemoveObserver(_observer.get());
}

- (void)reloadCredentials {
  std::string authenticatedID = _signinManager->GetAuthenticatedAccountId();
  if (!authenticatedID.empty()) {
    ProfileOAuth2TokenServiceIOSDelegate* tokenDelegate =
        static_cast<ProfileOAuth2TokenServiceIOSDelegate*>(
            _tokenService->GetDelegate());
    tokenDelegate->ReloadCredentials(authenticatedID);
  }
}

#pragma mark - Internal Methods

- (void)fetchAccessTokenForScopes:(const std::set<std::string>&)scopes
                         callback:(const ProfileOAuth2TokenServiceIOSProvider::
                                       AccessTokenCallback&)callback {
  NSMutableArray<NSString*>* scopesArray = [NSMutableArray array];
  for (const auto& scope : scopes) {
    [scopesArray addObject:base::SysUTF8ToNSString(scope)];
  }
  ProfileOAuth2TokenServiceIOSProvider::AccessTokenCallback scopedCallback =
      callback;
  [_dataSource syncController:self
      getAccessTokenForScopes:[scopesArray copy]
            completionHandler:^(NSString* accessToken, NSDate* expirationDate,
                                NSError* error) {
              if (!scopedCallback.is_null()) {
                scopedCallback.Run(accessToken, expirationDate, error);
              }
            }];
}

- (void)didSignoutWithSourceMetric:(signin_metrics::ProfileSignout)metric {
  if (![_delegate respondsToSelector:@selector
                  (syncController:didStopSyncWithReason:)]) {
    return;
  }
  CWVStopSyncReason reason;
  if (metric == signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS) {
    reason = CWVStopSyncReasonClient;
  } else if (metric == signin_metrics::ProfileSignout::SERVER_FORCED_DISABLE) {
    reason = CWVStopSyncReasonServer;
  } else {
    NOTREACHED();
    return;
  }
  [_delegate syncController:self didStopSyncWithReason:reason];
}

- (void)didUpdateAuthError:(const GoogleServiceAuthError&)authError {
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
