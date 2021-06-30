// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_mediator.h"

#import "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/resized_avatar_cache.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_consumer.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencyDefaultAccountMediator () <
    ChromeBrowserProviderObserver,
    ChromeIdentityServiceObserver> {
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  std::unique_ptr<ChromeBrowserProviderObserverBridge> _browserProviderObserver;
}

@property(nonatomic, strong) UIImage* avatar;
@property(nonatomic, strong) ResizedAvatarCache* avatarCache;
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

@implementation ConsistencyDefaultAccountMediator

- (instancetype)initWithAccountManagerService:
    (ChromeAccountManagerService*)accountManagerService {
  if (self = [super init]) {
    DCHECK(accountManagerService);
    _accountManagerService = accountManagerService;
    _identityServiceObserver =
        std::make_unique<ChromeIdentityServiceObserverBridge>(self);
    _browserProviderObserver =
        std::make_unique<ChromeBrowserProviderObserverBridge>(self);
    _avatarCache = [[ResizedAvatarCache alloc] init];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
}

- (void)disconnect {
  self.accountManagerService = nullptr;
}

#pragma mark - Properties

- (void)setConsumer:(id<ConsistencyDefaultAccountConsumer>)consumer {
  _consumer = consumer;
  [self selectSelectedIdentity];
}

- (void)setSelectedIdentity:(ChromeIdentity*)identity {
  DCHECK(identity);
  if (_selectedIdentity == identity) {
    return;
  }
  _selectedIdentity = identity;
  [self updateSelectedIdentityUI];
}

#pragma mark - Private

// Updates the default identity.
- (void)selectSelectedIdentity {
  if (!self.accountManagerService) {
    return;
  }

  ChromeIdentity* identity = self.accountManagerService->GetDefaultIdentity();
  if (!identity) {
    [self.delegate consistencyDefaultAccountMediatorNoIdentities:self];
    return;
  }

  if ([identity isEqual:self.selectedIdentity]) {
    return;
  }

  self.selectedIdentity = identity;
}

// Updates the view controller using the default identity.
- (void)updateSelectedIdentityUI {
  [self.consumer updateWithFullName:self.selectedIdentity.userFullName
                          givenName:self.selectedIdentity.userGivenName
                              email:self.selectedIdentity.userEmail];
  UIImage* avatar =
      [self.avatarCache resizedAvatarForIdentity:self.selectedIdentity];
  [self.consumer updateUserAvatar:avatar];
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
  [self selectSelectedIdentity];
}

- (void)profileUpdate:(ChromeIdentity*)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateSelectedIdentityUI];
  }
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

@end
