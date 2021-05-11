// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_consumer.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_mediator_delegate.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_view_controller.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninScreenCoordinator () <IdentityChooserCoordinatorDelegate,
                                       SigninScreenMediatorDelegate,
                                       SigninScreenViewControllerDelegate>

// First run screen delegate.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;
// Sign-in screen view controller.
@property(nonatomic, strong) SigninScreenViewController* viewController;
// Sign-in screen mediator.
@property(nonatomic, strong) SigninScreenMediator* mediator;
// Coordinator handling choosing the account to sign in with.
@property(nonatomic, strong)
    IdentityChooserCoordinator* identityChooserCoordinator;
// Coordinator handling adding a user account.
@property(nonatomic, strong) SigninCoordinator* addAccountSigninCoordinator;
// Whether the user attempted to sign in (the attempt can be successful, failed
// or canceled).
@property(nonatomic, assign) first_run::SignInAttemptStatus attemptStatus;
// Whether there was existing accounts when the screen was presented.
@property(nonatomic, assign) BOOL hadIdentitiesAtStartup;

@end

@implementation SigninScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  // TODO(crbug.com/1189836): The kSigninAllowed pref should be observed in case
  // the policy is applied while this screen is presented.

  if (!signin::IsSigninAllowed(self.browser->GetBrowserState()->GetPrefs())) {
    self.attemptStatus = first_run::SignInAttemptStatus::SKIPPED_BY_POLICY;
    [self finishPresentingAndSkipRemainingScreens:NO];
    return;
  }
  self.hadIdentitiesAtStartup = ios::GetChromeBrowserProvider()
                                    ->GetChromeIdentityService()
                                    ->HasIdentities();
  self.viewController = [[SigninScreenViewController alloc] init];
  self.viewController.delegate = self;
  self.mediator = [[SigninScreenMediator alloc]
      initWithPrefService:self.browser->GetBrowserState()->GetPrefs()];
  NSArray* identities = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->GetAllIdentitiesSortedForDisplay(
                                self.browser->GetBrowserState()->GetPrefs());
  ChromeIdentity* newIdentity = nil;
  if (identities.count != 0) {
    newIdentity = identities[0];
  }
  self.mediator.selectedIdentity = newIdentity;
  self.mediator.consumer = self.viewController;
  self.mediator.delegate = self;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
}

- (void)stop {
  self.delegate = nil;
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator = nil;
  [self.addAccountSigninCoordinator stop];
  self.addAccountSigninCoordinator = nil;
}

#pragma mark - SigninScreenViewControllerDelegate

- (void)showAccountPickerFromPoint:(CGPoint)point {
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
  self.identityChooserCoordinator.selectedIdentity =
      self.mediator.selectedIdentity;
}

- (void)didTapPrimaryActionButton {
  if (self.mediator.selectedIdentity) {
    [self startSignIn];
  } else {
    [self triggerAddAccount];
  }
}

- (void)didTapSecondaryActionButton {
  [self finishPresentingAndSkipRemainingScreens:NO];
}

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  self.identityChooserCoordinator.delegate = nil;
  self.identityChooserCoordinator = nil;
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  DCHECK(!self.addAccountSigninCoordinator);

  [self triggerAddAccount];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(ChromeIdentity*)identity {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  self.mediator.selectedIdentity = identity;
}

#pragma mark - SigninScreenMediatorDelegate

- (void)signinScreenMediator:(SigninScreenMediator*)mediator
    didFinishSigninWithResult:(SigninCoordinatorResult)result {
  [self finishPresentingAndSkipRemainingScreens:NO];
}

#pragma mark - Private

// Completes the presentation of the screen, recording the metrics and notifying
// the delegate to skip the rest of the FRE if |skipRemainingScreens| is YES, or
// to continue the FRE.
- (void)finishPresentingAndSkipRemainingScreens:(BOOL)skipRemainingScreens {
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  RecordFirstRunSignInMetrics(identityManager, self.attemptStatus,
                              self.hadIdentitiesAtStartup);

  if (skipRemainingScreens) {
    [self.delegate skipAll];
  } else {
    [self.delegate willFinishPresenting];
  }
}

// Starts the coordinator to present the Add Account module.
- (void)triggerAddAccount {
  self.attemptStatus = first_run::SignInAttemptStatus::ATTEMPTED;

  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_START_PAGE];

  __weak __typeof(self) weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf addAccountSigninCompleteWithResult:signinResult
                                      completionInfo:signinCompletionInfo];
      };
  [self.addAccountSigninCoordinator start];
}

// Callback handling the completion of the AddAccount action.
- (void)addAccountSigninCompleteWithResult:(SigninCoordinatorResult)signinResult
                            completionInfo:
                                (SigninCompletionInfo*)signinCompletionInfo {
  [self.addAccountSigninCoordinator stop];
  self.addAccountSigninCoordinator = nil;
  if (signinResult == SigninCoordinatorResultSuccess) {
    self.mediator.selectedIdentity = signinCompletionInfo.identity;
    self.mediator.addedAccount = YES;
  }
  if (signinCompletionInfo.signinCompletionAction ==
      SigninCompletionActionOpenCompletionURL) {
    // The user asked to create a new account.
    DCHECK(signinCompletionInfo.completionURL.is_valid());
    UrlLoadParams params =
        UrlLoadParams::InCurrentTab(signinCompletionInfo.completionURL);
    params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);

    [self finishPresentingAndSkipRemainingScreens:YES];
  }
}

// Starts the sign in process.
- (void)startSignIn {
  DCHECK(self.mediator.selectedIdentity);

  self.attemptStatus = first_run::SignInAttemptStatus::ATTEMPTED;

  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:self.mediator.selectedIdentity
                                  shouldClearData:SHOULD_CLEAR_DATA_MERGE_DATA
                                 postSignInAction:POST_SIGNIN_ACTION_NONE
                         presentingViewController:self.viewController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);
  authenticationFlow.delegate = self.viewController;

  [self.mediator startSignInWithAuthenticationFlow:authenticationFlow];
}

@end
