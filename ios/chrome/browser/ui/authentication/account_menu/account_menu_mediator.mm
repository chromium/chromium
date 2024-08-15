// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_consumer.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_data_source.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"

@interface AccountMenuMediator () <ChromeAccountManagerServiceObserver,
                                   IdentityManagerObserverBridgeDelegate,
                                   SyncObserverModelBridge>

@end

@implementation AccountMenuMediator {
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Chrome account manager service observer bridge.
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  raw_ptr<syncer::SyncService> _syncService;
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // The primary identity.
  id<SystemIdentity> _primaryIdentity;
  // The displayed error, if any.
  AccountErrorUIInfo* _error;

  // The list of identities to display and their index in the table view’s
  // identities section
  NSMutableArray<id<SystemIdentity>>* _identities;

  // The type of account error that is being displayed in the error section for
  // signed in accounts. Is set to kNone when there is no error section.
  syncer::SyncService::UserActionableError _diplayedAccountErrorType;

  // Whether an account switching is in progress.
  BOOL _accountSwitchingInProgress;
}

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
                        authService:(AuthenticationService*)authService
                    identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    CHECK(syncService);
    CHECK(accountManagerService);
    CHECK(authService);
    CHECK(identityManager);
    _identities = [NSMutableArray array];
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _authenticationService = authService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _primaryIdentity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    _syncService = syncService;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, _syncService);
    _diplayedAccountErrorType = syncer::SyncService::UserActionableError::kNone;
    _primaryIdentity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    [self updateIdentities];
    _error = GetAccountErrorUIInfo(_syncService);
  }
  return self;
}

- (void)disconnect {
  _accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
  _authenticationService = nullptr;
  _identityManagerObserver.reset();
  _identityManager = nullptr;
  _syncObserver.reset();
  _syncService = nullptr;
  _identities = nil;
  _primaryIdentity = nullptr;
}

#pragma mark - AccountMenuDataSource

- (NSArray<NSString*>*)secondaryAccountsGaiaIDs {
  NSMutableArray<NSString*>* gaiaIDs = [NSMutableArray array];
  for (id<SystemIdentity> identity : _identities) {
    [gaiaIDs addObject:identity.gaiaID];
  }
  return gaiaIDs;
}

- (TableViewAccountItem*)identityItemForGaiaID:(NSString*)gaiaID {
  for (id<SystemIdentity> identity : _identities) {
    if (gaiaID == identity.gaiaID) {
      TableViewAccountItem* item =
          [[TableViewAccountItem alloc] initWithType:SettingsItemTypeAccount];
      item.text = identity.userFullName;
      item.detailText = identity.userEmail;
      item.image = _accountManagerService->GetIdentityAvatarWithIdentity(
          identity, IdentityAvatarSize::TableViewIcon);
      return item;
    }
  }
  NOTREACHED();
}

- (NSString*)primaryAccountEmail {
  return _primaryIdentity.userEmail;
}

- (NSString*)primaryAccountUserFullName {
  return _primaryIdentity.userFullName;
}

- (UIImage*)primaryAccountAvatar {
  return _accountManagerService->GetIdentityAvatarWithIdentity(
      _primaryIdentity, IdentityAvatarSize::Large);
}

- (AccountErrorUIInfo*)accountErrorUIInfo {
  return _error;
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  [self updateIdentities];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  [self updateIdentities];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40067367): This method can be removed once
  // crbug.com/40067367 is fixed.
  [self disconnect];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      _primaryIdentity = _authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin);
      [self updateIdentities];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      if (_accountSwitchingInProgress) {
        return;
      }
      [self.delegate mediatorWantsToBeDismissed:self];
      break;
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  AccountErrorUIInfo* newError = GetAccountErrorUIInfo(_syncService);
  if (newError == _error) {
    return;
  }
  _error = newError;
  [self.consumer updateErrorSection:_error];
}

#pragma mark - AccountMenuMutator

- (void)accountTappedWithGaiaID:(NSString*)gaiaID
                     targetRect:(CGRect)targetRect {
  if (_accountSwitchingInProgress || self.signOutFlowInProgress) {
    return;
  }
  id<SystemIdentity> newIdentity = nil;
  for (id<SystemIdentity> identity : _identities) {
    if (identity.gaiaID == gaiaID) {
      newIdentity = identity;
      break;
    }
  }
  CHECK(newIdentity);
  _accountSwitchingInProgress = YES;
  __weak __typeof(self) weakSelf = self;
  [self.delegate triggerSignoutWithTargetRect:targetRect
                                   completion:^(BOOL success) {
                                     [weakSelf
                                         signoutDoneWithSuccess:success
                                                 systemIdentity:newIdentity];
                                   }];
}

- (void)didTapErrorButton {
  switch (_error.errorType) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate: {
      if (_authenticationService->HasCachedMDMErrorForIdentity(
              _primaryIdentity)) {
        [self.delegate openMDMErrodDialogWithSystemIdentity:_primaryIdentity];
      } else {
        [self.delegate openPrimaryAccountReauthDialog];
      }
      break;
    }
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      [self.delegate openPassphraseDialogWithModalPresentation:YES];
      break;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      [self.delegate openTrustedVaultReauthForFetchKeys];
      break;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      [self.delegate openTrustedVaultReauthForDegradedRecoverability];
      break;
    case syncer::SyncService::UserActionableError::kNone:
      NOTREACHED_IN_MIGRATION();
  }
}

#pragma mark - Private

// Updates the identity list in `_identities`, and sends an notification to
// the consumer.
- (void)updateIdentities {
  NSArray<id<SystemIdentity>>* allIdentities =
      _accountManagerService->GetAllIdentities();

  NSMutableArray<NSString*>* gaiaIDsToRemove = [NSMutableArray array];
  NSMutableArray<NSString*>* gaiaIDsToAdd = [NSMutableArray array];

  for (id<SystemIdentity> secondaryIdentity : allIdentities) {
    if (secondaryIdentity == _primaryIdentity) {
      continue;
    }
    BOOL mustAdd = YES;
    for (id<SystemIdentity> displayedIdentity : _identities) {
      if (secondaryIdentity.gaiaID == displayedIdentity.gaiaID) {
        mustAdd = NO;
        break;
      }
    }
    if (mustAdd) {
      [_identities addObject:secondaryIdentity];
      [gaiaIDsToAdd addObject:secondaryIdentity.gaiaID];
    }
  }

  for (NSUInteger i = 0; i < _identities.count; ++i) {
    id<SystemIdentity> identity = _identities[i];
    if (![allIdentities containsObject:identity] ||
        identity == _primaryIdentity) {
      [gaiaIDsToRemove addObject:identity.gaiaID];
      [_identities removeObjectAtIndex:i--];
      // There will be a new object at place `i`. So we must decrease `i`.
    }
  }

  [self.consumer updateAccountListWithGaiaIDsToAdd:gaiaIDsToAdd
                                   gaiaIDsToRemove:gaiaIDsToRemove];
  // In case the primary account information changed.
  [self.consumer updatePrimaryAccount];
}

- (void)signoutDoneWithSuccess:(BOOL)success
                systemIdentity:(id<SystemIdentity>)systemIdentity {
  if (!success) {
    _accountSwitchingInProgress = NO;
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [self.delegate
      triggerSigninWithSystemIdentity:systemIdentity
                           completion:^(id<SystemIdentity> signedInIdentity) {
                             [weakSelf signinDone:signedInIdentity];
                           }];
}

- (void)signinDone:(id<SystemIdentity>)systemIdentity {
  _accountSwitchingInProgress = NO;
  [_delegate triggerAccountSwitchSnackbarWithIdentity:systemIdentity];
  [_delegate mediatorWantsToBeDismissed:self];
}

@end
