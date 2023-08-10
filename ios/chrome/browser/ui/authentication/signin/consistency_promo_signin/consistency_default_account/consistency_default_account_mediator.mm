// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_mediator.h"

#import <UIKit/UIKit.h>

#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_consumer.h"

@interface ConsistencyDefaultAccountMediator () <
    ChromeAccountManagerServiceObserver> {
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
}

@property(nonatomic, strong) UIImage* avatar;
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
@property(nonatomic, assign) syncer::SyncService* syncService;

@end

@implementation ConsistencyDefaultAccountMediator

- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                                  syncService:
                                      (syncer::SyncService*)syncService {
  if (self = [super init]) {
    DCHECK(accountManagerService);
    _accountManagerService = accountManagerService;
    _syncService = syncService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.accountManagerService);
}

- (void)disconnect {
  self.accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
}

#pragma mark - Properties

- (void)setConsumer:(id<ConsistencyDefaultAccountConsumer>)consumer {
  _consumer = consumer;

  syncer::UserSelectableTypeSet disabledTypes;
  syncer::SyncUserSettings* syncSettings = _syncService->GetUserSettings();
  for (syncer::UserSelectableType type :
       syncSettings->GetRegisteredSelectableTypes()) {
    if (syncSettings->IsTypeManagedByPolicy(type)) {
      disabledTypes.Put(type);
    }
  }
  [_consumer setSyncTypesDisabledByPolicy:disabledTypes];
  [_consumer setSyncTransportDisabledByPolicy:
                 _syncService->HasDisableReason(
                     syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY)];

  [self selectSelectedIdentity];
}

- (void)setSelectedIdentity:(id<SystemIdentity>)identity {
  if (!IsConsistencyNewAccountInterfaceEnabled()) {
    DCHECK(identity);
  }
  if ([_selectedIdentity isEqual:identity]) {
    return;
  }
  _selectedIdentity = identity;
  [self updateSelectedIdentityUI];
}

#pragma mark - Private

// Updates the default identity, or hide the default identity if there isn't
// one present on the device.
- (void)selectSelectedIdentity {
  if (!self.accountManagerService) {
    return;
  }

  id<SystemIdentity> identity =
      self.accountManagerService->GetDefaultIdentity();

  if (!IsConsistencyNewAccountInterfaceEnabled() && !identity) {
    [self.delegate consistencyDefaultAccountMediatorNoIdentities:self];
    return;
  }

  // Here, default identity may be nil.
  self.selectedIdentity = identity;
}

// Updates the view controller using the default identity, or hide the default
// identity button if no identity is present on device.
- (void)updateSelectedIdentityUI {
  if (!IsConsistencyNewAccountInterfaceEnabled()) {
    DCHECK(self.selectedIdentity);
  }

  if (!self.selectedIdentity) {
    [self.consumer hideDefaultAccount];
    return;
  }

  id<SystemIdentity> selectedIdentity = self.selectedIdentity;
  UIImage* avatar = self.accountManagerService->GetIdentityAvatarWithIdentity(
      selectedIdentity, IdentityAvatarSize::TableViewIcon);
  [self.consumer showDefaultAccountWithFullName:selectedIdentity.userFullName
                                      givenName:selectedIdentity.userGivenName
                                          email:selectedIdentity.userEmail
                                         avatar:avatar];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  [self selectSelectedIdentity];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if ([self.selectedIdentity isEqual:identity]) {
    [self updateSelectedIdentityUI];
  }
}

@end
