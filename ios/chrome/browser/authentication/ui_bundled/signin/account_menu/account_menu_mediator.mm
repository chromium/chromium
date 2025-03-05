// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_mediator.h"

#import <optional>
#import <string>

#import "base/functional/callback_helpers.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_data_source.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"

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
    [self updateIdentitiesIfAllowed];
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

- (BOOL)isGaiaIDManaged:(NSString*)gaiaID {
  id<SystemIdentity> identity = [self identityForGaiaID:gaiaID];
  if (std::optional<BOOL> managed = IsIdentityManaged(identity);
      managed.has_value()) {
    return managed.value();
  }

  __weak __typeof(self) weakSelf = self;
  FetchManagedStatusForIdentity(identity, base::BindOnce(^(bool managed) {
                                  if (managed) {
                                    [weakSelf updateIdentitiesIfAllowed];
                                  }
                                }));
  return NO;
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
  if (IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `onAccountsOnDeviceChanged` instead.
    return;
  }
  [self handleIdentityListChanged];
}

- (void)identityUpdated:(id<SystemIdentity>)identity {
  if (IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `onExtendedAccountInfoUpdated` instead.
    return;
  }
  [self handleIdentityUpdated];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40067367): This method can be removed once
  // crbug.com/40067367 is fixed.
  [self disconnect];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfPrimaryAccountChanges {
  if (_blockUpdates) {
    return;
  }
  id<SystemIdentity> primaryIdentity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (primaryIdentity) {
    _primaryIdentity = primaryIdentity;
    [self updateIdentitiesIfAllowed];
    return;
  }
  // The user is not signed anymore. The account menu can be stopped.
  // The old value of `_primaryIdentity` can be kept during the shutdown.
  _blockUpdates = YES;
  self.userInteractionsBlocked = YES;
  [self.delegate mediatorWantsToBeDismissed:self
                                 withResult:SigninCoordinatorResultInterrupted
                             signedIdentity:nil];
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  if (!IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `identityUpdated` instead.
    return;
  }
  [self handleIdentityUpdated];
}

- (void)onAccountsOnDeviceChanged {
  if (!IsUseAccountListFromIdentityManagerEnabled()) {
    // Listening to `identityListChanged` instead.
    return;
  }
  [self handleIdentityListChanged];
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
  [_delegate mediatorWantsToBeDismissed:self
                             withResult:SigninCoordinatorResultCanceledByUser
                         signedIdentity:nil];
}

- (void)signOutFromTargetRect:(CGRect)targetRect {
  if (self.userInteractionsBlocked) {
    return;
  }
  _blockUpdates = YES;
  self.userInteractionsBlocked = YES;
  __weak __typeof(self) weakSelf = self;
  [self.delegate signOutFromTargetRect:targetRect
                            completion:^(BOOL success) {
                              [weakSelf signoutEndedWithSuccess:success];
                            }];
}

- (void)accountTappedWithGaiaID:(NSString*)gaiaID
                     targetRect:(CGRect)targetRect {
  if (self.userInteractionsBlocked) {
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

  [self.consumer switchingStarted];
  _blockUpdates = YES;
  self.userInteractionsBlocked = YES;

  __weak __typeof(self) weakSelf = self;
  id<SystemIdentity> fromIdentity = _primaryIdentity;
  _authenticationFlow = [self.delegate
      triggerSigninWithSystemIdentity:newIdentity
                           anchorRect:targetRect
                           completion:^(SigninCoordinatorResult result) {
                             [weakSelf signinEndedWithResult:result
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
  [self.delegate didTapAddAccountWithCompletion:^(SigninCoordinatorResult,
                                                  id<SystemIdentity>) {
    [weakSelf accountAddedIsDone];
  }];
}

- (void)didTapSettingsButton {
  if (self.userInteractionsBlocked) {
    return;
  }
  [self.delegate didTapSettingsButton];
}

#pragma mark - Callbacks

// Callback for didTapAddAccount
- (void)accountAddedIsDone {
  [self restartUpdates];
  self.userInteractionsBlocked = NO;
}

// Callback for signout.
- (void)signoutEndedWithSuccess:(BOOL)success {
  if (success) {
    // By signing-out the user cancelled the option to signin in this menu.
    // TODO(crbug.com/400715119): Should consider add a signout result in
    // SigninCoordinatorResult.
    [_delegate mediatorWantsToBeDismissed:self
                               withResult:SigninCoordinatorResultCanceledByUser
                           signedIdentity:nil];
  } else {
    // User had not signed-out. Allow to interact with the UI.
    self.userInteractionsBlocked = NO;
    [self restartUpdates];
  }
}

- (void)signinEndedWithResult:(SigninCoordinatorResult)result
                 fromIdentity:(id<SystemIdentity>)previousIdentity
                   toIdentity:(id<SystemIdentity>)newIdentity {
  CHECK(_authenticationFlow);
  _authenticationFlow = nil;
  BOOL success =
      result == SigninCoordinatorResult::SigninCoordinatorResultSuccess;
  if (success) {
    [_delegate triggerAccountSwitchSnackbarWithIdentity:newIdentity];
    [_delegate mediatorWantsToBeDismissed:self
                               withResult:result
                           signedIdentity:newIdentity];
  } else if (_accountManagerService->IsValidIdentity(previousIdentity)) {
    // If the sign-in failed, sign back in previous account if possible and
    // restart using the account menu.
    _authenticationService->SignIn(
        previousIdentity,
        signin_metrics::AccessPoint::kAccountMenuFailedSwitch);
    self.userInteractionsBlocked = NO;
    [self restartUpdates];
  } else {
    [_delegate mediatorWantsToBeDismissed:self
                               withResult:result
                           signedIdentity:nil];
  }
}

#pragma mark - Private

- (void)handleIdentityListChanged {
  [self updateIdentitiesIfAllowed];
}

- (void)handleIdentityUpdated {
  [self updateIdentitiesIfAllowed];
}

// Updates the identity list in `_identities`, and sends an notification to
// the consumer.
- (void)updateIdentitiesIfAllowed {
  if (_blockUpdates) {
    return;
  }

  NSArray<id<SystemIdentity>>* identitiesOnDevice =
      signin::GetIdentitiesOnDevice(_identityManager, _accountManagerService);

  NSMutableArray<NSString*>* gaiaIDsToRemove = [NSMutableArray array];
  NSMutableArray<NSString*>* gaiaIDsToAdd = [NSMutableArray array];
  NSMutableArray<NSString*>* gaiaIDsToKeep = [NSMutableArray array];
  for (id<SystemIdentity> secondaryIdentity : identitiesOnDevice) {
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
    if (![identitiesOnDevice containsObject:identity] ||
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
  [self updateIdentitiesIfAllowed];
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
