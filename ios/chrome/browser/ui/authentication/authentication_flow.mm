// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"

#import "base/check_op.h"
#import "base/feature_list.cc"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"
#import "ui/base/l10n/l10n_util.h"

using signin_ui::CompletionCallback;

namespace {

// The states of the sign-in flow state machine.
enum AuthenticationState {
  BEGIN,
  CHECK_SIGNIN_STEPS,
  FETCH_MANAGED_STATUS,
  CHECK_MERGE_CASE,
  SHOW_MANAGED_CONFIRMATION,
  SIGN_OUT_IF_NEEDED,
  CLEAR_DATA,
  SIGN_IN,
  COMMIT_SYNC,
  REGISTER_FOR_USER_POLICY,
  FETCH_USER_POLICY,
  COMPLETE_WITH_SUCCESS,
  COMPLETE_WITH_FAILURE,
  CLEANUP_BEFORE_DONE,
  DONE
};

}  // namespace

@interface AuthenticationFlow ()

// Whether this flow is curently handling an error.
@property(nonatomic, assign) BOOL handlingError;

// The action to perform following account sign-in.
@property(nonatomic, assign) PostSignInAction postSignInAction;

// Indicates how to handle existing data when the signed in account is being
// switched. Possible values:
//   * User choice: present an alert view asking the user whether the data
//     should be cleared or merged.
//   * Clear data: data is removed before signing in with `identity`.
//   * Merge data: data is not removed before signing in with `identity`.
@property(nonatomic, assign) ShouldClearData localDataClearingStrategy;

// Checks which sign-in steps to perform and updates member variables
// accordingly.
- (void)checkSigninSteps;

// Continues the sign-in state machine starting from `_state` and invokes
// `_signInCompletion` when finished.
- (void)continueSignin;

// Runs `_signInCompletion` asynchronously with `success` argument.
- (void)completeSignInWithSuccess:(BOOL)success;

// Cancels the current sign-in flow.
- (void)cancelFlow;

// Handles an authentication error and show an alert to the user.
- (void)handleAuthenticationError:(NSError*)error;

@end

@implementation AuthenticationFlow {
  UIViewController* _presentingViewController;
  CompletionCallback _signInCompletion;
  AuthenticationFlowPerformer* _performer;

  // State machine tracking.
  AuthenticationState _state;
  BOOL _didSignIn;
  BOOL _failedOrCancelled;
  BOOL _shouldSignOut;
  BOOL _alreadySignedInWithTheSameAccount;
  // YES if the signed in account is a managed account and the sign-in flow
  // includes sync.
  BOOL _shouldShowManagedConfirmation;
  // YES if user policies have to be fetched.
  BOOL _shouldFetchUserPolicy;
  // YES if user is opted into bookmark and reading list account storage.
  BOOL _shouldShowSigninSnackbar;

  Browser* _browser;
  id<SystemIdentity> _identityToSignIn;
  signin_metrics::AccessPoint _accessPoint;
  NSString* _identityToSignInHostedDomain;

  // Token to have access to user policies from dmserver.
  NSString* _dmToken;
  // ID of the client that is registered for user policy.
  NSString* _clientID;

  // This AuthenticationFlow keeps a reference to `self` while a sign-in flow is
  // is in progress to ensure it outlives any attempt to destroy it in
  // `_signInCompletion`.
  AuthenticationFlow* _selfRetainer;
}

@synthesize handlingError = _handlingError;
@synthesize dispatcher = _dispatcher;
@synthesize identity = _identityToSignIn;

#pragma mark - Public methods

- (instancetype)initWithBrowser:(Browser*)browser
                       identity:(id<SystemIdentity>)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
               postSignInAction:(PostSignInAction)postSignInAction
       presentingViewController:(UIViewController*)presentingViewController {
  if ((self = [super init])) {
    DCHECK(browser);
    DCHECK(presentingViewController);
    DCHECK(identity);
    _browser = browser;
    _identityToSignIn = identity;
    _accessPoint = accessPoint;
    _localDataClearingStrategy = SHOULD_CLEAR_DATA_USER_CHOICE;
    _postSignInAction = postSignInAction;
    _presentingViewController = presentingViewController;
    _state = BEGIN;
  }
  return self;
}

- (void)startSignInWithCompletion:(CompletionCallback)completion {
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
    [self cancelFlow];
  }
  DCHECK_EQ(DONE, _state);
}

- (void)setPresentingViewController:
    (UIViewController*)presentingViewController {
  _presentingViewController = presentingViewController;
}

#pragma mark - State machine management

- (AuthenticationState)nextStateFailedOrCancelled {
  DCHECK(_failedOrCancelled);
  switch (_state) {
    case BEGIN:
    case CHECK_SIGNIN_STEPS:
    case FETCH_MANAGED_STATUS:
    case CHECK_MERGE_CASE:
    case SHOW_MANAGED_CONFIRMATION:
    case SIGN_OUT_IF_NEEDED:
    case CLEAR_DATA:
    case SIGN_IN:
    case COMMIT_SYNC:
    case REGISTER_FOR_USER_POLICY:
    case FETCH_USER_POLICY:
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
  if (_failedOrCancelled) {
    return [self nextStateFailedOrCancelled];
  }
  DCHECK(!_failedOrCancelled);
  switch (_state) {
    case BEGIN:
      return CHECK_SIGNIN_STEPS;
    case CHECK_SIGNIN_STEPS:
      return FETCH_MANAGED_STATUS;
    case FETCH_MANAGED_STATUS:
      return CHECK_MERGE_CASE;
    case CHECK_MERGE_CASE:
      // If the user enabled Sync, expect the data clearing strategy to be set.
      switch (self.postSignInAction) {
        case PostSignInAction::kNone:
        case PostSignInAction::kShowSnackbar:
          // `localDataClearingStrategy` is not required.
          break;
        case PostSignInAction::kCommitSync:
          DCHECK_NE(SHOULD_CLEAR_DATA_USER_CHOICE,
                    self.localDataClearingStrategy);
          break;
      }
      if (_shouldShowManagedConfirmation)
        return SHOW_MANAGED_CONFIRMATION;
      else if (_shouldSignOut)
        return SIGN_OUT_IF_NEEDED;
      else if (self.localDataClearingStrategy == SHOULD_CLEAR_DATA_CLEAR_DATA)
        return CLEAR_DATA;
      else
        return SIGN_IN;
    case SHOW_MANAGED_CONFIRMATION:
      if (_shouldSignOut)
        return SIGN_OUT_IF_NEEDED;
      else if (self.localDataClearingStrategy == SHOULD_CLEAR_DATA_CLEAR_DATA)
        return CLEAR_DATA;
      else
        return SIGN_IN;
    case SIGN_OUT_IF_NEEDED:
      return self.localDataClearingStrategy == SHOULD_CLEAR_DATA_CLEAR_DATA
                 ? CLEAR_DATA
                 : SIGN_IN;
    case CLEAR_DATA:
      return SIGN_IN;
    case SIGN_IN:
      switch (self.postSignInAction) {
        case PostSignInAction::kCommitSync:
          return COMMIT_SYNC;
        case PostSignInAction::kShowSnackbar:
          _shouldShowSigninSnackbar = YES;
          [[fallthrough]];
        case PostSignInAction::kNone:
          if (_shouldFetchUserPolicy) {
            return REGISTER_FOR_USER_POLICY;
          } else {
            return COMPLETE_WITH_SUCCESS;
          }
      }
    case COMMIT_SYNC:
      if (_shouldFetchUserPolicy) {
        return REGISTER_FOR_USER_POLICY;
      }
      return COMPLETE_WITH_SUCCESS;
    case REGISTER_FOR_USER_POLICY:
      if (!_dmToken.length || !_clientID.length) {
        // Skip fetching user policies when registration failed.
        return COMPLETE_WITH_SUCCESS;
      }
      // Fetch user policies when registration is successful.
      return FETCH_USER_POLICY;
    case FETCH_USER_POLICY:
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
  ChromeBrowserState* browserState = [self originalBrowserState];
  if (self.handlingError) {
    // The flow should not continue while the error is being handled, e.g. while
    // the user is being informed of an issue.
    return;
  }
  _state = [self nextState];
  switch (_state) {
    case BEGIN:
      NOTREACHED();
      return;

    case CHECK_SIGNIN_STEPS:
      [self checkSigninSteps];
      [self continueSignin];
      return;

    case FETCH_MANAGED_STATUS:
      [_performer fetchManagedStatus:browserState
                         forIdentity:_identityToSignIn];
      return;

    case CHECK_MERGE_CASE: {
      DCHECK_EQ(SHOULD_CLEAR_DATA_USER_CHOICE, self.localDataClearingStrategy);
      // Do not perform a custom data clearing strategy for supervised users
      // with the experiment `syncer::kReplaceSyncPromosWithSignInPromos`.
      if (base::FeatureList::IsEnabled(
              syncer::kReplaceSyncPromosWithSignInPromos)) {
        [self checkPostSigninAction];
        return;
      }
      __weak AuthenticationFlow* weakSelf = self;
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->IsSubjectToParentalControls(
              _identityToSignIn,
              base::BindOnce(^(SystemIdentityCapabilityResult result) {
                if (result == SystemIdentityCapabilityResult::kTrue) {
                  [weakSelf checkMergeCaseForSupervisedAccounts];
                  return;
                }
                [weakSelf checkPostSigninAction];
              }));
      return;
    }

    case SHOW_MANAGED_CONFIRMATION: {
      BOOL syncConsent = self.postSignInAction == PostSignInAction::kCommitSync;
      [_performer
          showManagedConfirmationForHostedDomain:_identityToSignInHostedDomain
                                  viewController:_presentingViewController
                                         browser:_browser
                                     syncConsent:syncConsent];
      return;
    }

    case SIGN_OUT_IF_NEEDED:
      [_performer signOutBrowserState:browserState];
      return;

    case CLEAR_DATA:
      [_performer clearDataFromBrowser:_browser commandHandler:_dispatcher];
      return;

    case SIGN_IN:
      [self signInIdentity:_identityToSignIn];
      return;

    case COMMIT_SYNC:
      // TODO(crbug.com/1254359): This step should grant sync consent.
      [self continueSignin];
      return;

    case REGISTER_FOR_USER_POLICY:
      [_performer registerUserPolicy:browserState
                         forIdentity:_identityToSignIn];
      return;

    case FETCH_USER_POLICY:
      [_performer fetchUserPolicy:browserState
                      withDmToken:_dmToken
                         clientID:_clientID
                         identity:_identityToSignIn];
      return;

    case COMPLETE_WITH_SUCCESS:
      [self completeSignInWithSuccess:YES];
      return;

    case COMPLETE_WITH_FAILURE:
      if (_didSignIn) {
        [_performer signOutImmediatelyFromBrowserState:browserState];
      }
      [self completeSignInWithSuccess:NO];
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
  NOTREACHED();
}

- (void)checkPostSigninAction {
  switch (self.postSignInAction) {
    case PostSignInAction::kCommitSync:
      [self checkMergeCaseForIdentityToSignIn];
      break;
    case PostSignInAction::kShowSnackbar:
    case PostSignInAction::kNone:
      [self continueSignin];
      break;
  }
}

// Checks if data should be merged or cleared when `_identityToSignIn`
// is subject to parental controls and then continues sign-in.
- (void)checkMergeCaseForSupervisedAccounts {
  // Always clear the data for supervised accounts if the account
  // is not already signed in.
  self.localDataClearingStrategy = _alreadySignedInWithTheSameAccount
                                       ? SHOULD_CLEAR_DATA_MERGE_DATA
                                       : SHOULD_CLEAR_DATA_CLEAR_DATA;
  [self continueSignin];
}

// Checks if data should be merged or cleared for `_identityToSignIn`.
- (void)checkMergeCaseForIdentityToSignIn {
  if (([_performer shouldHandleMergeCaseForIdentity:_identityToSignIn
                                  browserStatePrefs:[self originalBrowserState]
                                                        ->GetPrefs()])) {
    [_performer promptMergeCaseForIdentity:_identityToSignIn
                                   browser:_browser
                            viewController:_presentingViewController];
  } else {
    // If the user is not prompted to choose a data clearing strategy,
    // Chrome defaults to merging the account data.
    self.localDataClearingStrategy = SHOULD_CLEAR_DATA_MERGE_DATA;
    [self continueSignin];
  }
}

- (void)checkSigninSteps {
  id<SystemIdentity> currentIdentity =
      AuthenticationServiceFactory::GetForBrowserState(
          [self originalBrowserState])
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
  ChromeBrowserState* browserState = [self originalBrowserState];
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);

  if (accountManagerService->IsValidIdentity(identity)) {
    [_performer signInIdentity:identity
                 atAccessPoint:self.accessPoint
              withHostedDomain:_identityToSignInHostedDomain
                toBrowserState:browserState];
    _didSignIn = YES;
    [self continueSignin];
  } else {
    // Handle the case where the identity is no longer valid.
    NSError* error = ios::provider::CreateMissingIdentitySigninError();
    [self handleAuthenticationError:error];
  }
}

- (void)completeSignInWithSuccess:(BOOL)success {
  DCHECK(_signInCompletion)
      << "`completeSignInWithSuccess` should not be called twice.";
  if (success) {
    bool isManagedAccount = _identityToSignInHostedDomain.length > 0;
    signin_metrics::RecordSigninAccountType(signin::ConsentLevel::kSignin,
                                            isManagedAccount);
    // TODO(crbug.com/1462858): Turn sync on was deprecated. Remove this branch
    // after phase 2 on iOS is launched. See ConsentLevel::kSync documentation
    // for details.
    if (self.postSignInAction == PostSignInAction::kCommitSync) {
      signin_metrics::RecordSigninAccountType(signin::ConsentLevel::kSync,
                                              isManagedAccount);
    }
  }
  if (_signInCompletion) {
    CompletionCallback signInCompletion = _signInCompletion;
    _signInCompletion = nil;
    signInCompletion(success);
  }
  if (_shouldShowSigninSnackbar) {
    [_performer showSnackbarWithSignInIdentity:_identityToSignIn
                                       browser:_browser];
  }
  [self continueSignin];
}

- (void)cancelFlow {
  if (_failedOrCancelled) {
    // Avoid double handling of cancel or error.
    return;
  }
  _failedOrCancelled = YES;
  [self continueSignin];
}

- (void)handleAuthenticationError:(NSError*)error {
  if (_failedOrCancelled) {
    // Avoid double handling of cancel or error.
    return;
  }
  DCHECK(error);
  _failedOrCancelled = YES;
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

- (void)didChooseClearDataPolicy:(ShouldClearData)shouldClearData {
  // Assumes this is the first time the user has updated their data clearing
  // strategy.
  DCHECK_NE(SHOULD_CLEAR_DATA_USER_CHOICE, shouldClearData);
  DCHECK_EQ(SHOULD_CLEAR_DATA_USER_CHOICE, self.localDataClearingStrategy);
  _shouldSignOut = YES;
  self.localDataClearingStrategy = shouldClearData;

  [self continueSignin];
}

- (void)didChooseCancel {
  [self cancelFlow];
}

- (void)didFetchManagedStatus:(NSString*)hostedDomain {
  DCHECK_EQ(FETCH_MANAGED_STATUS, _state);
  _shouldShowManagedConfirmation =
      [self shouldShowManagedConfirmationForHostedDomain:hostedDomain];
  _identityToSignInHostedDomain = hostedDomain;
  _shouldFetchUserPolicy = [self shouldFetchUserPolicy];
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
  [self continueSignin];
}

- (void)didCancelManagedConfirmation {
  [self cancelFlow];
}

- (void)didRegisterForUserPolicyWithDMToken:(NSString*)dmToken
                                   clientID:(NSString*)clientID {
  DCHECK_EQ(REGISTER_FOR_USER_POLICY, _state);

  _dmToken = dmToken;
  _clientID = clientID;
  [self continueSignin];
}

- (void)didFetchUserPolicyWithSuccess:(BOOL)success {
  DCHECK_EQ(FETCH_USER_POLICY, _state);
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  [self continueSignin];
}

- (void)dismissPresentingViewControllerAnimated:(BOOL)animated
                                     completion:(ProceduralBlock)completion {
  __weak __typeof(_delegate) weakDelegate = _delegate;
  [_presentingViewController
      dismissViewControllerAnimated:animated
                         completion:^() {
                           [weakDelegate didDismissDialog];
                           if (completion) {
                             completion();
                           }
                         }];
}

- (void)presentViewController:(UIViewController*)viewController
                     animated:(BOOL)animated
                   completion:(ProceduralBlock)completion {
  __weak __typeof(_delegate) weakDelegate = _delegate;
  [_presentingViewController presentViewController:viewController
                                          animated:animated
                                        completion:^() {
                                          [weakDelegate didPresentDialog];
                                          if (completion) {
                                            completion();
                                          }
                                        }];
}

#pragma mark - Private methods

// The original chrome browser state used for services that don't exist in
// incognito mode.
- (ChromeBrowserState*)originalBrowserState {
  return _browser->GetBrowserState()->GetOriginalChromeBrowserState();
}

// Returns YES if the managed confirmation dialog should be shown for the
// hosted domain.
- (BOOL)shouldShowManagedConfirmationForHostedDomain:(NSString*)hostedDomain {
  if ([hostedDomain length] == 0) {
    // No hosted domain, don't show the dialog as there is no host.
    return NO;
  }

  if (self.postSignInAction == PostSignInAction::kCommitSync) {
    // Show the dialog if there is a hosted domain and Sync consent.
    return YES;
  }

  // Show the dialog if User Policy and sign-in only features enabled.
  return policy::IsAnyUserPolicyFeatureEnabled() &&
         base::FeatureList::IsEnabled(
             syncer::kReplaceSyncPromosWithSignInPromos);
}

// Returns YES if should fetch user policy.
- (BOOL)shouldFetchUserPolicy {
  if (self.postSignInAction == PostSignInAction::kCommitSync) {
    return policy::IsUserPolicyEnabledForSigninOrSyncConsentLevel();
  } else {
    return policy::IsAnyUserPolicyFeatureEnabled();
  }
}

#pragma mark - Used for testing

- (void)setPerformerForTesting:(AuthenticationFlowPerformer*)performer {
  _performer = performer;
}

@end
