// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_mediator.h"

#include "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#include "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_consumer.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninScreenMediator () <ChromeIdentityServiceObserver,
                                    ChromeBrowserProviderObserver> {
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  std::unique_ptr<ChromeBrowserProviderObserverBridge> _browserProviderObserver;
}

@property(nonatomic, readonly) ios::ChromeIdentityService* identityService;
// Logger used to record sign in metrics.
@property(nonatomic, strong) UserSigninLogger* logger;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// Authentication service for sign in.
@property(nonatomic, assign) AuthenticationService* authenticationService;

@end

@implementation SigninScreenMediator

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
    _browserProviderObserver =
        std::make_unique<ChromeBrowserProviderObserverBridge>(self);
    _identityServiceObserver =
        std::make_unique<ChromeIdentityServiceObserverBridge>(self);

    _logger = [[FirstRunSigninLogger alloc]
          initWithAccessPoint:signin_metrics::AccessPoint::
                                  ACCESS_POINT_START_PAGE
                  promoAction:signin_metrics::PromoAction::
                                  PROMO_ACTION_NO_SIGNIN_PROMO
        accountManagerService:accountManagerService];

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
}

- (void)startSignIn {
  self.authenticationService->SignIn(self.selectedIdentity);

  [self.logger logSigninCompletedWithResult:SigninCoordinatorResultSuccess
                               addedAccount:self.addedAccount
                      advancedSettingsShown:NO];
}

#pragma mark - Properties

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  if ([self.selectedIdentity isEqual:selectedIdentity])
    return;
  // nil is allowed only if there is no other identity.
  DCHECK(selectedIdentity || !self.accountManagerService->HasIdentities());
  _selectedIdentity = selectedIdentity;

  [self updateConsumer];
}

- (void)setConsumer:(id<SigninScreenConsumer>)consumer {
  if (consumer == _consumer)
    return;
  _consumer = consumer;

  [self updateConsumer];
}

- (ios::ChromeIdentityService*)identityService {
  return ios::GetChromeBrowserProvider().GetChromeIdentityService();
}

#pragma mark - ChromeBrowserProviderObserver

- (void)chromeIdentityServiceDidChange:(ios::ChromeIdentityService*)identity {
  DCHECK(!_identityServiceObserver.get());
  _identityServiceObserver =
      std::make_unique<ChromeIdentityServiceObserverBridge>(self);
}

- (void)chromeBrowserProviderWillBeDestroyed {
  _browserProviderObserver.reset();
}

#pragma mark - ChromeIdentityServiceObserver

- (void)identityListChanged {
  if (!self.accountManagerService) {
    return;
  }

  if (!self.selectedIdentity ||
      !self.accountManagerService->IsValidIdentity(self.selectedIdentity)) {
    self.selectedIdentity = self.accountManagerService->GetDefaultIdentity();
  }
}

- (void)profileUpdate:(ChromeIdentity*)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateConsumer];
  }
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - Private

- (void)updateConsumer {
  if (!self.consumer)
    return;

  // Reset the image to the default image. If an avatar icon is found, the image
  // will be updated.
  [self.consumer setUserImage:nil];

  if (self.selectedIdentity) {
    [self.consumer
        setSelectedIdentityUserName:self.selectedIdentity.userFullName
                              email:self.selectedIdentity.userEmail
                          givenName:self.selectedIdentity.userGivenName];

    ChromeIdentity* selectedIdentity = self.selectedIdentity;
    __weak __typeof(self) weakSelf = self;
    self.identityService->GetAvatarForIdentity(
        selectedIdentity, ^(UIImage* identityAvatar) {
          if (weakSelf.selectedIdentity != selectedIdentity)
            return;
          [weakSelf.consumer setUserImage:identityAvatar];
        });
  } else {
    [self.consumer noIdentityAvailable];
  }
}


@end
