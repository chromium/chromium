// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/authentication_flow.h"

#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/signin_error_provider.h"
#include "ui/base/l10n/l10n_util.h"

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
  START_SYNC,
  COMPLETE_WITH_SUCCESS,
  COMPLETE_WITH_FAILURE,
  CLEANUP_BEFORE_DONE,
  DONE
};

NSError* IdentityMissingError() {
  ios::SigninErrorProvider* provider =
      ios::GetChromeBrowserProvider()->GetSigninErrorProvider();
  return [NSError
      errorWithDomain:provider->GetSigninErrorDomain()
                 code:provider->GetCode(ios::SigninError::MISSING_IDENTITY)
             userInfo:nil];
}

}  // namespace

@interface AuthenticationFlow ()

// Whether this flow is curently handling an error.
@property(nonatomic, assign) BOOL handlingError;

// Checks which sign-in steps to perform and updates member variables
// accordingly.
- (void)checkSigninSteps;

// Continues the sign-in state machine starting from |_state| and invokes
// |completion_| when finished.
- (void)continueSignin;

// Runs |completion_| asynchronously with |success| argument.
- (void)completeSignInWithSuccess:(BOOL)success;

// Cancels the current sign-in flow.
- (void)cancelFlow;

// Handles an authentication error and show an alert to the user.
- (void)handleAuthenticationError:(NSError*)error;

@end

@implementation AuthenticationFlow {
  ShouldClearData _shouldClearData;
  PostSignInAction _postSignInAction;
  UIViewController* _presentingViewController;
  CompletionCallback _signInCompletion;
  AuthenticationFlowPerformer* _performer;

  // State machine tracking.
  AuthenticationState _state;
  BOOL _didSignIn;
  BOOL _failedOrCancelled;
  BOOL _shouldSignIn;
  BOOL _shouldSignOut;
  BOOL _shouldShowManagedConfirmation;
  BOOL _shouldStartSync;
  Browser* _browser;
  ChromeIdentity* _browserStateIdentity;
  ChromeIdentity* _identityToSignIn;
  NSString* _identityToSignInHostedDomain;

  // This AuthenticationFlow keeps a reference to |self| while a sign-in flow is
  // is in progress to ensure it outlives any attempt to destroy it in
  // |_signInCompletion|.
  AuthenticationFlow* _selfRetainer;
}

@synthesize handlingError = _handlingError;
@synthesize dispatcher = _dispatcher;

#pragma mark - Public methods

- (instancetype)initWithBrowser:(Browser*)browser
                       identity:(ChromeIdentity*)identity
                shouldClearData:(ShouldClearData)shouldClearData
               postSignInAction:(PostSignInAction)postSignInAction
       presentingViewController:(UIViewController*)presentingViewController {
  if ((self = [super init])) {
    DCHECK(browser);
    DCHECK(presentingViewController);
    _browser = browser;
    _identityToSignIn = identity;
    _shouldClearData = shouldClearData;
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

- (void)cancelAndDismiss {
  if (_state == DONE)
    return;

  [_performer cancelAndDismiss];
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

#pragma mark State machine management

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
    case START_SYNC:
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
      if (_shouldSignIn)
        return FETCH_MANAGED_STATUS;
      else
        return CHECK_MERGE_CASE;
    case FETCH_MANAGED_STATUS:
      return CHECK_MERGE_CASE;
    case CHECK_MERGE_CASE:
      if (_shouldShowManagedConfirmation)
        return SHOW_MANAGED_CONFIRMATION;
      else if (_shouldSignOut)
        return SIGN_OUT_IF_NEEDED;
      else if (_shouldClearData == SHOULD_CLEAR_DATA_CLEAR_DATA)
        return CLEAR_DATA;
      else if (_shouldSignIn)
        return SIGN_IN;
      else
        return COMPLETE_WITH_SUCCESS;
    case SHOW_MANAGED_CONFIRMATION:
      if (_shouldSignOut)
        return SIGN_OUT_IF_NEEDED;
      else if (_shouldClearData == SHOULD_CLEAR_DATA_CLEAR_DATA)
        return CLEAR_DATA;
      else if (_shouldSignIn)
        return SIGN_IN;
      else
        return COMPLETE_WITH_SUCCESS;
    case SIGN_OUT_IF_NEEDED:
      return _shouldClearData == SHOULD_CLEAR_DATA_CLEAR_DATA ? CLEAR_DATA
                                                              : SIGN_IN;
    case CLEAR_DATA:
      return SIGN_IN;
    case SIGN_IN:
      if (_shouldStartSync)
        return START_SYNC;
      else
        return COMPLETE_WITH_SUCCESS;
    case START_SYNC:
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
  ios::ChromeBrowserState* browserState = _browser->GetBrowserState();
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

    case CHECK_MERGE_CASE:
      if ([_performer shouldHandleMergeCaseForIdentity:_identityToSignIn
                                          browserState:browserState]) {
        if (_shouldClearData == SHOULD_CLEAR_DATA_USER_CHOICE) {
          [_performer promptMergeCaseForIdentity:_identityToSignIn
                                         browser:_browser
                                  viewController:_presentingViewController];
          return;
        }
      }
      [self continueSignin];
      return;

    case SHOW_MANAGED_CONFIRMATION:
      [_performer
          showManagedConfirmationForHostedDomain:_identityToSignInHostedDomain
                                  viewController:_presentingViewController];
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

    case START_SYNC:
      [_performer commitSyncForBrowserState:browserState];
      [self continueSignin];
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
      // Clean up asynchronously to ensure that |self| does not die while
      // the flow is running.
      DCHECK([NSThread isMainThread]);
      dispatch_async(dispatch_get_main_queue(), ^{
        _selfRetainer = nil;
      });
      [self continueSignin];
      return;
    }
    case DONE:
      return;
  }
  NOTREACHED();
}

- (void)checkSigninSteps {
  _browserStateIdentity = AuthenticationServiceFactory::GetForBrowserState(
                              _browser->GetBrowserState())
                              ->GetAuthenticatedIdentity();
  if (_browserStateIdentity)
    _shouldSignOut = YES;

  _shouldSignIn = YES;
  _shouldStartSync = _postSignInAction == POST_SIGNIN_ACTION_START_SYNC;
}

- (void)signInIdentity:(ChromeIdentity*)identity {
  if (ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->IsValidIdentity(identity)) {
    [_performer signInIdentity:identity
              withHostedDomain:_identityToSignInHostedDomain
                toBrowserState:_browser->GetBrowserState()];
    _didSignIn = YES;
    [self continueSignin];
  } else {
    // Handle the case where the identity is no longer valid.
    [self handleAuthenticationError:IdentityMissingError()];
  }
}

- (void)completeSignInWithSuccess:(BOOL)success {
  DCHECK(_signInCompletion)
      << "|completeSignInWithSuccess| should not be called twice.";
  _signInCompletion(success);
  _signInCompletion = nil;
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
                       viewController:_presentingViewController];
}

#pragma mark AuthenticationFlowPerformerDelegate

- (void)didSignOut {
  [self continueSignin];
}

- (void)didClearData {
  [self continueSignin];
}

- (void)didChooseClearDataPolicy:(ShouldClearData)shouldClearData {
  DCHECK_NE(SHOULD_CLEAR_DATA_USER_CHOICE, shouldClearData);
  _shouldSignOut = YES;
  _shouldClearData = shouldClearData;
  [self continueSignin];
}

- (void)didChooseCancel {
  [self cancelFlow];
}

- (void)didFetchManagedStatus:(NSString*)hostedDomain {
  DCHECK_EQ(FETCH_MANAGED_STATUS, _state);
  _shouldShowManagedConfirmation = [hostedDomain length] > 0;
  _identityToSignInHostedDomain = hostedDomain;
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

- (UIViewController*)presentingViewController {
  return _presentingViewController;
}

#pragma mark - Used for testing

- (void)setPerformerForTesting:(AuthenticationFlowPerformer*)performer {
  _performer = performer;
}

@end
