// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator.h"

#import <optional>
#import <string>

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_consumer.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_data_source.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
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
  raw_ptr<PrefService> _prefs;
  raw_ptr<syncer::SyncService> _syncService;
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // The primary identity.
  id<SystemIdentity> _primaryIdentity;
  // The displayed error, if any.
  AccountErrorUIInfo* _error;
  // Whether the UI should not update anymore.
  BOOL _blockUpdates;
  // Whether the account menu operations requires the user interacitons to be
  // ignored.
  BOOL _blockUserInteractions;

  // The list of identities to display and their index in the table view’s
  // identities section
  NSMutableArray<id<SystemIdentity>>* _identities;

  // The type of account error that is being displayed in the error section for
  // signed in accounts. Is set to kNone when there is no error section.
  syncer::SyncService::UserActionableError _diplayedAccountErrorType;

  // Records the displayed primary account info by the view. Used to limit the
  // view updates to only when one of these values is updated.
  NSString* _primaryAccountDisplayedEmail;
  NSString* _primaryAccountDisplayedUserFullName;
  UIImage* _primaryAccountDisplayedAvatar;
  BOOL _primaryAccountDisplayedManaged;
}

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
                        authService:(AuthenticationService*)authService
                    identityManager:(signin::IdentityManager*)identityManager
                              prefs:(PrefService*)prefs {
  self = [super init];
  if (self) {
    CHECK(syncService);
    CHECK(accountManagerService);
    CHECK(authService);
    CHECK(identityManager);
    _blockUpdates = NO;
    _blockUserInteractions = NO;
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
    _prefs = prefs;
    _primaryIdentity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    _syncService = syncService;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, _syncService);
    _diplayedAccountErrorType = syncer::SyncService::UserActionableError::kNone;
    [self updateIdentities];
    _error = GetAccountErrorUIInfo(_syncService);
  }
  return self;
}

- (void)disconnect {
  _blockUpdates = YES;
  _accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
  _authenticationService = nullptr;
  _identityManagerObserver.reset();
  _identityManager = nullptr;
  _prefs = nullptr;
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

- (NSString*)nameForGaiaID:(NSString*)gaiaID {
  return [self identityForGaiaID:gaiaID].userFullName;
}

- (NSString*)emailForGaiaID:(NSString*)gaiaID {
  return [self identityForGaiaID:gaiaID].userEmail;
}

- (UIImage*)imageForGaiaID:(NSString*)gaiaID {
  return _accountManagerService->GetIdentityAvatarWithIdentity(
      [self identityForGaiaID:gaiaID], IdentityAvatarSize::TableViewIcon);
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

- (ManagementState)managementState {
  return GetManagementState(_identityManager, _authenticationService, _prefs);
}

- (AccountErrorUIInfo*)accountErrorUIInfo {
  return _error;
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityListChanged {
  if (_blockUpdates) {
    return;
  }
  [self updateIdentities];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if (_blockUpdates) {
    return;
  }
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
  if (_blockUpdates) {
    return;
  }
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      _primaryIdentity = _authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin);
      [self updateIdentities];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      if (_authenticationService->IsAccountSwitchInProgress()) {
        return;
      }
      [self.delegate mediatorWantsToBeDismissed:self];
      break;
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (_blockUpdates) {
    return;
  }
  AccountErrorUIInfo* newError = GetAccountErrorUIInfo(_syncService);
  if (newError == _error) {
    return;
  }
  _error = newError;
  [self.consumer updateErrorSection:_error];
}

#pragma mark - AccountMenuMutator

// The user tapped the close button.
- (void)viewControllerWantsToBeClosed:
    (AccountMenuViewController*)viewController {
  CHECK_EQ(viewController, _consumer);
  _blockUserInteractions = YES;
  [_delegate mediatorWantsToBeDismissed:self];
}

- (void)signOutFromTargetRect:(CGRect)targetRect {
  if (_blockUserInteractions) {
    return;
  }
  _blockUpdates = YES;
  _blockUserInteractions = YES;
  [self.delegate blockOtherScene];
  __weak __typeof(self) weakSelf = self;
  [self.delegate signOutFromTargetRect:targetRect
                              callback:^(BOOL success) {
                                [weakSelf signoutEndedWithSuccess:success];
                              }];
}

- (void)accountTappedWithGaiaID:(NSString*)gaiaID
                     targetRect:(CGRect)targetRect {
  if (_blockUserInteractions) {
    return;
  }
  _blockUpdates = YES;
  _blockUserInteractions = YES;
  id<SystemIdentity> newIdentity = nil;
  for (id<SystemIdentity> identity : _identities) {
    if (identity.gaiaID == gaiaID) {
      newIdentity = identity;
      break;
    }
  }
  CHECK(newIdentity);

  BOOL viewWillBeDismissedAfterSignout =
      _authenticationService->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSignin);
  __weak __typeof(self) weakSelf = self;
  void (^userDecisionCompletion)() = ^() {
    [weakSelf.delegate mediatorWantsToDismissTheView:weakSelf];
  };
  void (^signinCompletion)(SigninCoordinatorResult result,
                           SigninCompletionInfo* info) =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        BOOL success =
            result == SigninCoordinatorResult::SigninCoordinatorResultSuccess;
        [weakSelf signinEndedWithSuccess:success];
      };
  [self.delegate
      triggerAccountSwitchWithTargetRect:targetRect
                             newIdentity:newIdentity
         viewWillBeDismissedAfterSignout:viewWillBeDismissedAfterSignout
                  userDecisionCompletion:userDecisionCompletion
                        signInCompletion:signinCompletion];
}

- (void)didTapErrorButton {
  if (_blockUserInteractions) {
    return;
  }
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

- (void)didTapManageYourGoogleAccount {
  if (_blockUserInteractions) {
    return;
  }
  [self.delegate didTapManageYourGoogleAccount];
}

- (void)didTapEditAccountList {
  if (_blockUserInteractions) {
    return;
  }
  [self.delegate didTapEditAccountList];
}

- (void)didTapAddAccount {
  if (_blockUserInteractions) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  _blockUserInteractions = YES;
  [self.delegate didTapAddAccount:^(SigninCoordinatorResult result,
                                    SigninCompletionInfo* info) {
    [weakSelf accountAddedIsDone];
  }];
}

#pragma mark - Callbacks

// Callback for didTapAddAccount
- (void)accountAddedIsDone {
  [self restartUpdates];
  _blockUserInteractions = NO;
}

// Callback for signout.
- (void)signoutEndedWithSuccess:(BOOL)success {
  [self.delegate unblockOtherScene];
  if (!success) {
    // User had not signed-out. Allow to interact with the UI.
    _blockUserInteractions = NO;
    [self restartUpdates];
  }
}

- (void)signinEndedWithSuccess:(BOOL)success {
  if (success) {
    [_delegate mediatorWantsToBeDismissed:self];
  } else if (_authenticationService->GetPrimaryIdentity(
                 signin::ConsentLevel::kSignin)) {
    // Sign in to the new identity failed, and the user was signed back.
    [self restartUpdates];
    _blockUserInteractions = NO;
  } else {
    // That should be extremely are. MDM invalidated the previous account
    // during the switch.
    [self.delegate mediatorWantsToBeDismissed:self];
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
  if ([self primaryAccountInfoChanged]) {
    [self.consumer updatePrimaryAccount];
  }
}

// Refresh everything and update the UI according to the change in the state. Do
// nothing if the mediator was disconnected.
- (void)restartUpdates {
  if ([self isDisconnected]) {
    // The mediator was disconnected. Don’t restart updates.
    return;
  }
  _blockUpdates = NO;
  [self updateIdentities];
  [self onSyncStateChanged];
}

- (id<SystemIdentity>)identityForGaiaID:(NSString*)gaiaID {
  for (id<SystemIdentity> identity : _identities) {
    if (gaiaID == identity.gaiaID) {
      return identity;
    }
  }
  NOTREACHED();
}

// Updates the displayed values, and returns YES if the primary account info
// changed from the displayed ones. Otherwise returns NO.
- (BOOL)primaryAccountInfoChanged {
  if (_primaryAccountDisplayedAvatar != self.primaryAccountAvatar ||
      _primaryAccountDisplayedUserFullName != self.primaryAccountUserFullName ||
      _primaryAccountDisplayedEmail != self.primaryAccountEmail ||
      _primaryAccountDisplayedManaged !=
          self.managementState.is_profile_managed()) {
    [self recordPrimaryAccountDisplayedInfo];
    return YES;
  }
  return NO;
}

// Records the displayed primary account info.
- (void)recordPrimaryAccountDisplayedInfo {
  _primaryAccountDisplayedEmail = self.primaryAccountEmail;
  _primaryAccountDisplayedUserFullName = self.primaryAccountUserFullName;
  _primaryAccountDisplayedAvatar = self.primaryAccountAvatar;
  _primaryAccountDisplayedManaged = self.managementState.is_profile_managed();
}

// Returns whether this mediator is disconnected
- (BOOL)isDisconnected {
  // The account manager service is set in init and reset in `disconnect`. So
  // this property correctly reflects whether the mediator is disconnected.
  return !_accountManagerService;
}

@end
