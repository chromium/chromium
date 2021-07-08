// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_mediator.h"

#import "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/resized_avatar_cache.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/identity_item_configurator.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencyAccountChooserMediator () <
    ChromeBrowserProviderObserver,
    ChromeIdentityServiceObserver> {
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  std::unique_ptr<ChromeBrowserProviderObserverBridge> _browserProviderObserver;
}

@property(nonatomic, strong) ResizedAvatarCache* avatarCache;
// Configurators based on ChromeIdentity list.
@property(nonatomic, strong) NSArray* sortedIdentityItemConfigurators;
@property(nonatomic, assign, readonly)
    ios::ChromeIdentityService* chromeIdentityService;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

@implementation ConsistencyAccountChooserMediator

- (instancetype)initWithSelectedIdentity:(ChromeIdentity*)selectedIdentity
                   accountManagerService:
                       (ChromeAccountManagerService*)accountManagerService {
  if (self = [super init]) {
    DCHECK(accountManagerService);
    _accountManagerService = accountManagerService;
    _identityServiceObserver =
        std::make_unique<ChromeIdentityServiceObserverBridge>(self);
    _browserProviderObserver =
        std::make_unique<ChromeBrowserProviderObserverBridge>(self);
    _avatarCache = [[ResizedAvatarCache alloc] init];
    _selectedIdentity = selectedIdentity;
    [self loadIdentityItemConfigurators];
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

- (ios::ChromeIdentityService*)chromeIdentityService {
  return ios::GetChromeBrowserProvider().GetChromeIdentityService();
}

- (void)setSelectedIdentity:(ChromeIdentity*)identity {
  DCHECK(identity);
  if (_selectedIdentity == identity) {
    return;
  }
  ChromeIdentity* previousSelectedIdentity = _selectedIdentity;
  _selectedIdentity = identity;
  [self profileUpdate:previousSelectedIdentity];
  [self profileUpdate:_selectedIdentity];
}

#pragma mark - Private

// Updates |self.sortedIdentityItemConfigurators| based on ChromeIdentity list.
- (void)loadIdentityItemConfigurators {
  if (!self.accountManagerService) {
    return;
  }

  NSMutableArray* configurators = [NSMutableArray array];
  NSArray* identities = self.accountManagerService->GetAllIdentities();
  BOOL hasSelectedIdentity = NO;
  for (ChromeIdentity* identity in identities) {
    IdentityItemConfigurator* configurator =
        [[IdentityItemConfigurator alloc] init];
    [self updateIdentityItemConfigurator:configurator
                      withChromeIdentity:identity];
    [configurators addObject:configurator];
    if (configurator.selected) {
      hasSelectedIdentity = YES;
    }
    // If the configurator is selected, the identity must be equal to
    // |self.selectedIdentity|.
    DCHECK(!configurator.selected || [self.selectedIdentity isEqual:identity]);
  }
  if (!hasSelectedIdentity && identities.count > 0) {
    // No selected identity has been found, a default need to be selected.
    self.selectedIdentity = identities[0];
    IdentityItemConfigurator* configurator = configurators[0];
    configurator.selected = YES;
  }
  self.sortedIdentityItemConfigurators = configurators;
}

// Updates an IdentityItemConfigurator based on a ChromeIdentity.
- (void)updateIdentityItemConfigurator:(IdentityItemConfigurator*)configurator
                    withChromeIdentity:(ChromeIdentity*)identity {
  configurator.gaiaID = identity.gaiaID;
  configurator.name = identity.userFullName;
  configurator.email = identity.userEmail;
  configurator.avatar = [self.avatarCache resizedAvatarForIdentity:identity];
  configurator.selected = [identity isEqual:self.selectedIdentity];
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
  [self loadIdentityItemConfigurators];
  [self.consumer reloadAllIdentities];
}

- (void)profileUpdate:(ChromeIdentity*)identity {
  IdentityItemConfigurator* configurator = nil;
  for (IdentityItemConfigurator* cursor in self
           .sortedIdentityItemConfigurators) {
    if ([cursor.gaiaID isEqual:identity.gaiaID]) {
      configurator = cursor;
    }
  }
  DCHECK(configurator);
  [self updateIdentityItemConfigurator:configurator
                    withChromeIdentity:identity];
  [self.consumer reloadIdentityForIdentityItemConfigurator:configurator];
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

#pragma mark - ConsistencyAccountChooserTableViewControllerModelDelegate

- (NSArray*)sortedIdentityItemConfigurators {
  if (!_sortedIdentityItemConfigurators) {
    [self loadIdentityItemConfigurators];
  }
  DCHECK(_sortedIdentityItemConfigurators);
  return _sortedIdentityItemConfigurators;
}

@end
