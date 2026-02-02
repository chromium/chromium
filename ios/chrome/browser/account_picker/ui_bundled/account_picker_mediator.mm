// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_mediator.h"

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_mediator_delegate.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"

@interface AccountPickerMediator () <AuthenticationServiceObserving>
@end

@implementation AccountPickerMediator {
  raw_ptr<AuthenticationService> _authenticationService;
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;
}

- (instancetype)initWithAuthenticationService:
    (AuthenticationService*)authenticationService {
  CHECK(authenticationService->SigninEnabled(), base::NotFatalUntil::M144);
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _authServiceObserverBridge =
        std::make_unique<AuthenticationServiceObserverBridge>(
            authenticationService, self);
  }
  return self;
}

- (void)dealloc {
  CHECK(!_authenticationService, base::NotFatalUntil::M152);
}

#pragma mark - AccountPickerMediator

// Disconnect the mediator.
- (void)disconnect {
  _authenticationService = nil;
  _authServiceObserverBridge.reset();
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  if (!_authenticationService->SigninEnabled()) {
    // Signin is now disabled, so the consistency default account must be
    // stopped.
    [self.delegate accountPickerMediatorWantsToBeStopped:self];
  }
}

@end
