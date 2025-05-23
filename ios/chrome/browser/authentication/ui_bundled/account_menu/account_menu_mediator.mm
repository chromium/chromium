// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_mediator.h"

#import <optional>
#import <string>

#import "base/functional/callback_helpers.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_data_source.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_load_url.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_open_ntp.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_settings_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"

@interface AccountMenuMediator () <AuthenticationFlowDelegate,
                                   IdentityManagerObserverBridgeDelegate,
                                   SyncObserverModelBridge>

// Whether the account menu’s interaction is blocked.
@property(nonatomic, assign) BOOL userInteractionsBlocked;

@end

@implementation AccountMenuMediator {
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  raw_ptr<PrefService> _prefs;
  // The access point from which this account menu was triggered.
  AccountMenuAccessPoint _accessPoint;
  raw_ptr<syncer::SyncService> _syncService;
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // The primary identity. During an authentication flow, it contains the
  // previous identity.
  id<SystemIdentity> _primaryIdentityBeforeSignin;
  // The displayed error, if any.
  AccountErrorUIInfo* _error;
  // Whether the UI should not update anymore.
  BOOL _blockUpdates;
  // The authentication flow,
  AuthenticationFlow* _authenticationFlow;

  // The list of identities to display and their index in the table view’s
  // identities section
  NSMutableArray<id<SystemIdentity>>* _identities;

  // Records the displayed primary account info by the view. Used to limit the
  // view updates to only when one of these values is updated.
  NSString* _primaryAccountDisplayedEmail;
  NSString* _primaryAccountDisplayedUserFullName;
  UIImage* _primaryAccountDisplayedAvatar;
  // If the authentication flow started, the identity is switching to this
  // profile.
  id<SystemIdentity> _identityToSignin;
  // The URL which the the account menu was viewed from when
  // AccountMenuAccessPoint::kWeb.
  GURL _url;
  // Block to execute before a change in profile when
  // AccountMenuAccessPoint::kWeb.
  ProceduralBlock _prepareChangeProfile;
}

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
                        authService:(AuthenticationService*)authService
                    identityManager:(signin::IdentityManager*)identityManager
                              prefs:(PrefService*)prefs
                        accessPoint:(AccountMenuAccessPoint)accessPoint
                                URL:(const GURL&)url
               prepareChangeProfile:(ProceduralBlock)prepareChangeProfile {
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
    _authenticationService = authService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _prefs = prefs;
    _accessPoint = accessPoint;
    _url = url;
    _prepareChangeProfile = prepareChangeProfile;
    _primaryIdentityBeforeSignin = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    _syncService = syncService;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, _syncService);
    [self updateIdentitiesIfAllowed];
    _error = GetAccountErrorUIInfo(_syncService);
  }
  return self;
}

- (void)disconnect {
  _blockUpdates = YES;
  _accountManagerService = nullptr;
  _authenticationService = nullptr;
  _identityManagerObserver.reset();
  _identityManager = nullptr;
  _prefs = nullptr;
  _syncObserver.reset();
  _syncService = nullptr;
  _identities = nil;
  _primaryIdentityBeforeSignin = nullptr;
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
  return _primaryIdentityBeforeSignin.userEmail;
}

- (NSString*)primaryAccountUserFullName {
  return _primaryIdentityBeforeSignin.userFullName;
}

- (UIImage*)primaryAccountAvatar {
  return _accountManagerService->GetIdentityAvatarWithIdentity(
      _primaryIdentityBeforeSignin, IdentityAvatarSize::Large);
}

- (NSString*)managementDescription {
  return GetManagementDescription(
      GetManagementState(_identityManager, _authenticationService, _prefs));
}

- (AccountErrorUIInfo*)accountErrorUIInfo {
  return _error;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfPrimaryAccountChanges {
  if (_blockUpdates) {
    return;
  }
  id<SystemIdentity> primaryIdentity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (primaryIdentity) {
    _primaryIdentityBeforeSignin = primaryIdentity;
    [self updateIdentitiesIfAllowed];
    return;
  }
  // The user is not signed anymore. The account menu can be stopped.
  // The old value of `_primaryIdentityBeforeSignin` can be kept during the
  // shutdown.
  _blockUpdates = YES;
  self.userInteractionsBlocked = YES;
  [self.delegate mediatorWantsToBeDismissed:self
                                 withResult:SigninCoordinatorResultInterrupted
                             signedIdentity:nil
                            userTappedClose:NO];
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  [self updateIdentitiesIfAllowed];
}

- (void)onAccountsOnDeviceChanged {
  [self updateIdentitiesIfAllowed];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (_blockUpdates) {
    return;
  }
  AccountErrorUIInfo* newError = GetAccountErrorUIInfo(_syncService);
  if (_error == newError || [newError isEqual:_error]) {
    // The first disjunct is necessary for the case when both values are `nil`.
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
                         signedIdentity:nil
                        userTappedClose:YES];
}

- (void)signOutFromTargetRect:(CGRect)targetRect {
  if (self.userInteractionsBlocked) {
    return;
  }
  _blockUpdates = YES;
  self.userInteractionsBlocked = YES;
  __weak __typeof(self) weakSelf = self;
  [self.delegate
      signOutFromTargetRect:targetRect
                 completion:^(BOOL success, SceneState* scene_state) {
                   [weakSelf signoutEndedWithSuccess:success];
                 }];
}

- (void)accountTappedWithGaiaID:(NSString*)gaiaID
                     targetRect:(CGRect)targetRect {
  if (self.userInteractionsBlocked) {
    return;
  }

  CHECK(!_identityToSignin, base::NotFatalUntil::M140);
  for (id<SystemIdentity> identity : _identities) {
    if (identity.gaiaID == gaiaID) {
      _identityToSignin = identity;
      break;
    }
  }
  CHECK(_identityToSignin);
  [self.consumer switchingStarted];
  _blockUpdates = YES;
  self.userInteractionsBlocked = YES;

  _authenticationFlow = [self.delegate authenticationFlow:_identityToSignin
                                               anchorRect:targetRect];
  _authenticationFlow.delegate = self;
  [_authenticationFlow startSignIn];
}

- (void)didTapErrorButton {
  if (self.userInteractionsBlocked) {
    return;
  }
  switch (_error.errorType) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate: {
      if (_authenticationService->HasCachedMDMErrorForIdentity(
              _primaryIdentityBeforeSignin)) {
        base::RecordAction(
            base::UserMetricsAction("Signin_AccountMenu_ErrorButton_MDM"));
        [self.syncErrorSettingsCommandHandler
            openMDMErrodDialogWithSystemIdentity:_primaryIdentityBeforeSignin];
      } else {
        base::RecordAction(
            base::UserMetricsAction("Signin_AccountMenu_ErrorButton_Reauth"));
        [self.syncErrorSettingsCommandHandler openPrimaryAccountReauthDialog];
      }
      break;
    }
    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      base::RecordAction(
          base::UserMetricsAction("Signin_AccountMenu_ErrorButton_Passphrase"));
      [self.syncErrorSettingsCommandHandler
          openPassphraseDialogWithModalPresentation:YES];
      break;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
      base::RecordAction(base::UserMetricsAction(
          "Signin_AccountMenu_ErrorButton_TrustedVaultForPasswords"));
      [self.syncErrorSettingsCommandHandler openTrustedVaultReauthForFetchKeys];
      break;
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      base::RecordAction(base::UserMetricsAction(
          "Signin_AccountMenu_ErrorButton_TrustedVaultForEverything"));
      [self.syncErrorSettingsCommandHandler openTrustedVaultReauthForFetchKeys];
      break;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      base::RecordAction(base::UserMetricsAction(
          "Signin_AccountMenu_ErrorButton_TrustedVaultDegradedForPasswords"));
      [self.syncErrorSettingsCommandHandler
              openTrustedVaultReauthForDegradedRecoverability];
      break;
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      base::RecordAction(base::UserMetricsAction(
          "Signin_AccountMenu_ErrorButton_TrustedVaultDegradedForEverything"));
      [self.syncErrorSettingsCommandHandler
              openTrustedVaultReauthForDegradedRecoverability];
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
  self.userInteractionsBlocked = YES;
  [self.delegate didTapAddAccount];
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
                           signedIdentity:nil
                          userTappedClose:NO];
  } else {
    // User had not signed-out. Allow to interact with the UI.
    self.userInteractionsBlocked = NO;
    [self restartUpdates];
  }
}

#pragma mark - AuthenticationFlowDelegate

- (void)authenticationFlowDidSignInInSameProfileWithResult:
    (SigninCoordinatorResult)result {
  [_delegate signinFinished];
  if (_accessPoint == AccountMenuAccessPoint::kWeb &&
      result == SigninCoordinatorResultSuccess) {
    GetApplicationContext()->GetLocalState()->SetBoolean(
        prefs::kHasSwitchedAccountsViaWebFlow, true);
  }
  if (!_syncService) {
    // The mediator was disconnected. No need to update it.
    return;
  }
  CHECK(_identityToSignin, base::NotFatalUntil::M140);
  CHECK(_primaryIdentityBeforeSignin, base::NotFatalUntil::M140);
  _authenticationFlow = nil;
  BOOL success =
      result == SigninCoordinatorResult::SigninCoordinatorResultSuccess;
  if (success) {
    [_delegate mediatorWantsToBeDismissed:self
                               withResult:result
                           signedIdentity:_identityToSignin
                          userTappedClose:NO];
  } else if (_accountManagerService->IsValidIdentity(
                 _primaryIdentityBeforeSignin)) {
    // If the sign-in failed, sign back in previous account if possible and
    // restart using the account menu.
    _authenticationService->SignIn(
        _primaryIdentityBeforeSignin,
        signin_metrics::AccessPoint::kAccountMenuFailedSwitch);
    self.userInteractionsBlocked = NO;
    [self restartUpdates];
  } else {
    [_delegate mediatorWantsToBeDismissed:self
                               withResult:result
                           signedIdentity:nil
                          userTappedClose:NO];
  }
  _identityToSignin = nil;
}

- (ChangeProfileContinuation)authenticationFlowWillChangeProfile {
  _authenticationFlow = nil;
  [_delegate signinFinished];
  switch (_accessPoint) {
    case AccountMenuAccessPoint::kNewTabPage:
      return CreateChangeProfileOpensNTPContinuation();
    case AccountMenuAccessPoint::kSettings:
      return CreateChangeProfileSettingsContinuation();
    case AccountMenuAccessPoint::kWeb: {
      GetApplicationContext()->GetLocalState()->SetBoolean(
          prefs::kHasSwitchedAccountsViaWebFlow, true);
      if (_prepareChangeProfile) {
        _prepareChangeProfile();
      };
      return CreateChangeProfileOpensURLContinuation(_url);
    }
  }
}

#pragma mark - Private

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
    if (secondaryIdentity == _primaryIdentityBeforeSignin) {
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
        identity == _primaryIdentityBeforeSignin) {
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
      _primaryAccountDisplayedEmail != self.primaryAccountEmail) {
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
}

// Returns whether this mediator is disconnected
- (BOOL)isDisconnected {
  // The account manager service is set in init and reset in `disconnect`. So
  // this property correctly reflects whether the mediator is disconnected.
  return !_accountManagerService;
}

@end
