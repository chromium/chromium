// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_consumer.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"

@interface AccountPickerSelectionScreenMediator () <
    ChromeAccountManagerServiceObserver>

@end

@implementation AccountPickerSelectionScreenMediator {
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  // Configurators based on identity list.
  __strong NSArray* _sortedIdentityItemConfigurators;
}

- (instancetype)initWithSelectedIdentity:(id<SystemIdentity>)selectedIdentity
                   accountManagerService:
                       (ChromeAccountManagerService*)accountManagerService {
  if ((self = [super init])) {
    DCHECK(accountManagerService);
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
}

- (void)disconnect {
  _accountManagerServiceObserver.reset();
  _accountManagerService = nullptr;
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
  if (!_accountManagerService) {
    return;
  }

  NSMutableArray* configurators = [NSMutableArray array];
  NSArray<id<SystemIdentity>>* identities =
      _accountManagerService->GetAllIdentities();
  BOOL hasSelectedIdentity = NO;
  for (id<SystemIdentity> identity in identities) {
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
  if (!hasSelectedIdentity && identities.count > 0) {
    // No selected identity has been found, a default need to be selected.
    self.selectedIdentity = identities[0];
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

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
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

- (void)identityListChanged {
  [self loadIdentityItemConfigurators];
  [self.consumer reloadAllIdentities];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40284086): Remove `[self disconnect]`.
  [self disconnect];
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
