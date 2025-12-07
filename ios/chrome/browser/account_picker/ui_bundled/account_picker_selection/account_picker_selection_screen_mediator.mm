// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_consumer.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@interface AccountPickerSelectionScreenMediator () <
    IdentityManagerObserverBridgeDelegate>

@end

@implementation AccountPickerSelectionScreenMediator {
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Configurators based on identity list.
  __strong NSArray* _sortedIdentityItemConfigurators;
}

- (instancetype)initWithSelectedIdentity:(id<SystemIdentity>)selectedIdentity
                         identityManager:
                             (signin::IdentityManager*)identityManager
                   accountManagerService:
                       (ChromeAccountManagerService*)accountManagerService {
  if ((self = [super init])) {
    CHECK(identityManager);
    CHECK(accountManagerService);
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _accountManagerService = accountManagerService;
    _selectedIdentity = selectedIdentity;
    [self loadIdentityItemConfigurators];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_accountManagerService);
  DCHECK(!_identityManager);
}

- (void)disconnect {
  _accountManagerService = nullptr;
  _identityManagerObserver.reset();
  _identityManager = nullptr;
}

#pragma mark - Properties

- (void)setSelectedIdentity:(id<SystemIdentity>)identity {
  CHECK(identity, base::NotFatalUntil::M147);
  if ([_selectedIdentity isEqual:identity]) {
    return;
  }
  id<SystemIdentity> previousSelectedIdentity = _selectedIdentity;
  _selectedIdentity = identity;
  [self handleIdentityUpdated:previousSelectedIdentity];
  [self handleIdentityUpdated:_selectedIdentity];
}

#pragma mark - Private

// Updates `_sortedIdentityItemConfigurators` based on identity list.
- (void)loadIdentityItemConfigurators {
  if (!_accountManagerService || !_identityManager) {
    return;
  }

  NSMutableArray* configurators = [NSMutableArray array];
  NSArray<id<SystemIdentity>>* identitiesOnDevice =
      signin::GetIdentitiesOnDevice(_identityManager, _accountManagerService);
  BOOL hasSelectedIdentity = NO;
  for (id<SystemIdentity> identity in identitiesOnDevice) {
    AccountPickerSelectionScreenIdentityItemConfigurator* configurator =
        [[AccountPickerSelectionScreenIdentityItemConfigurator alloc] init];
    [self updateIdentityItemConfigurator:configurator withIdentity:identity];
    [configurators addObject:configurator];
    if (configurator.selected) {
      hasSelectedIdentity = YES;
    }
    // If the configurator is selected, the identity must be equal to
    // `self.selectedIdentity`.
    DCHECK(!configurator.selected || [self.selectedIdentity isEqual:identity]);
  }
  if (!hasSelectedIdentity && identitiesOnDevice.count > 0) {
    // No selected identity has been found, a default needs to be selected.
    self.selectedIdentity = identitiesOnDevice[0];
    AccountPickerSelectionScreenIdentityItemConfigurator* configurator =
        configurators[0];
    configurator.selected = YES;
  }
  _sortedIdentityItemConfigurators = configurators;
}

// Updates `configurator` based on `identity`.
- (void)updateIdentityItemConfigurator:
            (AccountPickerSelectionScreenIdentityItemConfigurator*)configurator
                          withIdentity:(id<SystemIdentity>)identity {
  CHECK(identity, base::NotFatalUntil::M147);
  configurator.gaiaID = identity.gaiaId;
  configurator.name = identity.userFullName;
  configurator.email = identity.userEmail;
  configurator.avatar =
      GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
          identity, IdentityAvatarSize::TableViewIcon);
  configurator.selected = [identity isEqual:self.selectedIdentity];

  if (std::optional<BOOL> isManaged = IsIdentityManaged(identity);
      isManaged.has_value()) {
    configurator.managed = isManaged.value();
    return;
  }
  configurator.managed = NO;
  __weak __typeof(self) weakSelf = self;
  FetchManagedStatusForIdentity(identity, base::BindOnce(^(bool managed) {
                                  CHECK(identity, base::NotFatalUntil::M147);
                                  if (managed) {
                                    [weakSelf handleIdentityUpdated:identity];
                                  }
                                }));
}

- (void)handleIdentityUpdated:(id<SystemIdentity>)identity {
  CHECK(identity, base::NotFatalUntil::M147);
  AccountPickerSelectionScreenIdentityItemConfigurator* configurator = nil;
  for (AccountPickerSelectionScreenIdentityItemConfigurator* cursor in self
           .sortedIdentityItemConfigurators) {
    if (cursor.gaiaID == identity.gaiaId) {
      configurator = cursor;
    }
  }
  DCHECK(configurator);
  [self updateIdentityItemConfigurator:configurator withIdentity:identity];
  [self.consumer reloadIdentityForIdentityItemConfigurator:configurator];
}

#pragma mark -  IdentityManagerObserver

- (void)onAccountsOnDeviceChanged {
  [self loadIdentityItemConfigurators];
  [self.consumer reloadAllIdentities];
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(info.gaia);
  CHECK(identity, base::NotFatalUntil::M147);
  [self handleIdentityUpdated:identity];
}

#pragma mark - AccountPickerSelectionScreenTableViewControllerModelDelegate

- (NSArray*)sortedIdentityItemConfigurators {
  if (!_sortedIdentityItemConfigurators) {
    [self loadIdentityItemConfigurators];
  }
  DCHECK(_sortedIdentityItemConfigurators);
  return _sortedIdentityItemConfigurators;
}

@end
