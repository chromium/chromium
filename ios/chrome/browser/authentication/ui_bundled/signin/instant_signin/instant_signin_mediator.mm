// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/instant_signin/instant_signin_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface InstantSigninMediator () <AuthenticationFlowDelegate,
                                     AuthenticationServiceObserving>
@end

@implementation InstantSigninMediator {
  AuthenticationFlow* _authenticationFlow;
  AccessPoint _accessPoint;
  ChangeProfileContinuationProvider _continuationProvider;
  raw_ptr<AuthenticationService> _authenticationService;

  // Observer for auth service status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;
}

- (instancetype)
      initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
    authenticationService:(AuthenticationService*)authenticationService
     continuationProvider:
         (const ChangeProfileContinuationProvider&)continuationProvider {
  self = [super init];
  if (self) {
    CHECK(continuationProvider);
    CHECK(authenticationService);
    CHECK(authenticationService->SigninEnabled());
    _continuationProvider = continuationProvider;
    _accessPoint = accessPoint;
    _authenticationService = authenticationService;
    _authServiceObserverBridge =
        std::make_unique<AuthenticationServiceObserverBridge>(
            authenticationService, self);
  }
  return self;
}

#pragma mark - Public

- (void)startSignInOnlyFlowWithAuthenticationFlow:
    (AuthenticationFlow*)authenticationFlow {
  CHECK(!_authenticationFlow);
  _authenticationFlow = authenticationFlow;
  signin_metrics::RecordSigninUserActionForAccessPoint(_accessPoint);
  _authenticationFlow.delegate = self;
  [_authenticationFlow startSignIn];
}

- (void)disconnect {
  _authServiceObserverBridge.reset();
  [_authenticationFlow interrupt];
}

#pragma mark - AuthenticationFlowDelegate

- (void)authenticationFlowDidSignInInSameProfileWithResult:
    (SigninCoordinatorResult)result {
  _authenticationFlow = nil;
  [self.delegate instantSigninMediator:self didSigninWithResult:result];
}

- (void)authenticationFlowWillSwitchProfileWithReadyCompletion:
    (ReadyForProfileSwitchingCompletion)readyCompletion {
  _authenticationFlow = nil;
  [self.delegate instantSigninMediatorWillSwitchProfile:self];
  std::move(readyCompletion).Run(_continuationProvider.Run());
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  if (!_authenticationService->SigninEnabled()) {
    // Signin is now disabled, so the consistency default account must be
    // stopped.
    [self.delegate
        instantSigninMediator:self
          didSigninWithResult:SigninCoordinatorResult::
                                  SigninCoordinatorResultInterrupted];
  }
}

@end
