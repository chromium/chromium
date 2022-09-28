// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_mediator.h"

#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TangibleSyncMediator () <ChromeAccountManagerServiceObserver>

@end

@implementation TangibleSyncMediator {
  AuthenticationService* _authenticationService;
  ChromeAccountManagerService* _accountManagerService;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
      chromeAccountManagerService:
          (ChromeAccountManagerService*)chromeAccountManagerService {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _accountManagerService = chromeAccountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
  }
  return self;
}

- (void)disconnect {
  _accountManagerServiceObserver.reset();
  self.consumer = nil;
  _authenticationService = nil;
  _accountManagerService = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<TangibleSyncConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  [self updateAvatarImageWithIdentity:identity];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityChanged:(id<SystemIdentity>)identity {
  id<SystemIdentity> primaryIdentity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if ([primaryIdentity isEqual:identity]) {
    [self updateAvatarImageWithIdentity:identity];
  }
}

#pragma mark - Private

// Updates the avatar image for the consumer from `identity`.
- (void)updateAvatarImageWithIdentity:(id<SystemIdentity>)identity {
  UIImage* image = _accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::Large);
  self.consumer.primaryIdentityAvatarImage = image;
}

@end
