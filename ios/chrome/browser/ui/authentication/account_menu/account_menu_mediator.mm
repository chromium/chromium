// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator.h"

#import <optional>
#import <string>

#import "base/functional/callback_helpers.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
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
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"

@interface AccountMenuMediator () <ChromeAccountManagerServiceObserver,
                                   IdentityManagerObserverBridgeDelegate,
                                   SyncObserverModelBridge>

// Whether the account menu’s interaction is blocked.
@property(nonatomic, assign) BOOL userInteractionsBlocked;

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
  // The authentication flow,
  AuthenticationFlow* _authenticationFlow;
  // This object is set iff an account switch is in progress.
  base::ScopedClosureRunner _accountSwitchInProgress;

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
    _userInteractionsBlocked = NO;
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
    // By default, if the mediator was not involved in stopping the account
    // menu, it mean the coordinator was directly interupted.
    self.signinCoordinatorResult = SigninCoordinatorResultInterrupted;
    _signinCompletionInfo =
        [SigninCompletionInfo signinCompletionInfoWithIdentity:nil];
    _error = GetAccountErrorUIInfo(_syncService);
  }
  return self;
}

- (void)disconnect {
  _accountSwitchInProgress.RunAndReset();
  _signinCompletionInfo = nil;
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
      self.signinCoordinatorResult =
          SigninCoordinatorResult::SigninCoordinatorResultInterrupted;
      _blockUpdates = YES;
      self.userInteractionsBlocked = YES;
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
  self.userInteractionsBlocked = YES;
  self.signinCoordinatorResult =
      SigninCoordinatorResult::SigninCoordinatorResultCanceledByUser;
  [_delegate mediatorWantsToBeDismissed:self];
}

- (void)signOutFromTargetRect:(CGRect)targetRect {
  if (self.userInteractionsBlocked) {
    return;
  }
  if (![self.delegate blockOtherScenesIfPossible]) {
    // This scene is currently blocked. Abort signout.
    return;
  }
  _blockUpdates = YES;
  self.userInteractionsBlocked = YES;
  __weak __typeof(self) weakSelf = self;
  [self.delegate signOutFromTargetRect:targetRect
                             forSwitch:NO
                            completion:^(BOOL success) {
                              [weakSelf signoutEndedWithSuccess:success];
                            }];
}

- (void)accountTappedWithGaiaID:(NSString*)gaiaID
                     targetRect:(CGRect)targetRect {
  if (self.userInteractionsBlocked) {
    return;
  }
  [self.consumer switchingStarted];
  [self.delegate blockOtherScenesIfPossible];
  _blockUpdates = YES;
  self.userInteractionsBlocked = YES;
  id<SystemIdentity> newIdentity = nil;
  for (id<SystemIdentity> identity : _identities) {
    if (identity.gaiaID == gaiaID) {
      newIdentity = identity;
      break;
    }
  }
  CHECK(newIdentity);
  _accountSwitchInProgress =
      _authenticationService->DeclareAccountSwitchInProgress();
  __weak __typeof(self) weakSelf = self;
  id<SystemIdentity> fromIdentity = _primaryIdentity;
  [self.delegate signOutFromTargetRect:targetRect
                             forSwitch:YES
                            completion:^(BOOL success) {
                              [weakSelf signoutEndedWithSuccess:success
                                                   fromIdentity:fromIdentity
                                                     toIdentity:newIdentity];
                            }];
}

- (void)didTapErrorButton {
  if (self.userInteractionsBlocked) {
    return;
  }
  switch (_error.errorType) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate: {
      if (_authenticationService->HasCachedMDMErrorForIdentity(
              _primaryIdentity)) {
        base::RecordAction(
            base::UserMetricsAction("Signin_AccountMenu_ErrorButton_MDM"));
        [self.delegate openMDMErrodDialogWithSystemIdentity:_primaryIdentity];
      } else {
        base::RecordAction(
            base::UserMetricsAction("Signin_AccountMenu_ErrorButton_Reauth"));
        [self.delegate openPrimaryAccountReauthDialog];
      }
      break;
    }
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      base::RecordAction(
          base::UserMetricsAction("Signin_AccountMenu_ErrorButton_Passphrase"));
      [self.delegate openPassphraseDialogWithModalPresentation:YES];
      break;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      base::RecordAction(base::UserMetricsAction(
          "Signin_AccountMenu_ErrorButton_TrustedVaultForPasswords"));
      [self.delegate openTrustedVaultReauthForFetchKeys];
      break;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      base::RecordAction(base::UserMetricsAction(
          "Signin_AccountMenu_ErrorButton_TrustedVaultForEverything"));
      [self.delegate openTrustedVaultReauthForFetchKeys];
      break;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      base::RecordAction(base::UserMetricsAction(
          "Signin_AccountMenu_ErrorButton_TrustedVaultDegradedForPasswords"));
      [self.delegate openTrustedVaultReauthForDegradedRecoverability];
      break;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      base::RecordAction(base::UserMetricsAction(
          "Signin_AccountMenu_ErrorButton_TrustedVaultDegradedForEverything"));
      [self.delegate openTrustedVaultReauthForDegradedRecoverability];
      break;
    case syncer::SyncService::UserActionableError::kNone:
      NOTREACHED();
  }
}

- (void)didTapManageYourGoogleAccount {
  if (self.userInteractionsBlocked) {
    return;
  }
  [self.delegate didTapManageYourGoogleAccount];
}

- (void)didTapManageAccounts {
  if (self.userInteractionsBlocked) {
    return;
  }
  [self.delegate didTapManageAccounts];
}

- (void)didTapAddAccount {
  if (self.userInteractionsBlocked) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  self.userInteractionsBlocked = YES;
  [self.delegate
      didTapAddAccountWithCompletion:^(SigninCoordinatorResult result,
                                       SigninCompletionInfo* info) {
        [weakSelf accountAddedIsDone];
      }];
}

#pragma mark - Callbacks

// Callback for didTapAddAccount
- (void)accountAddedIsDone {
  [self restartUpdates];
  self.userInteractionsBlocked = NO;
}

// Callback for signout.
- (void)signoutEndedWithSuccess:(BOOL)success {
  [self.delegate unblockOtherScenes];
  if (success) {
    // By signing-out the user cancelled the option to signin in this menu.
    self.signinCoordinatorResult = SigninCoordinatorResultCanceledByUser;
    [_delegate mediatorWantsToBeDismissed:self];
  } else {
    // User had not signed-out. Allow to interact with the UI.
    self.userInteractionsBlocked = NO;
    [self restartUpdates];
  }
}

// Callback for the first part of the switch, which is a sign-out.
- (void)signoutEndedWithSuccess:(BOOL)signoutSuccess
                   fromIdentity:(id<SystemIdentity>)previousIdentity
                     toIdentity:(id<SystemIdentity>)newIdentity {
  if (!signoutSuccess) {
    // User had not signed-out. Allow to interact with the UI.
    [self.delegate unblockOtherScenes];
    self.userInteractionsBlocked = NO;
    _accountSwitchInProgress.RunAndReset();
    [self restartUpdates];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  _authenticationFlow = [self.delegate
      triggerSigninWithSystemIdentity:newIdentity
                           completion:^(SigninCoordinatorResult result) {
                             [weakSelf signinEndedWithResult:result
                                                fromIdentity:previousIdentity
                                                  toIdentity:newIdentity];
                           }];
}

- (void)signinEndedWithResult:(SigninCoordinatorResult)result
                 fromIdentity:(id<SystemIdentity>)previousIdentity
                   toIdentity:(id<SystemIdentity>)newIdentity {
  CHECK(_authenticationFlow);
  _authenticationFlow = nil;
  _accountSwitchInProgress.RunAndReset();
  [self.delegate unblockOtherScenes];
  BOOL success =
      result == SigninCoordinatorResult::SigninCoordinatorResultSuccess;
  if (success) {
    _signinCompletionInfo =
        [SigninCompletionInfo signinCompletionInfoWithIdentity:newIdentity];
    self.signinCoordinatorResult = result;
    [_delegate triggerAccountSwitchSnackbarWithIdentity:newIdentity];
    [_delegate mediatorWantsToBeDismissed:self];
  } else if (_accountManagerService->IsValidIdentity(previousIdentity)) {
    // If the sign-in failed, sign back in previous account if possible and
    // restart using the account menu.
    _authenticationService->SignIn(
        previousIdentity,
        signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU_FAILED_SWITCH);
    self.userInteractionsBlocked = NO;
    [self restartUpdates];
  } else {
    self.signinCoordinatorResult = result;
    [_delegate mediatorWantsToBeDismissed:self];
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
  NSMutableArray<NSString*>* gaiaIDsToKeep = [NSMutableArray array];
  for (id<SystemIdentity> secondaryIdentity : allIdentities) {
    NSString* gaiaID = secondaryIdentity.gaiaID;
    if (secondaryIdentity == _primaryIdentity) {
      continue;
    }
    BOOL mustAdd = YES;
    for (id<SystemIdentity> displayedIdentity : _identities) {
      if (gaiaID == displayedIdentity.gaiaID) {
        [gaiaIDsToKeep addObject:gaiaID];
        mustAdd = NO;
        break;
      }
    }
    if (mustAdd) {
      [_identities addObject:secondaryIdentity];
      [gaiaIDsToAdd addObject:gaiaID];
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
                                   gaiaIDsToRemove:gaiaIDsToRemove
                                     gaiaIDsToKeep:gaiaIDsToKeep];
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
  [self.consumer switchingStopped];
  _blockUpdates = NO;
  [self updateIdentities];
  [self onSyncStateChanged];
}

- (void)setUserInteractionsBlocked:(BOOL)blocked {
  _userInteractionsBlocked = blocked;
  [self.consumer setUserInteractionsEnabled:!blocked];
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
