// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_mediator.h"

#include "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"
#include "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_consumer.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_mediator_delegate.h"
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
// Manager for the authentication flow.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// Logger used to record sign in metrics.
@property(nonatomic, strong) UserSigninLogger* logger;

@end

@implementation SigninScreenMediator

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
    _browserProviderObserver =
        std::make_unique<ChromeBrowserProviderObserverBridge>(self);
    _identityServiceObserver =
        std::make_unique<ChromeIdentityServiceObserverBridge>(self);

    _logger = [[FirstRunSigninLogger alloc]
        initWithAccessPoint:signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE
                promoAction:signin_metrics::PromoAction::
                                PROMO_ACTION_NO_SIGNIN_PROMO];
  }
  return self;
}

- (void)startSignInWithAuthenticationFlow:
    (AuthenticationFlow*)authenticationFlow {
  DCHECK(!self.authenticationFlow);

  [self.consumer setUIEnabled:NO];

  self.authenticationFlow = authenticationFlow;
  __weak __typeof(self) weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    [weakSelf onAccountSigninCompletionWithSuccess:success];
  }];
}

#pragma mark - Properties

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  if ([self.selectedIdentity isEqual:selectedIdentity])
    return;
  // nil is allowed only if there is no other identity.
  DCHECK(selectedIdentity || !self.identityService->HasIdentities());
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
  return ios::GetChromeBrowserProvider()->GetChromeIdentityService();
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
  if (!self.selectedIdentity ||
      !self.identityService->IsValidIdentity(self.selectedIdentity)) {
    NSArray* identities =
        self.identityService->GetAllIdentitiesSortedForDisplay();
    ChromeIdentity* newIdentity = nil;
    if (identities.count != 0) {
      newIdentity = identities[0];
    }
    self.selectedIdentity = newIdentity;
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
                              email:self.selectedIdentity.userEmail];

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

  // TODO(crbug.com/1189836): Update the buttons.
}

// Callback used when the sign in flow is complete, with |success|.
- (void)onAccountSigninCompletionWithSuccess:(BOOL)success {
  self.authenticationFlow = nil;
  [self.consumer setUIEnabled:YES];

  if (success) {
    // Only log if the sign-in is successful.
    [self.logger logSigninCompletedWithResult:SigninCoordinatorResultSuccess
                                 addedAccount:self.addedAccount
                        advancedSettingsShown:NO];

    [self.delegate signinScreenMediator:self
              didFinishSigninWithResult:SigninCoordinatorResultSuccess];
  }
}

@end
