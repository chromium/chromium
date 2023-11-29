// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/instant_signin/instant_signin_mediator.h"

#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@implementation InstantSigninMediator {
  syncer::SyncService* _syncService;
  AuthenticationFlow* _authenticationFlow;
  AccessPoint _accessPoint;
  // YES if the sign-in is interrupted.
  BOOL _interrupted;
  // Completion block to call once AuthenticationFlow is done while being
  // interrupted.
  ProceduralBlock _interruptionCompletion;
}

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                        accessPoint:(signin_metrics::AccessPoint)accessPoint {
  self = [super init];
  if (self) {
    CHECK(syncService);
    _syncService = syncService;
    _accessPoint = accessPoint;
  }
  return self;
}

#pragma mark - Public

- (void)startSignInOnlyFlowWithAuthenticationFlow:
    (AuthenticationFlow*)authenticationFlow {
  CHECK(!_authenticationFlow);
  _authenticationFlow = authenticationFlow;
  signin_metrics::RecordSigninUserActionForAccessPoint(_accessPoint);
  __weak __typeof(self) weakSelf = self;
  [_authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf signInFlowCompletedForSignInOnlyWithSuccess:success];
  }];
}

- (void)disconnect {
  CHECK(!_authenticationFlow);
  CHECK(!_interruptionCompletion);
  _syncService = nullptr;
}

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  CHECK(_authenticationFlow);
  _interrupted = YES;
  _interruptionCompletion = [completion copy];
  [_authenticationFlow interruptWithAction:action];
}

#pragma mark - Private

// Called when the sign-in flow is over.
- (void)signInFlowCompletedForSignInOnlyWithSuccess:(BOOL)success {
  CHECK(_authenticationFlow);
  _authenticationFlow.delegate = nil;
  _authenticationFlow = nil;
  ProceduralBlock interruptionCompletion = _interruptionCompletion;
  _interruptionCompletion = nil;
  SigninCoordinatorResult result;
  if (success) {
    [self enableBookmarkAndReadingListAccountStorageOptInIfNeeded];
    result = SigninCoordinatorResultSuccess;
  } else if (_interrupted) {
    result = SigninCoordinatorResultInterrupted;
  } else {
    result = SigninCoordinatorResultCanceledByUser;
  }
  [self.delegate instantSigninMediator:self didSigninWithResult:result];
  if (interruptionCompletion) {
    interruptionCompletion();
  }
}

// Enables bookmark and reading list storage opt-in only if the sign-in has been
// triggered from bookmark or reading list.
- (void)enableBookmarkAndReadingListAccountStorageOptInIfNeeded {
  if (_accessPoint !=
          signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER &&
      _accessPoint != signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST) {
    return;
  }
  bool bookmarksAccountStorageEnabled =
      base::FeatureList::IsEnabled(syncer::kEnableBookmarksAccountStorage);
  bool readingListTransportUponSignInEnabled = base::FeatureList::IsEnabled(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
  CHECK(bookmarksAccountStorageEnabled || readingListTransportUponSignInEnabled)
      << "bookmarksAccountStorageEnabled: " << bookmarksAccountStorageEnabled
      << ", readingListTransportUponSignInEnabled: "
      << readingListTransportUponSignInEnabled;
  _syncService->GetUserSettings()
      ->SetBookmarksAndReadingListAccountStorageOptIn(true);
}

@end
