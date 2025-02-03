// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_current_profile.h"

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

enum class AuthenticationFlowInCurrentProfileState {
  kBegin,
  kSignOutIfNeeded,
  kSignInIfNeeded,
  kRegisterForUserPolicy,
  kFetchUserPolicy,
  kFetchCapabilities,
  kCompletionWithSuccess,
  kCompletionWithFailure,
  kCleanupBeforeDone,
  kDone,
};

}  // namespace

@interface AuthenticationFlowInCurrentProfile () <
    AuthenticationFlowPerformerDelegate>
@end

@implementation AuthenticationFlowInCurrentProfile {
  // State machine tracking for sign-in flow.
  AuthenticationFlowInCurrentProfileState _state;
  // This AuthenticationFlowInCurrentProfile keeps a reference to `self` while
  // a sign-in flow is is in progress to ensure it outlives until the last step.
  AuthenticationFlowInCurrentProfile* _selfRetainer;
  signin_ui::SigninCompletionCallback _signInCompletion;
  BOOL _error;
  BOOL _shouldFetchUserPolicy;
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
    _state = AuthenticationFlowInCurrentProfileState::kBegin;
  }
  return self;
}

- (void)startSignInWithCompletion:
    (signin_ui::SigninCompletionCallback)completion {
  CHECK_EQ(_state, AuthenticationFlowInCurrentProfileState::kBegin);
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

- (AuthenticationFlowInCurrentProfileState)nextStateFailed {
  switch (_state) {
    case AuthenticationFlowInCurrentProfileState::kBegin:
      NOTREACHED();
    case AuthenticationFlowInCurrentProfileState::kSignOutIfNeeded:
    case AuthenticationFlowInCurrentProfileState::kSignInIfNeeded:
    case AuthenticationFlowInCurrentProfileState::kRegisterForUserPolicy:
    case AuthenticationFlowInCurrentProfileState::kFetchUserPolicy:
    case AuthenticationFlowInCurrentProfileState::kFetchCapabilities:
      return AuthenticationFlowInCurrentProfileState::kCompletionWithFailure;
    case AuthenticationFlowInCurrentProfileState::kCompletionWithFailure:
    case AuthenticationFlowInCurrentProfileState::kCompletionWithSuccess:
      return AuthenticationFlowInCurrentProfileState::kCleanupBeforeDone;
    case AuthenticationFlowInCurrentProfileState::kCleanupBeforeDone:
    case AuthenticationFlowInCurrentProfileState::kDone:
      return AuthenticationFlowInCurrentProfileState::kDone;
  }
}

- (AuthenticationFlowInCurrentProfileState)nextState {
  if (_error) {
    return [self nextStateFailed];
  }
  switch (_state) {
    case AuthenticationFlowInCurrentProfileState::kBegin:
      return AuthenticationFlowInCurrentProfileState::kSignOutIfNeeded;
    case AuthenticationFlowInCurrentProfileState::kSignOutIfNeeded:
      return AuthenticationFlowInCurrentProfileState::kSignInIfNeeded;
    case AuthenticationFlowInCurrentProfileState::kSignInIfNeeded:
      if (policy::IsAnyUserPolicyFeatureEnabled() && _isManagedIdentity) {
        return AuthenticationFlowInCurrentProfileState::kRegisterForUserPolicy;
      } else if ([self shouldFetchCapabilities]) {
        return AuthenticationFlowInCurrentProfileState::kFetchCapabilities;
      }
      return AuthenticationFlowInCurrentProfileState::kCompletionWithSuccess;
    case AuthenticationFlowInCurrentProfileState::kRegisterForUserPolicy:
      if (!_dmToken.length || !_clientID.length) {
        // Skip fetching user policies when registration failed.
        if ([self shouldFetchCapabilities]) {
          return AuthenticationFlowInCurrentProfileState::kFetchCapabilities;
        }
        return AuthenticationFlowInCurrentProfileState::kCompletionWithSuccess;
      }
      // Fetch user policies when registration is successful.
      return AuthenticationFlowInCurrentProfileState::kFetchUserPolicy;
    case AuthenticationFlowInCurrentProfileState::kFetchUserPolicy:
      if ([self shouldFetchCapabilities]) {
        return AuthenticationFlowInCurrentProfileState::kFetchCapabilities;
      }
      return AuthenticationFlowInCurrentProfileState::kCompletionWithSuccess;
    case AuthenticationFlowInCurrentProfileState::kFetchCapabilities:
      return AuthenticationFlowInCurrentProfileState::kCompletionWithSuccess;
    case AuthenticationFlowInCurrentProfileState::kCompletionWithSuccess:
    case AuthenticationFlowInCurrentProfileState::kCompletionWithFailure:
      return AuthenticationFlowInCurrentProfileState::kCleanupBeforeDone;
    case AuthenticationFlowInCurrentProfileState::kCleanupBeforeDone:
    case AuthenticationFlowInCurrentProfileState::kDone:
      return AuthenticationFlowInCurrentProfileState::kDone;
  }
}

- (void)continueFlow {
  ProfileIOS* profile = [self originalProfile];
  _state = [self nextState];
  switch (_state) {
    case AuthenticationFlowInCurrentProfileState::kBegin:
      NOTREACHED();
    case AuthenticationFlowInCurrentProfileState::kSignOutIfNeeded:
      [self signOutIfNeededStep];
      return;
    case AuthenticationFlowInCurrentProfileState::kSignInIfNeeded:
      [self signInIfNeededStep];
      return;
    case AuthenticationFlowInCurrentProfileState::kRegisterForUserPolicy:
      [_performer registerUserPolicy:profile forIdentity:_identityToSignIn];
      return;
    case AuthenticationFlowInCurrentProfileState::kFetchUserPolicy:
      [_performer fetchUserPolicy:profile
                      withDmToken:_dmToken
                         clientID:_clientID
               userAffiliationIDs:_userAffiliationIDs
                         identity:_identityToSignIn];
      return;
    case AuthenticationFlowInCurrentProfileState::kFetchCapabilities:
      [self fetchCapabilitiesStep];
      return;
    case AuthenticationFlowInCurrentProfileState::kCompletionWithSuccess:
      [self successCompleteFlowStep];
      return;
    case AuthenticationFlowInCurrentProfileState::kCompletionWithFailure:
      [self failureCompleteFlowStep];
      return;
    case AuthenticationFlowInCurrentProfileState::kCleanupBeforeDone:
      [self cleanupBeforeDoneStep];
      return;
    case AuthenticationFlowInCurrentProfileState::kDone:
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
    [_performer signOutProfile:profile];
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
    CHECK([currentIdentity isEqual:_identityToSignIn]);
  }
  [self continueFlow];
}

// Fetches capabilities on successful authentication for the upcoming History
// Sync Opt-In screen.
- (void)fetchCapabilitiesStep {
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
  CHECK([NSThread isMainThread]);
  dispatch_async(dispatch_get_main_queue(), ^{
    self->_selfRetainer = nil;
  });
  [self continueFlow];
}

#pragma mark - AuthenticationFlowPerformerDelegate

- (void)didSignOut {
  // TODO(crbug.com/375605482): It might be relevant to split
  // `AuthenticationFlowPerformer` into 2 classes. This would avoid having
  // all those NOTREACHED methods.
  NOTREACHED();
}

- (void)didClearData {
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
  CHECK_EQ(AuthenticationFlowInCurrentProfileState::kRegisterForUserPolicy,
           _state);
  _dmToken = dmToken;
  _clientID = clientID;
  _userAffiliationIDs = userAffiliationIDs;
  [self continueFlow];
}

- (void)didFetchUserPolicyWithSuccess:(BOOL)success {
  DCHECK_EQ(AuthenticationFlowInCurrentProfileState::kFetchUserPolicy, _state);
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
