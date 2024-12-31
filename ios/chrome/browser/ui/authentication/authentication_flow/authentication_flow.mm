// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow/authentication_flow.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/account_pref_utils.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow/authentication_flow_performer.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_capabilities_fetcher.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"
#import "ui/base/l10n/l10n_util.h"

using signin_ui::SigninCompletionCallback;

namespace {

// The states of the sign-in flow state machine.
enum AuthenticationState {
  BEGIN,
  CHECK_SIGNIN_STEPS,
  FETCH_MANAGED_STATUS,
  SHOW_MANAGED_CONFIRMATION,
  CONVERT_PERSONAL_PROFILE_TO_MANAGED,
  SIGN_OUT_IF_NEEDED,
  SIGN_IN,
  REGISTER_FOR_USER_POLICY,
  FETCH_USER_POLICY,
  FETCH_CAPABILITIES,
  COMPLETE_WITH_SUCCESS,
  COMPLETE_WITH_FAILURE,
  CLEANUP_BEFORE_DONE,
  DONE
};

// Values of Signin.AccountType histogram. This histogram records if the user
// uses a gmail account or a managed account when signing in.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with SigninAccountType in
// tools/metrics/histograms/metadata/signin/enums.xml.
enum class SigninAccountType {
  // Gmail account.
  kRegular = 0,
  // Managed account.
  kManaged = 1,
  // Always the last enumerated type.
  kMaxValue = kManaged,
};

enum class CancelationReason {
  // Not canceled.
  kNotCanceled,
  // Canceled by the user.
  kUserCanceled,
  // Canceled, but not by the user.
  kFailed,
};

// Returns YES if the `identity.gaiaID` is in one of the AccountInfo of
// `account_infos`.
BOOL IsIdentityInAccountInfos(id<SystemIdentity> identity,
                              const std::vector<AccountInfo>& account_infos) {
  std::string gaia_id = base::SysNSStringToUTF8(identity.gaiaID);
  for (const auto& account_info : account_infos) {
    if (account_info.gaia == gaia_id) {
      return YES;
    }
  }
  return NO;
}

// Returns YES if the `identity.gaiaID` is in one of the CoreAccountInfo of
// `core_account_infos`.
BOOL IsIdentityInCoreAccountInfos(
    id<SystemIdentity> identity,
    const std::vector<CoreAccountInfo>& core_account_infos) {
  std::string gaia_id = base::SysNSStringToUTF8(identity.gaiaID);
  for (const auto& core_account_info : core_account_infos) {
    if (core_account_info.gaia == gaia_id) {
      return YES;
    }
  }
  return NO;
}

}  // namespace

@interface AuthenticationFlow ()

// Whether this flow is curently handling an error.
@property(nonatomic, assign) BOOL handlingError;

// The actions to perform following account sign-in.
@property(nonatomic, assign) PostSignInActionSet postSignInActions;

@end

@implementation AuthenticationFlow {
  UIViewController* _presentingViewController;
  SigninCompletionCallback _signInCompletion;
  AuthenticationFlowPerformer* _performer;

  // State machine tracking.
  AuthenticationState _state;
  BOOL _didSignIn;
  CancelationReason _cancelationReason;
  BOOL _shouldSignOut;
  // YES if the personal profile should be converted to a managed (work) profile
  // as part of the signin flow. Can only be true if the to-be-signed-in account
  // is managed.
  BOOL _shouldConvertPersonalProfileToManaged;
  // YES if user is opted into bookmark and reading list account storage.
  BOOL _shouldShowSigninSnackbar;

  raw_ptr<Browser> _browser;
  id<SystemIdentity> _identityToSignIn;
  signin_metrics::AccessPoint _accessPoint;
  NSString* _identityToSignInHostedDomain;

  // Token to have access to user policies from dmserver.
  NSString* _dmToken;
  // ID of the client that is registered for user policy.
  NSString* _clientID;
  // List of IDs that represents the domain of the user. The list will be used
  // to compare with a similiar list from device mangement to understand whether
  // user and device are managed by the same domain.
  NSArray<NSString*>* _userAffiliationIDs;

  // This AuthenticationFlow keeps a reference to `self` while a sign-in flow is
  // is in progress to ensure it outlives any attempt to destroy it in
  // `_signInCompletion`.
  AuthenticationFlow* _selfRetainer;

  // Capabilities fetcher for the subsequent History Sync Opt-In screen.
  HistorySyncCapabilitiesFetcher* _capabilitiesFetcher;
}

@synthesize handlingError = _handlingError;
@synthesize identity = _identityToSignIn;

#pragma mark - Public methods

- (instancetype)initWithBrowser:(Browser*)browser
                       identity:(id<SystemIdentity>)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
              postSignInActions:(PostSignInActionSet)postSignInActions
       presentingViewController:(UIViewController*)presentingViewController {
  if ((self = [super init])) {
    DCHECK(browser);
    DCHECK(presentingViewController);
    DCHECK(identity);
    _browser = browser;
    _identityToSignIn = identity;
    _accessPoint = accessPoint;
    _postSignInActions = postSignInActions;
    _presentingViewController = presentingViewController;
    _state = BEGIN;
    _cancelationReason = CancelationReason::kNotCanceled;
  }
  return self;
}

- (void)startSignInWithCompletion:(SigninCompletionCallback)completion {
  DCHECK_EQ(BEGIN, _state);
  DCHECK(!_signInCompletion);
  DCHECK(completion);
  _signInCompletion = [completion copy];
  _selfRetainer = self;
  // Kick off the state machine.
  if (!_performer) {
    id<ChangeProfileCommands> changeProfileHandler = HandlerForProtocol(
        _browser->GetSceneState().profileState.appState.appCommandDispatcher,
        ChangeProfileCommands);
    _performer = [[AuthenticationFlowPerformer alloc]
            initWithDelegate:self
        changeProfileHandler:changeProfileHandler];
  }
  // Make sure -[AuthenticationFlow startSignInWithCompletion:] doesn't call
  // the completion block synchronously.
  // Related to http://crbug.com/1246480.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf continueFlow];
  });
}

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action {
  if (_state == DONE) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [_performer interruptWithAction:action
                       completion:^() {
                         [weakSelf performerInterrupted];
                       }];
}

- (void)performerInterrupted {
  if (_state != DONE) {
    // The performer might not have been able to continue the flow if it was
    // waiting for a callback (e.g. waiting for AccountReconcilor). In this
    // case, we force the flow to finish synchronously.
    [self cancelFlowWithReason:CancelationReason::kFailed];
  }
  DCHECK_EQ(DONE, _state);
}

- (void)setPresentingViewController:
    (UIViewController*)presentingViewController {
  _presentingViewController = presentingViewController;
}

#pragma mark - State machine management

- (AuthenticationState)nextStateFailedOrCanceled {
  DCHECK([self canceled]);
  switch (_state) {
    case BEGIN:
    case CHECK_SIGNIN_STEPS:
    case FETCH_MANAGED_STATUS:
    case SHOW_MANAGED_CONFIRMATION:
    case CONVERT_PERSONAL_PROFILE_TO_MANAGED:
    case SIGN_OUT_IF_NEEDED:
    case SIGN_IN:
    case REGISTER_FOR_USER_POLICY:
    case FETCH_USER_POLICY:
      return COMPLETE_WITH_FAILURE;
    case FETCH_CAPABILITIES:
      return COMPLETE_WITH_FAILURE;
    case COMPLETE_WITH_SUCCESS:
    case COMPLETE_WITH_FAILURE:
      return CLEANUP_BEFORE_DONE;
    case CLEANUP_BEFORE_DONE:
    case DONE:
      return DONE;
  }
}

- (AuthenticationState)nextState {
  DCHECK(!self.handlingError);
  if ([self canceled]) {
    return [self nextStateFailedOrCanceled];
  }
  DCHECK(![self canceled]);
  switch (_state) {
    case BEGIN:
      return CHECK_SIGNIN_STEPS;
    case CHECK_SIGNIN_STEPS:
      return FETCH_MANAGED_STATUS;
    case FETCH_MANAGED_STATUS:
      if (ShouldShowManagedConfirmationForHostedDomain(
              _identityToSignInHostedDomain, _accessPoint,
              _identityToSignIn.gaiaID, [self prefs])) {
        return SHOW_MANAGED_CONFIRMATION;
      } else if (_shouldSignOut) {
        return SIGN_OUT_IF_NEEDED;
      } else {
        return SIGN_IN;
      }
    case SHOW_MANAGED_CONFIRMATION:
      if (_shouldConvertPersonalProfileToManaged) {
        return CONVERT_PERSONAL_PROFILE_TO_MANAGED;
      } else if (_shouldSignOut) {
        return SIGN_OUT_IF_NEEDED;
      } else {
        return SIGN_IN;
      }
    case CONVERT_PERSONAL_PROFILE_TO_MANAGED:
      return SIGN_IN;
    case SIGN_OUT_IF_NEEDED:
      return SIGN_IN;
    case SIGN_IN:
      if (self.postSignInActions.Has(PostSignInAction::kShowSnackbar)) {
        _shouldShowSigninSnackbar = YES;
      }
      if (policy::IsAnyUserPolicyFeatureEnabled() &&
          _identityToSignInHostedDomain.length > 0) {
        return REGISTER_FOR_USER_POLICY;
      } else if ([self shouldFetchCapabilities]) {
        return FETCH_CAPABILITIES;
      } else {
        return COMPLETE_WITH_SUCCESS;
      }
    case REGISTER_FOR_USER_POLICY:
      if (!_dmToken.length || !_clientID.length) {
        // Skip fetching user policies when registration failed.
        if ([self shouldFetchCapabilities]) {
          return FETCH_CAPABILITIES;
        } else {
          return COMPLETE_WITH_SUCCESS;
        }
      }
      // Fetch user policies when registration is successful.
      return FETCH_USER_POLICY;
    case FETCH_USER_POLICY:
      if ([self shouldFetchCapabilities]) {
        return FETCH_CAPABILITIES;
      } else {
        return COMPLETE_WITH_SUCCESS;
      }
    case FETCH_CAPABILITIES:
      return COMPLETE_WITH_SUCCESS;
    case COMPLETE_WITH_SUCCESS:
    case COMPLETE_WITH_FAILURE:
      return CLEANUP_BEFORE_DONE;
    case CLEANUP_BEFORE_DONE:
    case DONE:
      return DONE;
  }
}

// Continues the sign-in state machine starting from `_state` and invokes
// `_signInCompletion` when finished.
- (void)continueFlow {
  ProfileIOS* profile = [self originalProfile];
  if (self.handlingError) {
    // The flow should not continue while the error is being handled, e.g. while
    // the user is being informed of an issue.
    return;
  }
  _state = [self nextState];
  switch (_state) {
    case BEGIN:
      NOTREACHED();

    case CHECK_SIGNIN_STEPS:
      [self checkSigninSteps];
      [self continueFlow];
      return;

    case FETCH_MANAGED_STATUS:
      [_performer fetchManagedStatus:profile forIdentity:_identityToSignIn];
      return;

    case SHOW_MANAGED_CONFIRMATION: {
      [_performer
          showManagedConfirmationForHostedDomain:_identityToSignInHostedDomain
                                       userEmail:_identityToSignIn.userEmail
                                  viewController:_presentingViewController
                                         browser:_browser
                       skipBrowsingDataMigration:_accessPoint ==
                                                 signin_metrics::AccessPoint::
                                                     ACCESS_POINT_START_PAGE];
      return;
    }

    case CONVERT_PERSONAL_PROFILE_TO_MANAGED: {
      [_performer makePersonalProfileManagedWithIdentity:_identityToSignIn];
      return;
    }

    case SIGN_OUT_IF_NEEDED:
      // TODO(crbug.com/375605482): skip sign out if there is a profile
      // switching.
      [_performer signOutProfile:profile];
      return;

    case SIGN_IN:
      [self multiProfileSignIn];
      return;

    case REGISTER_FOR_USER_POLICY:
      [_performer registerUserPolicy:profile forIdentity:_identityToSignIn];
      return;

    case FETCH_USER_POLICY:
      [_performer fetchUserPolicy:profile
                      withDmToken:_dmToken
                         clientID:_clientID
               userAffiliationIDs:_userAffiliationIDs
                         identity:_identityToSignIn];
      return;
    case FETCH_CAPABILITIES:
      [self fetchCapabilities];
      return;
    case COMPLETE_WITH_SUCCESS:
      [self completeSignInWithResult:SigninCoordinatorResult::
                                         SigninCoordinatorResultSuccess];
      return;
    case COMPLETE_WITH_FAILURE:
      if (_didSignIn) {
        [_performer signOutImmediatelyFromProfile:profile];
      }
      SigninCoordinatorResult result;
      switch (_cancelationReason) {
        case CancelationReason::kFailed:
          result = SigninCoordinatorResult::SigninCoordinatorResultInterrupted;
          break;
        case CancelationReason::kUserCanceled:
          result =
              SigninCoordinatorResult::SigninCoordinatorResultCanceledByUser;
          break;
        case CancelationReason::kNotCanceled:
          NOTREACHED();
      }
      [self completeSignInWithResult:result];
      return;
    case CLEANUP_BEFORE_DONE: {
      // Clean up asynchronously to ensure that `self` does not die while
      // the flow is running.
      DCHECK([NSThread isMainThread]);
      dispatch_async(dispatch_get_main_queue(), ^{
        self->_selfRetainer = nil;
      });
      [self continueFlow];
      return;
    }
    case DONE:
      return;
  }
  NOTREACHED();
}

// Checks which sign-in steps to perform and updates member variables
// accordingly.
- (void)checkSigninSteps {
  id<SystemIdentity> currentIdentity =
      AuthenticationServiceFactory::GetForProfile([self originalProfile])
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (currentIdentity && ![currentIdentity isEqual:_identityToSignIn]) {
    // If the identity to sign-in is different than the current identity,
    // sign-out is required.
    _shouldSignOut = YES;
  }
}

- (void)multiProfileSignIn {
  ProfileIOS* profile = [self originalProfile];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  BOOL isValidIdentityInProfile = NO;
  BOOL isValidIdentityOnDevice = NO;
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    isValidIdentityOnDevice = IsIdentityInAccountInfos(
        _identityToSignIn, identityManager->GetAccountsOnDevice());
    isValidIdentityInProfile = IsIdentityInCoreAccountInfos(
        _identityToSignIn, identityManager->GetAccountsWithRefreshTokens());
  } else {
    isValidIdentityOnDevice = isValidIdentityInProfile =
        accountManagerService->IsValidIdentity(_identityToSignIn);
  }

  if (isValidIdentityInProfile) {
    [self signInInCurrentProfile];
  } else if (isValidIdentityOnDevice) {
    CHECK(AreSeparateProfilesForManagedAccountsEnabled());
    NSString* sceneIdentifier = _browser->GetSceneState().sceneSessionID;
    __weak __typeof(self) weakSelf = self;
    OnProfileSwitchCompletion completion = base::BindOnce(
        [](__typeof(self) strong_self, bool success,
           Browser* new_profile_browser, UIViewController* view_controller) {
          [strong_self onSwitchToProfileWithSuccess:success
                                  newProfileBrowser:new_profile_browser
                                     viewController:view_controller];
        },
        weakSelf);
    [_performer switchToProfileWithIdentity:_identityToSignIn
                            sceneIdentifier:sceneIdentifier
                                 completion:std::move(completion)];
  } else {
    // Handle the case where the identity is no longer valid.
    NSError* error = ios::provider::CreateMissingIdentitySigninError();
    [self handleAuthenticationError:error];
  }
}

// Fetches capabilities on successful authentication for the upcoming History
// Sync Opt-In screen.
- (void)fetchCapabilities {
  CHECK([self shouldFetchCapabilities]);
  ProfileIOS* profile = [self originalProfile];

  // Create the capability fetcher and start fetching capabilities.
  __weak __typeof(self) weakSelf = self;
  _capabilitiesFetcher = [[HistorySyncCapabilitiesFetcher alloc]
      initWithIdentityManager:IdentityManagerFactory::GetForProfile(profile)];

  [_capabilitiesFetcher
      startFetchingRestrictionCapabilityWithCallback:base::BindOnce(^(
                                                         signin::Tribool
                                                             capability) {
        // The capability value is ignored.
        [weakSelf continueFlow];
      })];
}

// Runs `_signInCompletion` asynchronously with `result` argument.
- (void)completeSignInWithResult:(SigninCoordinatorResult)result {
  DCHECK(_signInCompletion)
      << "`completeSignInWithResult` should not be called twice.";
  if (result == SigninCoordinatorResult::SigninCoordinatorResultSuccess) {
    base::UmaHistogramEnumeration("Signin.AccountType.SigninConsent",
                                  _identityToSignInHostedDomain.length > 0
                                      ? SigninAccountType::kManaged
                                      : SigninAccountType::kRegular);
  }
  if (_signInCompletion) {
    SigninCompletionCallback signInCompletion = _signInCompletion;
    _signInCompletion = nil;
    signInCompletion(result);
  }
  if (_shouldShowSigninSnackbar) {
    [_performer completePostSignInActions:_postSignInActions
                             withIdentity:_identityToSignIn
                                  browser:_browser];
  }
  [self continueFlow];
}

- (BOOL)canceled {
  return _cancelationReason != CancelationReason::kNotCanceled;
}

// Cancels the current sign-in flow.
- (void)cancelFlowWithReason:(CancelationReason)reason {
  CHECK_NE(reason, CancelationReason::kNotCanceled);
  if ([self canceled]) {
    // Avoid double handling of cancel or error.
    return;
  }
  _cancelationReason = reason;
  [self continueFlow];
}

// Handles an authentication error and show an alert to the user.
- (void)handleAuthenticationError:(NSError*)error {
  if ([self canceled]) {
    // Avoid double handling of cancel or error.
    return;
  }
  DCHECK(error);
  _cancelationReason = CancelationReason::kFailed;
  self.handlingError = YES;
  __weak AuthenticationFlow* weakSelf = self;
  [_performer showAuthenticationError:error
                       withCompletion:^{
                         AuthenticationFlow* strongSelf = weakSelf;
                         if (!strongSelf) {
                           return;
                         }
                         [strongSelf setHandlingError:NO];
                         [strongSelf continueFlow];
                       }
                       viewController:_presentingViewController
                              browser:_browser];
}

#pragma mark AuthenticationFlowPerformerDelegate

- (void)didSignOut {
  [self continueFlow];
}

- (void)didClearData {
  [self continueFlow];
}

- (void)didFetchManagedStatus:(NSString*)hostedDomain {
  DCHECK_EQ(FETCH_MANAGED_STATUS, _state);
  _identityToSignInHostedDomain = hostedDomain;
  [self continueFlow];
}

- (void)didFailFetchManagedStatus:(NSError*)error {
  DCHECK_EQ(FETCH_MANAGED_STATUS, _state);
  NSError* flowError =
      [NSError errorWithDomain:kAuthenticationErrorDomain
                          code:AUTHENTICATION_FLOW_ERROR
                      userInfo:@{
                        NSLocalizedDescriptionKey :
                            l10n_util::GetNSString(IDS_IOS_SIGN_IN_FAILED),
                        NSUnderlyingErrorKey : error
                      }];
  [self handleAuthenticationError:flowError];
}

- (void)didAcceptManagedConfirmation:(BOOL)keepBrowsingDataSeparate {
  if (base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)) {
    // Only show the dialog once per account.
    signin::GaiaIdHash gaiaIDHash = signin::GaiaIdHash::FromGaiaId(
        base::SysNSStringToUTF8(_identityToSignIn.gaiaID));
    syncer::SetAccountKeyedPrefValue([self prefs],
                                     prefs::kSigninHasAcceptedManagementDialog,
                                     gaiaIDHash, base::Value(true));
  }

  _shouldConvertPersonalProfileToManaged =
      AreSeparateProfilesForManagedAccountsEnabled() &&
      (!keepBrowsingDataSeparate ||
       _accessPoint == signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE);

  [self continueFlow];
}

- (void)didCancelManagedConfirmation {
  [self cancelFlowWithReason:CancelationReason::kUserCanceled];
}

- (void)didRegisterForUserPolicyWithDMToken:(NSString*)dmToken
                                   clientID:(NSString*)clientID
                         userAffiliationIDs:
                             (NSArray<NSString*>*)userAffiliationIDs {
  DCHECK_EQ(REGISTER_FOR_USER_POLICY, _state);

  _dmToken = dmToken;
  _clientID = clientID;
  _userAffiliationIDs = userAffiliationIDs;
  [self continueFlow];
}

- (void)didFetchUserPolicyWithSuccess:(BOOL)success {
  DCHECK_EQ(FETCH_USER_POLICY, _state);
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  [self continueFlow];
}

- (void)didMakePersonalProfileManaged {
  [self continueFlow];
}

#pragma mark - Private methods

// The original profile used for services that don't exist in incognito mode.
- (ProfileIOS*)originalProfile {
  if (!_browser) {
    return nullptr;
  }
  return _browser->GetProfile()->GetOriginalProfile();
}

- (PrefService*)prefs {
  return [self originalProfile]->GetPrefs();
}

// Return YES if capabilities should be fetched for the History Sync screen.
- (BOOL)shouldFetchCapabilities {
  if (!self.precedingHistorySync) {
    return NO;
  }

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile([self originalProfile]);
  syncer::SyncUserSettings* userSettings = syncService->GetUserSettings();

  if (userSettings->GetSelectedTypes().HasAll(
          {syncer::UserSelectableType::kHistory,
           syncer::UserSelectableType::kTabs})) {
    // History Opt-In is already set and the screen won't be shown.
    return NO;
  }

  return YES;
}

// Called when the profile switching succeeded or failed (according to
// `success`).
- (void)onSwitchToProfileWithSuccess:(BOOL)success
                   newProfileBrowser:(Browser*)newProfileBrowser
                      viewController:(UIViewController*)viewController {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  if (success) {
    _browser = newProfileBrowser;
    _presentingViewController = viewController;
    // TODO(crbug.com/375605482): Need to sign-out if the new profile is not
    // signed in with the right identity (useful for the personal profile).
    // TODO(crbug.com/375605482): Need to block user until AuthenticationFlow
    // is done? Probably with a blur animation.
    [self signInInCurrentProfile];
  } else {
    // TODO(crbug.com/375605482): Generate an error and call:
    // `[self handleAuthenticationError:error];`.
  }
}

// Signs in the user using `_identityToSignIn`. The identity must be assigned
// to the current profile.
- (void)signInInCurrentProfile {
  ProfileIOS* profile = [self originalProfile];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(IsIdentityInCoreAccountInfos(
      _identityToSignIn, identityManager->GetAccountsWithRefreshTokens()));
  [_performer signInIdentity:_identityToSignIn
               atAccessPoint:self.accessPoint
              currentProfile:profile];
  _didSignIn = YES;
  [self continueFlow];
}

#pragma mark - Used for testing

- (void)setPerformerForTesting:(AuthenticationFlowPerformer*)performer {
  _performer = performer;
}

@end
