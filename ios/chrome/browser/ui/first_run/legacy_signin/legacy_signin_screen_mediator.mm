// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/legacy_signin/legacy_signin_screen_mediator.h"

#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/force_signin_logger.h"
#import "ios/chrome/browser/ui/first_run/legacy_signin/legacy_signin_screen_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacySigninScreenMediator () <ChromeAccountManagerServiceObserver> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

// Logger used to record sign in metrics.
@property(nonatomic, strong) UserSigninLogger* logger;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// Authentication service for sign in.
@property(nonatomic, assign) AuthenticationService* authenticationService;

@end

@implementation LegacySigninScreenMediator

- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                        authenticationService:
                            (AuthenticationService*)authenticationService {
  self = [super init];
  if (self) {
    DCHECK(accountManagerService);
    DCHECK(authenticationService);

    _accountManagerService = accountManagerService;
    _authenticationService = authenticationService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);

    // Use the forced sign-in access point when the force sign-in policy is
    // enabled, otherwise infer that the sign-in screen is used in the FRE. If
    // the forced sign-in screen is presented in the FRE, the forced sign-in
    // access point will still be used. The forced sign-in screen may also be
    // presented outside of the FRE when the user has to be prompted to sign-in
    // because of the policy.
    switch (authenticationService->GetServiceStatus()) {
      case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
        _logger = [[ForceSigninLogger alloc]
              initWithPromoAction:signin_metrics::PromoAction::
                                      PROMO_ACTION_NO_SIGNIN_PROMO
            accountManagerService:accountManagerService];
        break;
      case AuthenticationService::ServiceStatus::SigninAllowed:
        _logger = [[FirstRunSigninLogger alloc]
              initWithPromoAction:signin_metrics::PromoAction::
                                      PROMO_ACTION_NO_SIGNIN_PROMO
            accountManagerService:accountManagerService];
        break;
      case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
      case AuthenticationService::ServiceStatus::SigninDisabledByUser:
        NOTREACHED() << "Sign-in disabled: "
                     << static_cast<int>(
                            authenticationService->GetServiceStatus());
        break;
    }
    [_logger logSigninStarted];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
}

- (void)disconnect {
  [self.logger disconnect];
  self.accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
}

- (void)startSignInWithAuthenticationFlow:
            (AuthenticationFlow*)authenticationFlow
                               completion:(ProceduralBlock)completion {
  [self.consumer setUIEnabled:NO];
  __weak __typeof(self) weakSelf = self;
  [authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf.consumer setUIEnabled:YES];
    if (!success)
      return;
    [weakSelf.logger logSigninCompletedWithResult:SigninCoordinatorResultSuccess
                                     addedAccount:weakSelf.addedAccount
                            advancedSettingsShown:NO];
    if (completion)
      completion();
  }];
}

#pragma mark - Properties

- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  if ([_selectedIdentity isEqual:selectedIdentity])
    return;
  // nil is allowed only if there is no other identity.
  DCHECK(selectedIdentity || !self.accountManagerService->HasIdentities());
  _selectedIdentity = selectedIdentity;

  [self updateConsumer];
}

- (void)setConsumer:(id<LegacySigninScreenConsumer>)consumer {
  if (consumer == _consumer)
    return;
  _consumer = consumer;

  [self updateConsumer];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (!self.accountManagerService) {
    return;
  }

  if (!self.accountManagerService->IsValidIdentity(self.selectedIdentity)) {
    self.selectedIdentity = self.accountManagerService->GetDefaultIdentity();
  }
}

- (void)identityChanged:(id<SystemIdentity>)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateConsumer];
  }
}

#pragma mark - Private

- (void)updateConsumer {
  id<SystemIdentity> selectedIdentity = self.selectedIdentity;
  if (!selectedIdentity) {
    [self.consumer noIdentityAvailable];
  } else {
    UIImage* avatar = self.accountManagerService->GetIdentityAvatarWithIdentity(
        selectedIdentity, IdentityAvatarSize::Regular);
    [self.consumer setSelectedIdentityUserName:selectedIdentity.userFullName
                                         email:selectedIdentity.userEmail
                                     givenName:selectedIdentity.userGivenName
                                        avatar:avatar];
  }
}

@end
