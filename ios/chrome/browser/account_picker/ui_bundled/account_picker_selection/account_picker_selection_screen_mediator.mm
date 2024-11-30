// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_consumer.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

@interface AccountPickerSelectionScreenMediator () <
    ChromeAccountManagerServiceObserver,
    IdentityManagerObserverBridgeDelegate>

@end

@implementation AccountPickerSelectionScreenMediator {
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
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
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
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
  _accountManagerServiceObserver.reset();
  _accountManagerService = nullptr;
  _identityManagerObserver.reset();
  _identityManager = nullptr;
}

#pragma mark - Properties

- (void)setSelectedIdentity:(id<SystemIdentity>)identity {
  DCHECK(identity);
  if ([_selectedIdentity isEqual:identity]) {
    return;
  }
  id<SystemIdentity> previousSelectedIdentity = _selectedIdentity;
  _selectedIdentity = identity;
  [self identityUpdated:previousSelectedIdentity];
  [self identityUpdated:_selectedIdentity];
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
  configurator.gaiaID = identity.gaiaID;
  configurator.name = identity.userFullName;
  configurator.email = identity.userEmail;
  configurator.avatar = _accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::TableViewIcon);
  configurator.selected = [identity isEqual:self.selectedIdentity];
}

- (void)handleIdentityListChanged {
  [self loadIdentityItemConfigurators];
  [self.consumer reloadAllIdentities];
}

- (void)handleIdentityUpdated:(id<SystemIdentity>)identity {
  AccountPickerSelectionScreenIdentityItemConfigurator* configurator = nil;
  for (AccountPickerSelectionScreenIdentityItemConfigurator* cursor in self
           .sortedIdentityItemConfigurators) {
    if ([cursor.gaiaID isEqualToString:identity.gaiaID]) {
      configurator = cursor;
    }
  }
  DCHECK(configurator);
  [self updateIdentityItemConfigurator:configurator withIdentity:identity];
  [self.consumer reloadIdentityForIdentityItemConfigurator:configurator];
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

#pragma mark -  IdentityManagerObserver

- (void)onAccountsOnDeviceChanged {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `identityListChanged` instead.
    return;
  }
  [self handleIdentityListChanged];
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    // Listening to `identityUpdated` instead.
    return;
  }
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(info.gaia);
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
