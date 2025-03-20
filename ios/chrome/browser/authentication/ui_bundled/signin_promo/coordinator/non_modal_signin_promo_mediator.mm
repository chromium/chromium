// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_mediator.h"

#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface NonModalSignInPromoMediator () <
    IdentityManagerObserverBridgeDelegate>

@end

@implementation NonModalSignInPromoMediator {
  // Bridge to observe changes in the identity manager.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // The AuthenticationService used by the mediator to monitor sign-in status.
  raw_ptr<AuthenticationService> _authService;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _authService = authService;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
  }

  return self;
}

#pragma mark - Public

- (void)startPromoDisplayTimer {
  // TODO(crbug.com/404845739): Implement Non modal sign in mediator.
}

- (void)stopShowingPromo {
  // TODO(crbug.com/404845739): Implement Non modal sign in mediator.
}

- (void)handleSignInButtonTapped {
  // TODO(crbug.com/404845739): Implement Non modal sign in mediator.
}

- (void)disconnect {
  _identityManagerObserver.reset();
  _authService = nil;
}

@end
