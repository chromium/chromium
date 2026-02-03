// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_mediator.h"

#import <algorithm>

#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_identity_item.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/avatar/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@interface IdentityChooserMediator () <IdentityManagerObserverBridgeDelegate> {
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
}

@end

@implementation IdentityChooserMediator {
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Identity manager to retrieve Chrome identities.
  raw_ptr<signin::IdentityManager> _identityManager;
  id<SystemIdentity> _defaultIdentity;
}

- (instancetype)
    initWithIdentityManager:(signin::IdentityManager*)identityManager
      accountManagerService:(ChromeAccountManagerService*)accountManagerService
            defaultIdentity:(id<SystemIdentity>)defaultIdentity {
  if ((self = [super init])) {
    CHECK(identityManager);
    CHECK(accountManagerService);
    _identityManager = identityManager;
    _accountManagerService = accountManagerService;
    _defaultIdentity = defaultIdentity;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_accountManagerService);
  DCHECK(!_identityManager);
}

- (void)start {
  _identityManagerObserver =
      std::make_unique<signin::IdentityManagerObserverBridge>(_identityManager,
                                                              self);
  [self loadIdentitySection];
}

- (void)disconnect {
  _accountManagerService = nullptr;
  _identityManagerObserver.reset();
  _identityManager = nullptr;
}


#pragma mark - Private

- (bool)defaultIdentityIsValid {
  if (_defaultIdentity) {
    GaiaId gaia(_defaultIdentity.gaiaId);
    return std::ranges::contains(
        _identityManager->GetAccountsOnDevice(), gaia,
        [](const AccountInfo& info) { return info.gaia; });
  }
  return false;
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
  CHECK(identity, base::NotFatalUntil::M147);
  item.gaiaID = identity.gaiaId;
  item.name = identity.userFullName;
  item.email = identity.userEmail;
  item.selected = _defaultIdentity.gaiaId == identity.gaiaId;
  item.avatar =
      GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
          identity, IdentityAvatarSize::Regular);

  if (std::optional<BOOL> isManaged = IsIdentityManaged(identity);
      isManaged.has_value()) {
    item.managed = isManaged.value();
  } else {
    __weak __typeof(self) weakSelf = self;
    FetchManagedStatusForIdentity(
        identity, base::BindOnce(^(bool managed) {
          if (managed) {
            CHECK(identity, base::NotFatalUntil::M147);
            [weakSelf updateTableViewIdentityItem:item withIdentity:identity];
          }
        }));
  }

  [self.consumer itemHasChanged:item];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(info.gaia);
  CHECK(identity, base::NotFatalUntil::M147);
  TableViewIdentityItem* item =
      [self.consumer tableViewIdentityItemWithGaiaID:identity.gaiaId];
  [self updateTableViewIdentityItem:item withIdentity:identity];
}

- (void)onAccountsOnDeviceChanged {
  if (!_accountManagerService || !_identityManager) {
    return;
  }

  [self loadIdentitySection];
  // Update the selection, in case no identity was chosen yet, or the default
  // identity has become unavailable (probably removed from the device).
  if (![self defaultIdentityIsValid]) {
    _defaultIdentity = signin::GetDefaultIdentityOnDevice(
        _identityManager, _accountManagerService);
  }
}

@end
