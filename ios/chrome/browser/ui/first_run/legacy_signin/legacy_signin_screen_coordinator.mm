// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/legacy_signin/legacy_signin_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_coordinator.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_delegate.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/legacy_signin/legacy_signin_screen_consumer.h"
#import "ios/chrome/browser/ui/first_run/legacy_signin/legacy_signin_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/legacy_signin/legacy_signin_screen_view_controller.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacySigninScreenCoordinator () <
    EnterprisePromptCoordinatorDelegate,
    IdentityChooserCoordinatorDelegate,
    PolicyWatcherBrowserAgentObserving,
    LegacySigninScreenViewControllerDelegate> {
  // Observer for the sign-out policy changes.
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
}

// First run screen delegate.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;
// Sign-in screen view controller.
@property(nonatomic, strong) LegacySigninScreenViewController* viewController;
// Sign-in screen mediator.
@property(nonatomic, strong) LegacySigninScreenMediator* mediator;
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
// The coordinator that manages enterprise prompts.
@property(nonatomic, strong)
    EnterprisePromptCoordinator* enterprisePromptCoordinator;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// YES if this coordinator is currently used in First Run.
@property(nonatomic, readonly) BOOL firstRun;

@end

@implementation LegacySigninScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
    _baseNavigationController = navigationController;
    _delegate = delegate;
    _policyWatcherObserverBridge =
        std::make_unique<PolicyWatcherBrowserAgentObserverBridge>(self);

    // Determine if the sign-in screen is used in First Run.
    SceneState* sceneState =
        SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
    AppState* appState = sceneState.appState;
    _firstRun = appState.initStage == InitStageFirstRun;
  }
  return self;
}

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  switch (authenticationService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
      // This case should not happen, unless testers trigger the FRE while
      // sign-in is disabled by user.
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      self.attemptStatus = first_run::SignInAttemptStatus::NOT_SUPPORTED;
      [self finishPresentingAndSkipRemainingScreens:NO];
      return;
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
      self.attemptStatus = first_run::SignInAttemptStatus::SKIPPED_BY_POLICY;
      [self finishPresentingAndSkipRemainingScreens:NO];
      return;
    case AuthenticationService::ServiceStatus::SigninAllowed:
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
      break;
  }

  if (authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // Don't show sign in screen if there is already an account signed in (for
    // example going through the FRE then killing the app and restarting the
    // FRE). Don't record any metric as the user didn't take any action.
    [self.delegate screenWillFinishPresenting];
    return;
  }

  PolicyWatcherBrowserAgent::FromBrowser(self.browser)
      ->AddObserver(_policyWatcherObserverBridge.get());

  self.viewController = [[LegacySigninScreenViewController alloc] init];
  self.viewController.delegate = self;
  PrefService* prefService = browserState->GetPrefs();
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  self.viewController.enterpriseSignInRestrictions =
      GetEnterpriseSignInRestrictions(authenticationService, prefService,
                                      syncService);

  self.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);

  self.mediator = [[LegacySigninScreenMediator alloc]
      initWithAccountManagerService:self.accountManagerService
              authenticationService:authenticationService];

  self.mediator.selectedIdentity =
      self.accountManagerService->GetDefaultIdentity();
  self.hadIdentitiesAtStartup = self.accountManagerService->HasIdentities();

  self.mediator.consumer = self.viewController;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
  self.viewController.modalInPresentation = YES;

  if (self.firstRun) {
    base::UmaHistogramEnumeration("FirstRun.Stage",
                                  first_run::kSignInScreenStart);
  }
}

- (void)stop {
  PolicyWatcherBrowserAgent::FromBrowser(self.browser)
      ->RemoveObserver(_policyWatcherObserverBridge.get());

  self.delegate = nil;
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator = nil;
  // If advancedSettingsSigninCoordinator wasn't dismissed yet (which can
  // happen when closing the scene), try to call -interruptWithAction: to
  // properly cleanup the coordinator.
  SigninCoordinator* signinCoordiantor = self.addAccountSigninCoordinator;
  [self.addAccountSigninCoordinator
      interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
               completion:^() {
                 [signinCoordiantor stop];
               }];
  self.addAccountSigninCoordinator = nil;
  [self.enterprisePromptCoordinator stop];
  self.enterprisePromptCoordinator = nil;
}

#pragma mark - LegacySigninScreenViewControllerDelegate

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

- (void)logScrollButtonVisible:(BOOL)scrollButtonVisible
            withIdentityPicker:(BOOL)identityPickerVisible
                     andFooter:(BOOL)footerVisible {
  first_run::FirstRunScreenType screenType;
  if (identityPickerVisible && footerVisible) {
    screenType =
        first_run::FirstRunScreenType::kSignInScreenWithFooterAndIdentityPicker;
  } else if (identityPickerVisible) {
    screenType = first_run::FirstRunScreenType::kSignInScreenWithIdentityPicker;
  } else if (footerVisible) {
    screenType = first_run::FirstRunScreenType::kSignInScreenWithFooter;
  } else {
    screenType = first_run::FirstRunScreenType::
        kSignInScreenWithoutFooterOrIdentityPicker;
  }
  RecordFirstRunScrollButtonVisibilityMetrics(screenType, scrollButtonVisible);
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
  if (self.firstRun) {
    base::UmaHistogramEnumeration(
        "FirstRun.Stage", first_run::kSignInScreenCompletionWithoutSignIn);
  }
}

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator = nil;
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  DCHECK(!self.addAccountSigninCoordinator);

  [self triggerAddAccount];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(id<SystemIdentity>)identity {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  self.mediator.selectedIdentity = identity;
}

#pragma mark - PolicyWatcherBrowserAgentObserving

- (void)policyWatcherBrowserAgentNotifySignInDisabled:
    (PolicyWatcherBrowserAgent*)policyWatcher {
  if (self.addAccountSigninCoordinator) {
    __weak __typeof(self) weakSelf = self;
    [self.addAccountSigninCoordinator
        interruptWithAction:SigninCoordinatorInterruptActionDismissWithAnimation
                 completion:^{
                   [weakSelf showSignedOutModal];
                 }];
  } else {
    [self showSignedOutModal];
  }
}

#pragma mark - EnterprisePromptCoordinatorDelegate

- (void)hideEnterprisePrompForLearnMore:(BOOL)learnMore {
  [self dismissSignedOutModalAndSkipScreens:learnMore];
}

#pragma mark - Private

// Dismisses the Signed Out modal if it is still present and `skipScreens`.
- (void)dismissSignedOutModalAndSkipScreens:(BOOL)skipScreens {
  [self.enterprisePromptCoordinator stop];
  self.enterprisePromptCoordinator = nil;
  [self finishPresentingAndSkipRemainingScreens:skipScreens];
}

// Shows the modal letting the user know that they have been signed out.
- (void)showSignedOutModal {
  self.attemptStatus = first_run::SignInAttemptStatus::SKIPPED_BY_POLICY;
  self.enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                      promptType:EnterprisePromptTypeForceSignOut];
  self.enterprisePromptCoordinator.delegate = self;
  [self.enterprisePromptCoordinator start];
}

// Completes the presentation of the screen, recording the metrics and notifying
// the delegate to skip the rest of the FRE if `skipRemainingScreens` is YES, or
// to continue the FRE.
- (void)finishPresentingAndSkipRemainingScreens:(BOOL)skipRemainingScreens {
  if (self.firstRun) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    RecordFirstRunSignInMetrics(identityManager, self.attemptStatus,
                                self.hadIdentitiesAtStartup);
  }
  if (skipRemainingScreens) {
    [self.delegate skipAllScreens];
  } else {
    [self.delegate screenWillFinishPresenting];
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
  if (signinResult == SigninCoordinatorResultSuccess &&
      self.accountManagerService->IsValidIdentity(
          signinCompletionInfo.identity)) {
    self.mediator.selectedIdentity = signinCompletionInfo.identity;
    self.mediator.addedAccount = YES;
  }
}

// Starts the sign in process.
- (void)startSignIn {
  DCHECK(self.mediator.selectedIdentity);

  self.attemptStatus = first_run::SignInAttemptStatus::ATTEMPTED;
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:self.mediator.selectedIdentity
                                 postSignInAction:POST_SIGNIN_ACTION_NONE
                         presentingViewController:self.viewController];
  __weak __typeof(self) weakSelf = self;
  [self.mediator
      startSignInWithAuthenticationFlow:authenticationFlow
                             completion:^() {
                               [weakSelf
                                   finishPresentingAndSkipRemainingScreens:NO];
                               if (self.firstRun) {
                                 base::UmaHistogramEnumeration(
                                     "FirstRun.Stage",
                                     first_run::
                                         kSignInScreenCompletionWithSignIn);
                               }
                             }];
}

@end
