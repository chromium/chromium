// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_mediator.h"

#include "base/check.h"
#import "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#include "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UnifiedConsentMediator ()<ChromeIdentityServiceObserver,
                                     ChromeBrowserProviderObserver> {
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  std::unique_ptr<ChromeBrowserProviderObserverBridge> _browserProviderObserver;
}

// Unified consent view controller.
@property(nonatomic, weak)
    UnifiedConsentViewController* unifiedConsentViewController;
// Image for the selected identity avatar.
@property(nonatomic, strong) UIImage* selectedIdentityAvatar;
// NO until the mediator is started.
@property(nonatomic, assign) BOOL started;
// Authentication service for identities.
@property(nonatomic, assign) AuthenticationService* authenticationService;
// Pref service to retrieve preference values.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation UnifiedConsentMediator

@synthesize selectedIdentityAvatar = _selectedIdentityAvatar;
@synthesize selectedIdentity = _selectedIdentity;
@synthesize unifiedConsentViewController = _unifiedConsentViewController;
@synthesize started = _started;

- (instancetype)initWithUnifiedConsentViewController:
                    (UnifiedConsentViewController*)viewController
                               authenticationService:
                                   (AuthenticationService*)authenticationService
                                         prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _unifiedConsentViewController = viewController;
    _authenticationService = authenticationService;
    _identityServiceObserver =
        std::make_unique<ChromeIdentityServiceObserverBridge>(self);
    _browserProviderObserver =
        std::make_unique<ChromeBrowserProviderObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.prefService);
}

- (void)start {
  DCHECK(self.prefService);

  self.selectedIdentity = [self findDefaultSelectedIdentity];

  // Make sure the view is loaded so the mediator can set it up.
  [self.unifiedConsentViewController loadViewIfNeeded];
  self.started = YES;
  [self updateViewController];
}

- (void)disconnect {
  self.prefService = nullptr;
}

#pragma mark - Properties

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  if ([self.selectedIdentity isEqual:selectedIdentity]) {
    return;
  }
  // nil is allowed only if there is no other identity.
  DCHECK(selectedIdentity || !ios::GetChromeBrowserProvider()
                                  ->GetChromeIdentityService()
                                  ->HasIdentities());
  _selectedIdentity = selectedIdentity;
  self.selectedIdentityAvatar = nil;
  [self updateViewController];
}

#pragma mark - Private

- (ChromeIdentity*)findDefaultSelectedIdentity {
  if (self.authenticationService->IsAuthenticated()) {
    return self.authenticationService->GetAuthenticatedIdentity();
  }

  NSArray* identities =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->GetAllIdentitiesSortedForDisplay(self.prefService);
  return identities.count ? identities[0] : nil;
}

// Updates the view if the mediator has been started.
- (void)updateViewController {
  // The UI should not be updated before the view is loaded.
  if (!self.started)
    return;
  if (self.selectedIdentity) {
    [self.unifiedConsentViewController
        updateIdentityButtonControlWithUserFullName:self.selectedIdentity
                                                        .userFullName
                                              email:self.selectedIdentity
                                                        .userEmail];
    [self.unifiedConsentViewController
        updateIdentityButtonControlWithAvatar:self.selectedIdentityAvatar];
    ChromeIdentity* selectedIdentity = self.selectedIdentity;
    __weak UnifiedConsentMediator* weakSelf = self;
    ios::GetChromeBrowserProvider()
        ->GetChromeIdentityService()
        ->GetAvatarForIdentity(selectedIdentity, ^(UIImage* identityAvatar) {
          if (weakSelf.selectedIdentity != selectedIdentity)
            return;
          [weakSelf identityAvatarUpdated:identityAvatar];
        });
  } else {
    [self.unifiedConsentViewController hideIdentityButtonControl];
  }
}

- (void)identityAvatarUpdated:(UIImage*)identityAvatar {
  if (_selectedIdentityAvatar == identityAvatar)
    return;
  _selectedIdentityAvatar = identityAvatar;
  [self.unifiedConsentViewController
      updateIdentityButtonControlWithAvatar:self.selectedIdentityAvatar];
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
  if (!self.prefService) {
    return;
  }

  if (!self.selectedIdentity || !ios::GetChromeBrowserProvider()
                                     ->GetChromeIdentityService()
                                     ->IsValidIdentity(self.selectedIdentity)) {
    NSArray* identities =
        ios::GetChromeBrowserProvider()
            ->GetChromeIdentityService()
            ->GetAllIdentitiesSortedForDisplay(self.prefService);
    ChromeIdentity* newIdentity = nil;
    if (identities.count != 0) {
      newIdentity = identities[0];
    }
    self.selectedIdentity = newIdentity;
    [self.delegate
        unifiedConsentViewMediatorDelegateNeedPrimaryButtonUpdate:self];
  }
}

- (void)profileUpdate:(ChromeIdentity*)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateViewController];
  }
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

@end
