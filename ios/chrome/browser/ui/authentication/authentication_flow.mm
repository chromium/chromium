// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "base/strings/sys_string_conversions.h"

#import "base/check_op.h"
#import "base/ios/block_types.h"
#import "base/notreached.h"
#import "components/signin/ios/browser/features.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  // YES if the signed in account is a managed account and the sign-in flow
  // includes sync.
  BOOL _shouldShowManagedConfirmation;
  // YES if user policies have to be fetched.
  BOOL _shouldFetchUserPolicy;

  Browser* _browser;
  id<SystemIdentity> _identityToSignIn;
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
               postSignInAction:(PostSignInAction)postSignInAction
       presentingViewController:(UIViewController*)presentingViewController {
  if ((self = [super init])) {
    DCHECK(browser);
    DCHECK(presentingViewController);
    DCHECK(identity);
    _browser = browser;
    _identityToSignIn = identity;
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
  [self continueSignin];
}

- (void)cancelAndDismissAnimated:(BOOL)animated {
  if (_state == DONE)
    return;

  [_performer cancelAndDismissAnimated:animated];
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
      DCHECK(self.postSignInAction == POST_SIGNIN_ACTION_NONE ||
             (self.postSignInAction == POST_SIGNIN_ACTION_COMMIT_SYNC &&
              self.localDataClearingStrategy != SHOULD_CLEAR_DATA_USER_CHOICE));
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
        case POST_SIGNIN_ACTION_COMMIT_SYNC:
          return COMMIT_SYNC;
        case POST_SIGNIN_ACTION_NONE:
          return COMPLETE_WITH_SUCCESS;
      }
    case COMMIT_SYNC:
      if (policy::IsUserPolicyEnabled() && _shouldFetchUserPolicy)
        return REGISTER_FOR_USER_POLICY;
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
  ChromeBrowserState* browserState = _browser->GetBrowserState();
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
      __weak AuthenticationFlow* weakSelf = self;
      ios::CapabilitiesCallback callback =
          ^(ios::ChromeIdentityCapabilityResult result) {
            if (result == ios::ChromeIdentityCapabilityResult::kTrue) {
              [weakSelf didChooseClearDataPolicy:SHOULD_CLEAR_DATA_CLEAR_DATA];
              return;
            }
            switch (weakSelf.postSignInAction) {
              case POST_SIGNIN_ACTION_COMMIT_SYNC:
                [weakSelf checkMergeCaseForUnsupervisedAccounts];
                break;
              case POST_SIGNIN_ACTION_NONE:
                [weakSelf continueSignin];
                break;
            }
          };
      if (base::FeatureList::IsEnabled(signin::kEnableUnicornAccountSupport)) {
        ios::ChromeIdentityService* identity_service =
            ios::GetChromeBrowserProvider().GetChromeIdentityService();
        identity_service->IsSubjectToParentalControls(_identityToSignIn,
                                                      callback);
      } else {
        callback(ios::ChromeIdentityCapabilityResult::kFalse);
      }
      return;
    }
    case SHOW_MANAGED_CONFIRMATION:
      [_performer
          showManagedConfirmationForHostedDomain:_identityToSignInHostedDomain
                                  viewController:_presentingViewController
                                         browser:_browser];
      return;

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
      [_performer commitSyncForBrowserState:browserState];
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
        // Enabling/disabling sync does not take effect in the sync backend
        // until committing changes.
        [_performer commitSyncForBrowserState:browserState];
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

- (void)checkMergeCaseForUnsupervisedAccounts {
  if (([_performer shouldHandleMergeCaseForIdentity:_identityToSignIn
                                  browserStatePrefs:_browser->GetBrowserState()
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
          _browser->GetBrowserState())
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (currentIdentity && ![currentIdentity isEqual:_identityToSignIn]) {
    // If the identity to sign-in is different than the current identity,
    // sign-out is required.
    _shouldSignOut = YES;
  }
}

- (void)signInIdentity:(id<SystemIdentity>)identity {
  ChromeBrowserState* browserState = _browser->GetBrowserState();
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);

  if (accountManagerService->IsValidIdentity(identity)) {
    [_performer signInIdentity:identity
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
    if (self.postSignInAction == POST_SIGNIN_ACTION_COMMIT_SYNC)
      signin_metrics::RecordSigninAccountType(signin::ConsentLevel::kSync,
                                              isManagedAccount);
  }
  if (_signInCompletion) {
    // Make sure the completion callback is always called after
    // -[AuthenticationFlow startSignInWithCompletion:] returns.
    CompletionCallback signInCompletion = _signInCompletion;
    _signInCompletion = nil;
    dispatch_async(dispatch_get_main_queue(), ^{
      signInCompletion(success);
    });
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
      [hostedDomain length] > 0 &&
      (self.postSignInAction == POST_SIGNIN_ACTION_COMMIT_SYNC);
  _identityToSignInHostedDomain = hostedDomain;
  _shouldFetchUserPolicy = YES;
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

#pragma mark - Used for testing

- (void)setPerformerForTesting:(AuthenticationFlowPerformer*)performer {
  _performer = performer;
}

@end
