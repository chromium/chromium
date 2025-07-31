// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/add_account_signin/add_account_signin_mediator.h"

#import "ios/chrome/browser/authentication/ui_bundled/signin/add_account_signin/add_account_signin_mediator_delegate.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"

@interface AddAccountSigninMediator () <AuthenticationServiceObserving> {
  // Observer for auth service status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;
  raw_ptr<AuthenticationService> _authenticationService;
}

@end

@implementation AddAccountSigninMediator

- (instancetype)initWithAuthenticationService:
    (AuthenticationService*)authenticationService {
  if ((self = [super init])) {
    _authenticationService = authenticationService;
    _authServiceObserverBridge =
        std::make_unique<AuthenticationServiceObserverBridge>(
            authenticationService, self);
    CHECK(_authenticationService->SigninEnabled(), base::NotFatalUntil::M144);
  }
  return self;
}

#pragma mark - Public

- (void)disconnect {
  _authenticationService = nullptr;
  _authServiceObserverBridge.reset();
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  if (!_authenticationService->SigninEnabled()) {
    // Signin is now disabled, so the consistency default account must be
    // stopped.
    [self.delegate mediatorWantsToBeStopped:self];
  }
}

@end
