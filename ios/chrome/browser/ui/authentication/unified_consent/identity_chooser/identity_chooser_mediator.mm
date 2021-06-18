// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"
#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_consumer.h"
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

// Pref service to retrieve preference values.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation IdentityChooserMediator

@synthesize consumer = _consumer;
@synthesize selectedIdentity = _selectedIdentity;

- (instancetype)initWithPrefService:(PrefService*)prefService {
  if (self = [super init]) {
    _prefService = prefService;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.prefService);
}

- (void)start {
  _identityServiceObserver =
      std::make_unique<ChromeIdentityServiceObserverBridge>(self);
  _browserProviderObserver =
      std::make_unique<ChromeBrowserProviderObserverBridge>(self);
  [self loadIdentitySection];
}

- (void)disconnect {
  self.prefService = nullptr;
}

- (void)setSelectedIdentity:(ChromeIdentity*)selectedIdentity {
  if ([_selectedIdentity isEqual:selectedIdentity])
    return;
  TableViewIdentityItem* previousSelectedItem = [self.consumer
      tableViewIdentityItemWithGaiaID:self.selectedIdentity.gaiaID];
  if (previousSelectedItem) {
    previousSelectedItem.selected = NO;
    [self.consumer itemHasChanged:previousSelectedItem];
  }
  _selectedIdentity = selectedIdentity;
  if (!_selectedIdentity) {
    return;
  }
  TableViewIdentityItem* selectedItem = [self.consumer
      tableViewIdentityItemWithGaiaID:self.selectedIdentity.gaiaID];
  DCHECK(selectedItem);
  selectedItem.selected = YES;
  [self.consumer itemHasChanged:selectedItem];
}

- (void)selectIdentityWithGaiaID:(NSString*)gaiaID {
  self.selectedIdentity = self.chromeIdentityService->GetIdentityWithGaiaID(
      base::SysNSStringToUTF8(gaiaID));
}

#pragma mark - Private

// Creates the identity section with its header item, and all the identity items
// based on the ChromeIdentity.
- (void)loadIdentitySection {
  if (!self.prefService) {
    return;
  }

  // Create all the identity items.
  NSArray<ChromeIdentity*>* identities =
      self.chromeIdentityService->GetAllIdentities(self.prefService);
  NSMutableArray<TableViewIdentityItem*>* items = [NSMutableArray array];
  for (ChromeIdentity* identity in identities) {
    TableViewIdentityItem* item =
        [[TableViewIdentityItem alloc] initWithType:0];
    [self updateTableViewIdentityItem:item withChromeIdentity:identity];
    [items addObject:item];
  }

  [self.consumer setIdentityItems:items];
}

// Updates an TableViewIdentityItem based on a ChromeIdentity.
- (void)updateTableViewIdentityItem:(TableViewIdentityItem*)item
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
  if (!self.prefService) {
    return;
  }

  [self loadIdentitySection];
  // Updates the selection.
  NSArray* allIdentities =
      self.chromeIdentityService->GetAllIdentities(self.prefService);
  if (![allIdentities containsObject:self.selectedIdentity]) {
    if (allIdentities.count) {
      self.selectedIdentity = allIdentities[0];
    } else {
      self.selectedIdentity = nil;
    }
  }
}

- (void)profileUpdate:(ChromeIdentity*)identity {
  TableViewIdentityItem* item =
      [self.consumer tableViewIdentityItemWithGaiaID:identity.gaiaID];
  [self updateTableViewIdentityItem:item withChromeIdentity:identity];
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _identityServiceObserver.reset();
}

@end
