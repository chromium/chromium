// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"

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
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
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
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
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

// Returns yes if the browser has machine level policies.
bool HasMachineLevelPolicies() {
  BrowserPolicyConnectorIOS* policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  return policy_connector && policy_connector->HasMachineLevelPolicies();
}

}  // namespace

@interface AuthenticationFlow ()

// Whether this flow is curently handling an error.
@property(nonatomic, assign) BOOL handlingError;

// The actions to perform following account sign-in.
@property(nonatomic, assign) PostSignInActionSet postSignInActions;

// Checks which sign-in steps to perform and updates member variables
// accordingly.
- (void)checkSigninSteps;

// Continues the sign-in state machine starting from `_state` and invokes
// `_signInCompletion` when finished.
- (void)continueSignin;

// Runs `_signInCompletion` asynchronously with `result` argument.
- (void)completeSignInWithResult:(SigninCoordinatorResult)result;

// Cancels the current sign-in flow.
- (void)cancelFlowWithReason:(CancelationReason)byUser;

// Handles an authentication error and show an alert to the user.
- (void)handleAuthenticationError:(NSError*)error;

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
  BOOL _alreadySignedInWithTheSameAccount;
  // YES if the signed in account is a managed account and the sign-in flow
  // includes sync.
  BOOL _shouldShowManagedConfirmation;
  // YES if user policies have to be fetched.
  BOOL _shouldFetchUserPolicy;
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
    _performer = [[AuthenticationFlowPerformer alloc] initWithDelegate:self];
  }
  // Make sure -[AuthenticationFlow startSignInWithCompletion:] doesn't call
  // the completion block synchronously.
  // Related to http://crbug.com/1246480.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf continueSignin];
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
      if (_shouldShowManagedConfirmation)
        return SHOW_MANAGED_CONFIRMATION;
      else if (_shouldSignOut)
        return SIGN_OUT_IF_NEEDED;
      else
        return SIGN_IN;
    case SHOW_MANAGED_CONFIRMATION:
      if (_shouldSignOut)
        return SIGN_OUT_IF_NEEDED;
      else
        return SIGN_IN;
    case SIGN_OUT_IF_NEEDED:
      return SIGN_IN;
    case SIGN_IN:
      if (self.postSignInActions.Has(PostSignInAction::kShowSnackbar)) {
        _shouldShowSigninSnackbar = YES;
      }
      if (_shouldFetchUserPolicy) {
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

- (void)continueSignin {
  ProfileIOS* profile = [self originalProfile];
  if (self.handlingError) {
    // The flow should not continue while the error is being handled, e.g. while
    // the user is being informed of an issue.
    return;
  }
  _state = [self nextState];
  switch (_state) {
    case BEGIN:
      NOTREACHED_IN_MIGRATION();
      return;

    case CHECK_SIGNIN_STEPS:
      [self checkSigninSteps];
      [self continueSignin];
      return;

    case FETCH_MANAGED_STATUS:
      [_performer fetchManagedStatus:profile forIdentity:_identityToSignIn];
      return;

    case SHOW_MANAGED_CONFIRMATION: {
      [_performer
          showManagedConfirmationForHostedDomain:_identityToSignInHostedDomain
                                  viewController:_presentingViewController
                                         browser:_browser];
      return;
    }

    case SIGN_OUT_IF_NEEDED:
      [_performer signOutProfile:profile];
      return;

    case SIGN_IN:
      [self signInIdentity:_identityToSignIn];
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
      [self continueSignin];
      return;
    }
    case DONE:
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

- (void)checkSigninSteps {
  id<SystemIdentity> currentIdentity =
      AuthenticationServiceFactory::GetForProfile([self originalProfile])
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (currentIdentity && ![currentIdentity isEqual:_identityToSignIn]) {
    // If the identity to sign-in is different than the current identity,
    // sign-out is required.
    _shouldSignOut = YES;
  }
  _alreadySignedInWithTheSameAccount =
      [currentIdentity isEqual:_identityToSignIn];
}

- (void)signInIdentity:(id<SystemIdentity>)identity {
  if (self.userDecisionCompletion) {
    self.userDecisionCompletion();
  }
  ProfileIOS* profile = [self originalProfile];
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);

  if (accountManagerService->IsValidIdentity(identity)) {
    [_performer signInIdentity:identity
                 atAccessPoint:self.accessPoint
              withHostedDomain:_identityToSignInHostedDomain
                     toProfile:profile];
    _didSignIn = YES;
    [self continueSignin];
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
        [weakSelf continueSignin];
      })];
}

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
  [self continueSignin];
}

- (BOOL)canceled {
  return _cancelationReason != CancelationReason::kNotCanceled;
}

- (void)cancelFlowWithReason:(CancelationReason)reason {
  CHECK_NE(reason, CancelationReason::kNotCanceled);
  if ([self canceled]) {
    // Avoid double handling of cancel or error.
    return;
  }
  _cancelationReason = reason;
  [self continueSignin];
}

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
                         if (!strongSelf)
                           return;
                         [strongSelf setHandlingError:NO];
                         [strongSelf continueSignin];
                       }
                       viewController:_presentingViewController
                              browser:_browser];
}

#pragma mark AuthenticationFlowPerformerDelegate

- (void)didSignOut {
  [self continueSignin];
}

- (void)didClearData {
  [self continueSignin];
}

- (void)didFetchManagedStatus:(NSString*)hostedDomain {
  DCHECK_EQ(FETCH_MANAGED_STATUS, _state);
  _shouldShowManagedConfirmation =
      [self shouldShowManagedConfirmationForHostedDomain:hostedDomain];
  _identityToSignInHostedDomain = hostedDomain;
  _shouldFetchUserPolicy =
      [self shouldFetchUserPolicy] && hostedDomain.length > 0;
  [self continueSignin];
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

- (void)didAcceptManagedConfirmation {
  if (base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)) {
    // Only show the dialog once per account.
    signin::GaiaIdHash gaiaIDHash = signin::GaiaIdHash::FromGaiaId(
        base::SysNSStringToUTF8(_identityToSignIn.gaiaID));
    syncer::SetAccountKeyedPrefValue([self prefs],
                                     prefs::kSigninHasAcceptedManagementDialog,
                                     gaiaIDHash, base::Value(true));
  }
  [self continueSignin];
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
  [self continueSignin];
}

- (void)didFetchUserPolicyWithSuccess:(BOOL)success {
  DCHECK_EQ(FETCH_USER_POLICY, _state);
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  [self continueSignin];
}

#pragma mark - Private methods

// The original profile used for services that don't exist in incognito mode.
- (ProfileIOS*)originalProfile {
  return _browser->GetProfile()->GetOriginalProfile();
}

- (PrefService*)prefs {
  return [self originalProfile]->GetPrefs();
}

// Returns YES if the managed confirmation dialog should be shown for the
// hosted domain.
- (BOOL)shouldShowManagedConfirmationForHostedDomain:(NSString*)hostedDomain {
  if ([hostedDomain length] == 0) {
    // No hosted domain, don't show the dialog as there is no host.
    return NO;
  }

  if (HasMachineLevelPolicies()) {
    // Don't show the dialog if the browser has already machine level policies
    // as the user already knows that their browser is managed.
    return NO;
  }

  if (_accessPoint == signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU &&
      base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)) {
    // Only show the dialog once per account, when switching from the Account
    // Menu.
    signin::GaiaIdHash gaiaIDHash = signin::GaiaIdHash::FromGaiaId(
        base::SysNSStringToUTF8(_identityToSignIn.gaiaID));
    const base::Value* alreadySeen = syncer::GetAccountKeyedPrefValue(
        [self prefs], prefs::kSigninHasAcceptedManagementDialog, gaiaIDHash);
    if (alreadySeen && alreadySeen->GetIfBool().value_or(false)) {
      return NO;
    }
  }

  // Show the dialog if User Policy is enabled.
  return policy::IsAnyUserPolicyFeatureEnabled();
}

// Returns YES if should fetch user policy.
- (BOOL)shouldFetchUserPolicy {
  return policy::IsAnyUserPolicyFeatureEnabled();
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

#pragma mark - Used for testing

- (void)setPerformerForTesting:(AuthenticationFlowPerformer*)performer {
  _performer = performer;
}

@end
