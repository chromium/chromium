// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_mediator.h"

#import "base/containers/contains.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

@interface IdentityChooserMediator () <ChromeAccountManagerServiceObserver,
                                       IdentityManagerObserverBridgeDelegate> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
}

@end

@implementation IdentityChooserMediator {
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Identity manager to retrieve Chrome identities.
  raw_ptr<signin::IdentityManager> _identityManager;
}

@synthesize consumer = _consumer;
@synthesize selectedIdentity = _selectedIdentity;

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                  accountManagerService:
                      (ChromeAccountManagerService*)accountManagerService {
  if ((self = [super init])) {
    CHECK(identityManager);
    CHECK(accountManagerService);
    _identityManager = identityManager;
    _accountManagerService = accountManagerService;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_accountManagerService);
  DCHECK(!_identityManager);
}

- (void)start {
  _accountManagerServiceObserver =
      std::make_unique<ChromeAccountManagerServiceObserverBridge>(
          self, _accountManagerService);
  _identityManagerObserver =
      std::make_unique<signin::IdentityManagerObserverBridge>(_identityManager,
                                                              self);
  [self loadIdentitySection];
}

- (void)disconnect {
  _accountManagerServiceObserver.reset();
  _accountManagerService = nullptr;
  _identityManagerObserver.reset();
  _identityManager = nullptr;
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
  self.selectedIdentity = _accountManagerService->GetIdentityOnDeviceWithGaiaID(
      base::SysNSStringToUTF8(gaiaID));
}

#pragma mark - Private

- (bool)selectedIdentityIsValid {
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    if (self.selectedIdentity) {
      std::string gaia = base::SysNSStringToUTF8(self.selectedIdentity.gaiaID);
      return base::Contains(_identityManager->GetAccountsOnDevice(), gaia,
                            [](const AccountInfo& info) { return info.gaia; });
    }
    return false;
  } else {
    return _accountManagerService->IsValidIdentity(self.selectedIdentity);
  }
}

// Creates the identity section with its header item, and all the identity items
// based on the SystemIdentity.
- (void)loadIdentitySection {
  if (!_accountManagerService || !_identityManager) {
    return;
  }

  // Create all the identity items.
  NSArray<id<SystemIdentity>>* identitiesOnDevice =
      signin::GetIdentitiesOnDevice(_identityManager, _accountManagerService);
  NSMutableArray<TableViewIdentityItem*>* items = [NSMutableArray array];
  for (id<SystemIdentity> identity in identitiesOnDevice) {
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
  item.avatar = _accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::Regular);
  [self.consumer itemHasChanged:item];
}

- (void)handleIdentityListChanged {
  if (!_accountManagerService || !_identityManager) {
    return;
  }

  [self loadIdentitySection];
  // Update the selection, in case no identity was chosen yet, or the currently
  // selected identity has become unavailable (probably removed from the
  // device).
  if (![self selectedIdentityIsValid]) {
    self.selectedIdentity = signin::GetDefaultIdentityOnDevice(
        _identityManager, _accountManagerService);
  }
}

- (void)handleIdentityUpdated:(id<SystemIdentity>)identity {
  TableViewIdentityItem* item =
      [self.consumer tableViewIdentityItemWithGaiaID:identity.gaiaID];
  [self updateTableViewIdentityItem:item withIdentity:identity];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `onAccountsOnDeviceChanged` instead.
    return;
  }
  [self handleIdentityListChanged];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `onExtendedAccountInfoUpdated` instead.
    return;
  }
  [self handleIdentityUpdated:identity];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40284086): Remove `[self disconnect]`.
  [self disconnect];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `identityUpdated` instead.
    return;
  }
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(info.gaia);
  [self handleIdentityUpdated:identity];
}

- (void)onAccountsOnDeviceChanged {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `identityListChanged` instead.
    return;
  }
  [self handleIdentityListChanged];
}

@end
