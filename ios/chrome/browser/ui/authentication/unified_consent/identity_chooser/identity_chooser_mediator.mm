// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_consumer.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_item.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IdentityChooserMediator ()<ChromeIdentityServiceObserver,
                                      ChromeBrowserProviderObserver> {
  std::unique_ptr<ChromeIdentityServiceObserverBridge> _identityServiceObserver;
  std::unique_ptr<ChromeBrowserProviderObserverBridge> _browserProviderObserver;
}

// Gets the Chrome identity service.
@property(nonatomic, assign, readonly)
    ios::ChromeIdentityService* chromeIdentityService;

@end

@implementation IdentityChooserMediator

@synthesize consumer = _consumer;
@synthesize selectedIdentity = _selectedIdentity;

- (void)start {
  _identityServiceObserver =
      std::make_unique<ChromeIdentityServiceObserverBridge>(self);
  _browserProviderObserver =
      std::make_unique<ChromeBrowserProviderObserverBridge>(self);
  [self loadIdentitySection];
}

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  if (_selectedIdentity == selectedIdentity)
    return;
  IdentityChooserItem* previousSelectedItem = [self.consumer
      identityChooserItemWithGaiaID:self.selectedIdentity.gaiaID];
  if (previousSelectedItem) {
    previousSelectedItem.selected = NO;
    [self.consumer itemHasChanged:previousSelectedItem];
  }
  _selectedIdentity = selectedIdentity;
  if (!_selectedIdentity) {
    return;
  }
  IdentityChooserItem* selectedItem = [self.consumer
      identityChooserItemWithGaiaID:self.selectedIdentity.gaiaID];
  DCHECK(selectedItem);
  selectedItem.selected = YES;
  [self.consumer itemHasChanged:selectedItem];
}

- (void)selectIdentityWithGaiaID:(NSString*)gaiaID {
  ChromeIdentity* identity = self.chromeIdentityService->GetIdentityWithGaiaID(
      base::SysNSStringToUTF8(gaiaID));
  DCHECK(identity);
  self.selectedIdentity = identity;
}

#pragma mark - Private

// Creates the identity section with its header item, and all the identity items
// based on the ChromeIdentity.
- (void)loadIdentitySection {
  // Create all the identity items.
  NSArray<ChromeIdentity*>* identities =
      self.chromeIdentityService->GetAllIdentitiesSortedForDisplay();
  NSMutableArray<IdentityChooserItem*>* items = [NSMutableArray array];
  for (ChromeIdentity* identity in identities) {
    IdentityChooserItem* item = [[IdentityChooserItem alloc] initWithType:0];
    [self updateIdentityChooserItem:item withChromeIdentity:identity];
    [items addObject:item];
  }

  [self.consumer setIdentityItems:items];
}

// Updates an IdentityChooserItem based on a ChromeIdentity.
- (void)updateIdentityChooserItem:(IdentityChooserItem*)item
               withChromeIdentity:(ChromeIdentity*)identity {
  item.gaiaID = identity.gaiaID;
  item.name = identity.userFullName;
  item.email = identity.userEmail;
  item.selected =
      [self.selectedIdentity.gaiaID isEqualToString:identity.gaiaID];
  __weak __typeof(self) weakSelf = self;
  ios::GetAvatarCallback callback = ^(UIImage* identityAvatar) {
    item.avatar = identityAvatar;
    [weakSelf.consumer itemHasChanged:item];
  };
  self.chromeIdentityService->GetAvatarForIdentity(identity, callback);
}

// Getter for the Chrome identity service.
- (ios::ChromeIdentityService*)chromeIdentityService {
  return ios::GetChromeBrowserProvider()->GetChromeIdentityService();
}

#pragma mark - ChromeIdentityServiceObserver

- (void)identityListChanged {
  [self loadIdentitySection];
  // Updates the selection.
  NSArray* allIdentities =
      self.chromeIdentityService->GetAllIdentitiesSortedForDisplay();
  if (![allIdentities containsObject:self.selectedIdentity]) {
    if (allIdentities.count) {
      self.selectedIdentity = allIdentities[0];
    } else {
      self.selectedIdentity = nil;
    }
  }
}

- (void)profileUpdate:(ChromeIdentity*)identity {
  IdentityChooserItem* item =
      [self.consumer identityChooserItemWithGaiaID:identity.gaiaID];
  [self updateIdentityChooserItem:item withChromeIdentity:identity];
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

@end
