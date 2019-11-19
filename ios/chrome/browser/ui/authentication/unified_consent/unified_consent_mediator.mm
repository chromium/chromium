// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_mediator.h"

#include "base/logging.h"
#import "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"
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

@end

@implementation UnifiedConsentMediator

@synthesize selectedIdentityAvatar = _selectedIdentityAvatar;
@synthesize selectedIdentity = _selectedIdentity;
@synthesize unifiedConsentViewController = _unifiedConsentViewController;
@synthesize started = _started;

- (instancetype)initWithUnifiedConsentViewController:
    (UnifiedConsentViewController*)viewController {
  self = [super init];
  if (self) {
    _unifiedConsentViewController = viewController;
    _identityServiceObserver =
        std::make_unique<ChromeIdentityServiceObserverBridge>(self);
    _browserProviderObserver =
        std::make_unique<ChromeBrowserProviderObserverBridge>(self);
  }
  return self;
}

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  if (selectedIdentity == self.selectedIdentity) {
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

- (void)start {
  NSArray* identities = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->GetAllIdentitiesSortedForDisplay();
  if (identities.count != 0) {
    self.selectedIdentity = identities[0];
  }
  // Make sure the view is loaded so the mediator can set it up.
  [self.unifiedConsentViewController loadViewIfNeeded];
  self.started = YES;
  [self updateViewController];
}

#pragma mark - Private

// Updates the view if the mediator has been started.
- (void)updateViewController {
  // The UI should not be updated before the view is loaded.
  if (!self.started)
    return;
  if (self.selectedIdentity) {
    [self.unifiedConsentViewController
        updateIdentityPickerViewWithUserFullName:self.selectedIdentity
                                                     .userFullName
                                           email:self.selectedIdentity
                                                     .userEmail];
    [self.unifiedConsentViewController
        updateIdentityPickerViewWithAvatar:self.selectedIdentityAvatar];
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
    [self.unifiedConsentViewController hideIdentityPickerView];
  }
}

- (void)identityAvatarUpdated:(UIImage*)identityAvatar {
  if (_selectedIdentityAvatar == identityAvatar)
    return;
  _selectedIdentityAvatar = identityAvatar;
  [self.unifiedConsentViewController
      updateIdentityPickerViewWithAvatar:self.selectedIdentityAvatar];
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
  if (!self.selectedIdentity || !ios::GetChromeBrowserProvider()
                                     ->GetChromeIdentityService()
                                     ->IsValidIdentity(self.selectedIdentity)) {
    NSArray* identities = ios::GetChromeBrowserProvider()
                              ->GetChromeIdentityService()
                              ->GetAllIdentitiesSortedForDisplay();
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
  if (identity == self.selectedIdentity) {
    [self updateViewController];
  }
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

@end
