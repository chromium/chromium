// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_consumer.h"

@interface IdentityChooserMediator () <ChromeAccountManagerServiceObserver> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;

@end

@implementation IdentityChooserMediator

@synthesize consumer = _consumer;
@synthesize selectedIdentity = _selectedIdentity;

- (instancetype)initWithAccountManagerService:
    (ChromeAccountManagerService*)accountManagerService {
  if ((self = [super init])) {
    DCHECK(accountManagerService);
    _accountManagerService = accountManagerService;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
}

- (void)start {
  _accountManagerServiceObserver =
      std::make_unique<ChromeAccountManagerServiceObserverBridge>(
          self, _accountManagerService);
  [self loadIdentitySection];
}

- (void)disconnect {
  _accountManagerServiceObserver.reset();
  self.accountManagerService = nullptr;
}

- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  if ([_selectedIdentity isEqual:selectedIdentity])
    return;
  TableViewIdentityItem* previousSelectedItem =
      [self.consumer tableViewIdentityItemWithGaiaID:_selectedIdentity.gaiaID];
  if (previousSelectedItem) {
    previousSelectedItem.selected = NO;
    [self.consumer itemHasChanged:previousSelectedItem];
  }
  _selectedIdentity = selectedIdentity;
  if (!_selectedIdentity) {
    return;
  }
  TableViewIdentityItem* selectedItem =
      [self.consumer tableViewIdentityItemWithGaiaID:_selectedIdentity.gaiaID];
  DCHECK(selectedItem);
  selectedItem.selected = YES;
  [self.consumer itemHasChanged:selectedItem];
}

- (void)selectIdentityWithGaiaID:(NSString*)gaiaID {
  self.selectedIdentity = self.accountManagerService->GetIdentityWithGaiaID(
      base::SysNSStringToUTF8(gaiaID));
}

#pragma mark - Private

// Creates the identity section with its header item, and all the identity items
// based on the SystemIdentity.
- (void)loadIdentitySection {
  if (!self.accountManagerService) {
    return;
  }

  // Create all the identity items.
  NSArray<id<SystemIdentity>>* identities =
      self.accountManagerService->GetAllIdentities();
  NSMutableArray<TableViewIdentityItem*>* items = [NSMutableArray array];
  for (id<SystemIdentity> identity in identities) {
    TableViewIdentityItem* item =
        [[TableViewIdentityItem alloc] initWithType:0];
    item.identityViewStyle = IdentityViewStyleIdentityChooser;
    [self updateTableViewIdentityItem:item withIdentity:identity];
    [items addObject:item];
  }

  [self.consumer setIdentityItems:items];
}

// Updates an TableViewIdentityItem based on a SystemIdentity.
- (void)updateTableViewIdentityItem:(TableViewIdentityItem*)item
                       withIdentity:(id<SystemIdentity>)identity {
  item.gaiaID = identity.gaiaID;
  item.name = identity.userFullName;
  item.email = identity.userEmail;
  item.selected =
      [self.selectedIdentity.gaiaID isEqualToString:identity.gaiaID];
  item.avatar = self.accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::Regular);
  [self.consumer itemHasChanged:item];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (!self.accountManagerService) {
    return;
  }

  [self loadIdentitySection];
  // Updates the selection.
  if (!self.accountManagerService->IsValidIdentity(self.selectedIdentity)) {
    self.selectedIdentity = self.accountManagerService->GetDefaultIdentity();
  }
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  TableViewIdentityItem* item =
      [self.consumer tableViewIdentityItemWithGaiaID:identity.gaiaID];
  [self updateTableViewIdentityItem:item withIdentity:identity];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40284086): Remove `[self disconnect]`.
  [self disconnect];
}

@end
