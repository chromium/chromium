// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_coordinator.h"

#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_mediator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface NonModalSignInPromoCoordinator () <
    NonModalSignInPromoMediatorDelegate>

@end

@implementation NonModalSignInPromoCoordinator {
  NonModalSignInPromoMediator* _mediator;
  SignInPromoType _promoType;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 promoType:(SignInPromoType)promoType {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _promoType = promoType;
  }
  return self;
}

- (void)start {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);

  _mediator = [[NonModalSignInPromoMediator alloc]
      initWithAuthenticationService:authService
                    identityManager:identityManager];

  _mediator.delegate = self;
  [_mediator startPromoDisplayTimer];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - NonModalSignInPromoMediatorDelegate

- (bool)nonModalSignInPromoMediatorTimerExpired:
    (NonModalSignInPromoMediator*)mediator {
  // TODO(crbug.com/404844914): Implement Non modal sign in coordinator.
  return false;
}

@end
