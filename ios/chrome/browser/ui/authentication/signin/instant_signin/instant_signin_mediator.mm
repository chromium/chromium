// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/instant_signin/instant_signin_mediator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@implementation InstantSigninMediator {
  AuthenticationFlow* _authenticationFlow;
  AccessPoint _accessPoint;
  // YES if the sign-in is interrupted.
  BOOL _interrupted;
  // Completion block to call once AuthenticationFlow is done while being
  // interrupted.
  ProceduralBlock _interruptionCompletion;
}

- (instancetype)initWithAccessPoint:(AccessPoint)accessPoint {
  self = [super init];
  if (self) {
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

@end
