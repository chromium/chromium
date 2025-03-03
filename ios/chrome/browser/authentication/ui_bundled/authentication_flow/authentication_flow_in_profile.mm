// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile.h"

#import "base/check_op.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_capabilities_fetcher.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace {

enum class AuthenticationFlowInProfileState {
  kBegin,
  kSignOutIfNeeded,
  kSignInIfNeeded,
  kRegisterForUserPolicyIfNeeded,
  kFetchUserPolicyIfNeeded,
  kFetchCapabilitiesIfNeeded,
  kCompletionWithSuccess,
  kCompletionWithFailure,
  kCleanupBeforeDone,
  kDone,
};

}  // namespace

@interface AuthenticationFlowInProfile () <AuthenticationFlowPerformerDelegate>
@end

@implementation AuthenticationFlowInProfile {
  // State machine tracking for sign-in flow.
  AuthenticationFlowInProfileState _state;
  // This AuthenticationFlowInProfile keeps a reference to `self` while a
  // sign-in flow is is in progress to ensure it outlives until the last step.
  AuthenticationFlowInProfile* _selfRetainer;
  signin_ui::SigninCompletionCallback _signInCompletion;
  BOOL _error;
  id<SystemIdentity> _identityToSignIn;
  // `YES` if `_identityToSignIn` is a managed identity.
  BOOL _isManagedIdentity;
  AuthenticationFlowPerformer* _performer;
  raw_ptr<Browser> _browser;
  signin_metrics::AccessPoint _accessPoint;
  // Token to have access to user policies from dmserver.
  NSString* _dmToken;
  // ID of the client that is registered for user policy.
  NSString* _clientID;
  // List of IDs that represents the domain of the user. The list will be used
  // to compare with a similiar list from device mangement to understand whether
  // user and device are managed by the same domain.
  NSArray<NSString*>* _userAffiliationIDs;
  // Capabilities fetcher for the subsequent History Sync Opt-In screen.
  HistorySyncCapabilitiesFetcher* _capabilitiesFetcher;
  PostSignInActionSet _postSignInActions;
}

- (instancetype)initWithBrowser:(Browser*)browser
                       identity:(id<SystemIdentity>)identity
              isManagedIdentity:(BOOL)isManagedIdentity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
              postSignInActions:(PostSignInActionSet)postSignInActions {
  self = [super init];
  if (self) {
    CHECK(browser);
    CHECK(identity);
    _browser = browser;
    _identityToSignIn = identity;
    _isManagedIdentity = isManagedIdentity;
    _accessPoint = accessPoint;
    _postSignInActions = postSignInActions;
    _state = AuthenticationFlowInProfileState::kBegin;
  }
  return self;
}

- (void)startSignInWithCompletion:
    (signin_ui::SigninCompletionCallback)completion {
  CHECK_EQ(_state, AuthenticationFlowInProfileState::kBegin,
           base::NotFatalUntil::M138);
  CHECK(completion);
  _selfRetainer = self;
  _signInCompletion = completion;
  _performer = [[AuthenticationFlowPerformer alloc] initWithDelegate:self
                                                changeProfileHandler:nil];
  // Make sure -[AuthenticationFlow startSignInWithCompletion:] doesn't call
  // the completion block synchronously.
  // Related to http://crbug.com/1246480.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf continueFlow];
  });
}

#pragma mark - Private methods

// The original profile used for services that don't exist in incognito mode.
- (ProfileIOS*)originalProfile {
  return _browser->GetProfile()->GetOriginalProfile();
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

#pragma mark - State machine management

- (AuthenticationFlowInProfileState)nextStateFailed {
  switch (_state) {
    case AuthenticationFlowInProfileState::kBegin:
      NOTREACHED();
    case AuthenticationFlowInProfileState::kSignOutIfNeeded:
    case AuthenticationFlowInProfileState::kSignInIfNeeded:
    case AuthenticationFlowInProfileState::kRegisterForUserPolicyIfNeeded:
    case AuthenticationFlowInProfileState::kFetchUserPolicyIfNeeded:
    case AuthenticationFlowInProfileState::kFetchCapabilitiesIfNeeded:
      return AuthenticationFlowInProfileState::kCompletionWithFailure;
    case AuthenticationFlowInProfileState::kCompletionWithFailure:
    case AuthenticationFlowInProfileState::kCompletionWithSuccess:
      return AuthenticationFlowInProfileState::kCleanupBeforeDone;
    case AuthenticationFlowInProfileState::kCleanupBeforeDone:
    case AuthenticationFlowInProfileState::kDone:
      return AuthenticationFlowInProfileState::kDone;
  }
}

- (AuthenticationFlowInProfileState)nextState {
  if (_error) {
    return [self nextStateFailed];
  }
  switch (_state) {
    case AuthenticationFlowInProfileState::kBegin:
      return AuthenticationFlowInProfileState::kSignOutIfNeeded;
    case AuthenticationFlowInProfileState::kSignOutIfNeeded:
      return AuthenticationFlowInProfileState::kSignInIfNeeded;
    case AuthenticationFlowInProfileState::kSignInIfNeeded:
      return AuthenticationFlowInProfileState::kRegisterForUserPolicyIfNeeded;
    case AuthenticationFlowInProfileState::kRegisterForUserPolicyIfNeeded:
      return AuthenticationFlowInProfileState::kFetchUserPolicyIfNeeded;
    case AuthenticationFlowInProfileState::kFetchUserPolicyIfNeeded:
      return AuthenticationFlowInProfileState::kFetchCapabilitiesIfNeeded;
    case AuthenticationFlowInProfileState::kFetchCapabilitiesIfNeeded:
      return AuthenticationFlowInProfileState::kCompletionWithSuccess;
    case AuthenticationFlowInProfileState::kCompletionWithSuccess:
    case AuthenticationFlowInProfileState::kCompletionWithFailure:
      return AuthenticationFlowInProfileState::kCleanupBeforeDone;
    case AuthenticationFlowInProfileState::kCleanupBeforeDone:
    case AuthenticationFlowInProfileState::kDone:
      return AuthenticationFlowInProfileState::kDone;
  }
}

- (void)continueFlow {
  _state = [self nextState];
  switch (_state) {
    case AuthenticationFlowInProfileState::kBegin:
      NOTREACHED();
    case AuthenticationFlowInProfileState::kSignOutIfNeeded:
      [self signOutIfNeededStep];
      return;
    case AuthenticationFlowInProfileState::kSignInIfNeeded:
      [self signInIfNeededStep];
      return;
    case AuthenticationFlowInProfileState::kRegisterForUserPolicyIfNeeded:
      [self registerForUserPolicyIfNeededStep];
      return;
    case AuthenticationFlowInProfileState::kFetchUserPolicyIfNeeded:
      [self fetchUserPolicyIfNeededStep];
      return;
    case AuthenticationFlowInProfileState::kFetchCapabilitiesIfNeeded:
      [self fetchCapabilitiesIfNeededStep];
      return;
    case AuthenticationFlowInProfileState::kCompletionWithSuccess:
      [self successCompleteFlowStep];
      return;
    case AuthenticationFlowInProfileState::kCompletionWithFailure:
      [self failureCompleteFlowStep];
      return;
    case AuthenticationFlowInProfileState::kCleanupBeforeDone:
      [self cleanupBeforeDoneStep];
      return;
    case AuthenticationFlowInProfileState::kDone:
      return;
  }
  NOTREACHED();
}

#pragma mark - Steps

// Signs out, if the user is already signed in with a different identity.
// Otherwise, this step does nothing and the flow continues to the next step.
- (void)signOutIfNeededStep {
  ProfileIOS* profile = [self originalProfile];
  id<SystemIdentity> currentIdentity =
      AuthenticationServiceFactory::GetForProfile(profile)->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin);
  if (currentIdentity && ![currentIdentity isEqual:_identityToSignIn]) {
    [_performer signOutForAccountSwitchWithProfile:profile];
    return;
  }
  [self continueFlow];
}

// Sets the primary identity if not already set.
- (void)signInIfNeededStep {
  ProfileIOS* profile = [self originalProfile];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  std::vector<CoreAccountInfo> accountsInProfile =
      identityManager->GetAccountsWithRefreshTokens();
  BOOL isValidIdentityInProfile =
      base::Contains(accountsInProfile, GaiaId(_identityToSignIn.gaiaID),
                     &CoreAccountInfo::gaia);
  if (!isValidIdentityInProfile) {
    _error = YES;
    [self continueFlow];
    return;
  }
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> currentIdentity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!currentIdentity) {
    [_performer signInIdentity:_identityToSignIn
                 atAccessPoint:_accessPoint
                currentProfile:profile];
  } else {
    CHECK([currentIdentity isEqual:_identityToSignIn],
          base::NotFatalUntil::M138);
  }
  [self continueFlow];
}

// Registers to DM Server to get a DM token and client ID, to fetch user
// policies in the next step.
- (void)registerForUserPolicyIfNeededStep {
  if (!policy::IsAnyUserPolicyFeatureEnabled() || !_isManagedIdentity) {
    [self continueFlow];
    return;
  }
  ProfileIOS* profile = [self originalProfile];
  [_performer registerUserPolicy:profile forIdentity:_identityToSignIn];
}

// Fetches user policy.
- (void)fetchUserPolicyIfNeededStep {
  if (!_dmToken.length || !_clientID.length) {
    // Skip fetching user policies when registration failed or was not required.
    [self continueFlow];
    return;
  }
  CHECK(policy::IsAnyUserPolicyFeatureEnabled(), base::NotFatalUntil::M140);
  CHECK(_isManagedIdentity, base::NotFatalUntil::M140);
  ProfileIOS* profile = [self originalProfile];
  [_performer fetchUserPolicy:profile
                  withDmToken:_dmToken
                     clientID:_clientID
           userAffiliationIDs:_userAffiliationIDs
                     identity:_identityToSignIn];
}

// Fetches capabilities on successful authentication for the upcoming History
// Sync Opt-In screen.
- (void)fetchCapabilitiesIfNeededStep {
  if (![self shouldFetchCapabilities]) {
    [self continueFlow];
    return;
  }
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

- (void)successCompleteFlowStep {
  // Need to identify if the current profile is a managed profile or the
  // personal profile.
  signin_metrics::SigninAccountType accountType =
      _isManagedIdentity ? signin_metrics::SigninAccountType::kManaged
                         : signin_metrics::SigninAccountType::kRegular;
  signin_metrics::LogSigninWithAccountType(accountType);
  signin_ui::SigninCompletionCallback signInCompletion = _signInCompletion;
  _signInCompletion = nil;
  signInCompletion(SigninCoordinatorResult::SigninCoordinatorResultSuccess);
  [_performer completePostSignInActions:_postSignInActions
                           withIdentity:_identityToSignIn
                                browser:_browser];
  [self continueFlow];
}

- (void)failureCompleteFlowStep {
  // TODO(crbug.com/375605482): Need to switch back to the personal profile
  // (if the current profile is a managed profile), sign out (if the personal
  // profile is signed in), and display an error.
  signin_ui::SigninCompletionCallback signInCompletion = _signInCompletion;
  _signInCompletion = nil;
  // If the sign-in failed, the result is `SigninCoordinatorResultInterrupted`.
  signInCompletion(SigninCoordinatorResult::SigninCoordinatorResultInterrupted);
  [self continueFlow];
}

- (void)cleanupBeforeDoneStep {
  // Clean up asynchronously to ensure that `self` does not die while
  // the flow is running.
  CHECK([NSThread isMainThread], base::NotFatalUntil::M138);
  dispatch_async(dispatch_get_main_queue(), ^{
    self->_selfRetainer = nil;
  });
  [self continueFlow];
}

#pragma mark - AuthenticationFlowPerformerDelegate

- (void)didSignOutForAccountSwitch {
  CHECK_EQ(AuthenticationFlowInProfileState::kSignOutIfNeeded, _state,
           base::NotFatalUntil::M138);
  [self continueFlow];
}

- (void)didClearData {
  // TODO(crbug.com/375605482): It might be relevant to split
  // `AuthenticationFlowPerformer` into 2 classes. This would avoid having
  // all those NOTREACHED methods.
  NOTREACHED();
}

- (void)didFetchUnsyncedDataWithUnsyncedDataTypes:
    (syncer::DataTypeSet)unsyncedDataTypes {
  // Unsynced data is checked by AuthenticationFlow before calling
  // `AuthenticationFlowInProfile`.
  // So unsynced data is checked when leaving a profile (for profile switching),
  // or before sign-out (for account switching).
  NOTREACHED();
}

- (void)didAcceptToLeavePrimaryAccount:(BOOL)acceptToContinue {
  // Unsynced data confirmation dialog should not be shown. See the explaination
  // in `-[AuthenticationFlowInProfile
  // didFetchUnsyncedDataWithUnsyncedDataTypes:]`.
  NOTREACHED();
}

- (void)didFetchManagedStatus:(NSString*)hostedDomain {
  NOTREACHED();
}

- (void)didFailFetchManagedStatus:(NSError*)error {
  NOTREACHED();
}

- (void)didAcceptManagedConfirmation:(BOOL)keepBrowsingDataSeparate {
  NOTREACHED();
}

- (void)didCancelManagedConfirmation {
  NOTREACHED();
}

- (void)didRegisterForUserPolicyWithDMToken:(NSString*)dmToken
                                   clientID:(NSString*)clientID
                         userAffiliationIDs:
                             (NSArray<NSString*>*)userAffiliationIDs {
  CHECK_EQ(AuthenticationFlowInProfileState::kRegisterForUserPolicyIfNeeded,
           _state, base::NotFatalUntil::M138);
  _dmToken = dmToken;
  _clientID = clientID;
  _userAffiliationIDs = userAffiliationIDs;
  [self continueFlow];
}

- (void)didFetchUserPolicyWithSuccess:(BOOL)success {
  // The result can be ignored, the goal was to prefetch the user policy.
  CHECK_EQ(AuthenticationFlowInProfileState::kFetchUserPolicyIfNeeded, _state,
           base::NotFatalUntil::M138);
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  [self continueFlow];
}

- (void)didMakePersonalProfileManaged {
  NOTREACHED();
}

- (void)didFetchProfileSeparationPolicies:
    (policy::ProfileSeparationDataMigrationSettings)
        profileSeparationDataMigrationSettings {
  NOTREACHED();
}

@end
