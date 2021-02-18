
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"

#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/consent_auditor_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

namespace {
const CGFloat kFadeOutAnimationDuration = 0.16f;
}  // namespace

@interface UserSigninCoordinator () <UIAdaptivePresentationControllerDelegate,
                                     UnifiedConsentCoordinatorDelegate,
                                     UserSigninViewControllerDelegate,
                                     UserSigninMediatorDelegate>

// Coordinator that handles the user consent before the user signs in.
@property(nonatomic, strong)
    UnifiedConsentCoordinator* unifiedConsentCoordinator;
// Coordinator that handles adding a user account.
@property(nonatomic, strong) SigninCoordinator* addAccountSigninCoordinator;
// Coordinator that handles the advanced settings sign-in.
@property(nonatomic, strong)
    SigninCoordinator* advancedSettingsSigninCoordinator;
// View controller that handles the sign-in UI.
@property(nonatomic, strong, readwrite)
    UserSigninViewController* viewController;
// Mediator that handles the sign-in authentication state.
@property(nonatomic, strong) UserSigninMediator* mediator;
// Suggested identity shown at sign-in.
@property(nonatomic, strong, readonly) ChromeIdentity* defaultIdentity;
// Logger for sign-in operations.
@property(nonatomic, strong, readonly) UserSigninLogger* logger;
// Sign-in intent.
@property(nonatomic, assign, readonly) UserSigninIntent signinIntent;
// Whether an account has been added during sign-in flow.
@property(nonatomic, assign) BOOL addedAccount;
// YES if the view controller started the presenting animation.
@property(nonatomic, assign) BOOL viewControllerPresentingAnimation;
// Callback to be invoked when the view controller presenting animation is done.
@property(nonatomic, copy) ProceduralBlock interruptCallback;

@end

@implementation UserSigninCoordinator

@synthesize baseNavigationController = _baseNavigationController;

#pragma mark - Public

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                    signinIntent:(UserSigninIntent)signinIntent
                                          logger:(UserSigninLogger*)logger {
  if (self = [self initWithBaseViewController:navigationController
                                      browser:browser
                                     identity:nil
                                 signinIntent:signinIntent
                                       logger:logger]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  identity:(ChromeIdentity*)identity
                              signinIntent:(UserSigninIntent)signinIntent
                                    logger:(UserSigninLogger*)logger {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _defaultIdentity = identity;
    _signinIntent = signinIntent;
    DCHECK(logger);
    _logger = logger;
  }
  return self;
}

#pragma mark - SigninCoordinator

- (void)start {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  // The user should be signed out before triggering sign-in or upgrade states.
  // Users are allowed to be signed-in during FirstRun for testing purposes.
  DCHECK(base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency) ||
         !authenticationService->IsAuthenticated() ||
         self.signinIntent == UserSigninIntentFirstRun);
  [super start];
  self.viewController = [self generateUserSigninViewController];
  self.viewController.delegate = self;
  self.viewController.useFirstRunSkipButton =
      self.signinIntent == UserSigninIntentFirstRun;

  self.mediator = [[UserSigninMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::
                                        GetForBrowserState(
                                            self.browser->GetBrowserState())
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        self.browser->GetBrowserState())
                     consentAuditor:ConsentAuditorFactory::GetForBrowserState(
                                        self.browser->GetBrowserState())
              unifiedConsentService:UnifiedConsentServiceFactory::
                                        GetForBrowserState(
                                            self.browser->GetBrowserState())
                   syncSetupService:SyncSetupServiceFactory::GetForBrowserState(
                                        self.browser->GetBrowserState())];
  self.mediator.delegate = self;

  self.unifiedConsentCoordinator = [[UnifiedConsentCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser];
  self.unifiedConsentCoordinator.delegate = self;

  // Set UnifiedConsentCoordinator properties.
  if (self.defaultIdentity) {
    self.unifiedConsentCoordinator.selectedIdentity = self.defaultIdentity;
  }
  self.unifiedConsentCoordinator.autoOpenIdentityPicker =
      self.logger.promoAction == PromoAction::PROMO_ACTION_NOT_DEFAULT;

  [self.unifiedConsentCoordinator start];

  // Display UnifiedConsentViewController within the host.
  self.viewController.unifiedConsentViewController =
      self.unifiedConsentCoordinator.viewController;

  [self presentUserSigninViewController];
  [self.logger logSigninStarted];
}

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  if (self.mediator.isAuthenticationInProgress) {
    [self.logger
        logSigninCompletedWithResult:SigninCoordinatorResultInterrupted
                        addedAccount:self.addAccountSigninCoordinator != nil
               advancedSettingsShown:self.advancedSettingsSigninCoordinator !=
                                     nil];
  }

  __weak UserSigninCoordinator* weakSelf = self;
  if (self.addAccountSigninCoordinator) {
    // |self.addAccountSigninCoordinator| needs to be interupted before
    // interrupting |self.viewController|.
    // The add account view should not be dismissed since the
    // |self.viewController| will take care of that according to |action|.
    [self.addAccountSigninCoordinator
        interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
                 completion:^{
                   // |self.addAccountSigninCoordinator.signinCompletion|
                   // is expected to be called before this block.
                   // Therefore |weakSelf.addAccountSigninCoordinator| is
                   // expected to be nil.
                   DCHECK(!weakSelf.addAccountSigninCoordinator);
                   [weakSelf interruptUserSigninUIWithAction:action
                                                  completion:completion];
                 }];
    return;
  } else if (self.advancedSettingsSigninCoordinator) {
    // |self.viewController| has already been dismissed. The interruption should
    // be sent to |self.advancedSettingsSigninCoordinator|.
    DCHECK(!self.viewController);
    DCHECK(!self.mediator);
    DCHECK(!self.unifiedConsentCoordinator);
    [self.advancedSettingsSigninCoordinator
        interruptWithAction:action
                 completion:^{
                   // |self.advancedSettingsSigninCoordinator.signinCompletion|
                   // is expected to be called before this block.
                   // Therefore |weakSelf.advancedSettingsSigninCoordinator| is
                   // expected to be nil.
                   DCHECK(!weakSelf.advancedSettingsSigninCoordinator);
                   if (completion) {
                     completion();
                   }
                 }];
    return;
  }
  [self interruptUserSigninUIWithAction:action completion:completion];
}

- (void)stop {
  DCHECK(!self.viewController);
  DCHECK(!self.mediator);
  DCHECK(!self.unifiedConsentCoordinator);
  DCHECK(!self.addAccountSigninCoordinator);
  DCHECK(!self.advancedSettingsSigninCoordinator);
  [super stop];
}

#pragma mark - UnifiedConsentCoordinatorDelegate

- (void)unifiedConsentCoordinatorDidTapSettingsLink:
    (UnifiedConsentCoordinator*)coordinator {
  [self startSigninFlow];
}

- (void)unifiedConsentCoordinatorDidReachBottom:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(self.unifiedConsentCoordinator, coordinator);
  [self.viewController markUnifiedConsentScreenReachedBottom];
}

- (void)unifiedConsentCoordinatorDidTapOnAddAccount:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(self.unifiedConsentCoordinator, coordinator);
  [self userSigninViewControllerDidTapOnAddAccount];
}

- (void)unifiedConsentCoordinatorNeedPrimaryButtonUpdate:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(self.unifiedConsentCoordinator, coordinator);
  [self.viewController setConfirmationButtonProperties];
}

#pragma mark - UserSigninViewControllerDelegate

- (BOOL)unifiedConsentCoordinatorHasIdentity {
  return self.unifiedConsentCoordinator.selectedIdentity != nil;
}

- (void)userSigninViewControllerDidTapOnAddAccount {
  DCHECK(!self.addAccountSigninCoordinator);
  [self notifyUserSigninAttempted];

  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:self.logger.accessPoint];

  __weak UserSigninCoordinator* weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        if (signinResult == SigninCoordinatorResultSuccess) {
          weakSelf.unifiedConsentCoordinator.selectedIdentity =
              signinCompletionInfo.identity;
          weakSelf.addedAccount = YES;
        }
        [weakSelf.addAccountSigninCoordinator stop];
        weakSelf.addAccountSigninCoordinator = nil;
      };
  [self.addAccountSigninCoordinator start];
}

- (void)userSigninViewControllerDidScrollOnUnifiedConsent {
  [self.unifiedConsentCoordinator scrollToBottom];
}

- (void)userSigninViewControllerDidTapOnSkipSignin {
  [self cancelSignin];
}

- (void)userSigninViewControllerDidTapOnSignin {
  [self notifyUserSigninAttempted];
  [self startSigninFlow];
}

#pragma mark - UserSigninMediatorDelegate

- (BOOL)userSigninMediatorGetSettingsLinkWasTapped {
  return self.unifiedConsentCoordinator.settingsLinkWasTapped;
}

- (int)userSigninMediatorGetConsentConfirmationId {
  if (self.userSigninMediatorGetSettingsLinkWasTapped) {
    return self.unifiedConsentCoordinator.openSettingsStringId;
  }
  return self.viewController.acceptSigninButtonStringId;
}

- (const std::vector<int>&)userSigninMediatorGetConsentStringIds {
  return self.unifiedConsentCoordinator.consentStringIds;
}

- (void)userSigninMediatorSigninFinishedWithResult:
    (SigninCoordinatorResult)signinResult {
  [self.logger logSigninCompletedWithResult:signinResult
                               addedAccount:self.addedAccount
                      advancedSettingsShown:self.unifiedConsentCoordinator
                                                .settingsLinkWasTapped];

  BOOL settingsWasTapped = self.unifiedConsentCoordinator.settingsLinkWasTapped;
  ChromeIdentity* identity = self.unifiedConsentCoordinator.selectedIdentity;
  __weak UserSigninCoordinator* weakSelf = self;
  ProceduralBlock completion = ^void() {
    [weakSelf viewControllerDismissedWithResult:signinResult
                                       identity:identity
                          settingsLinkWasTapped:settingsWasTapped];
  };
  switch (self.signinIntent) {
    case UserSigninIntentFirstRun: {
      // The caller is responsible for cleaning up the base view controller for
      // first run sign-in.
      if (completion) {
        completion();
      }
      break;
    }
    case UserSigninIntentSignin:
    case UserSigninIntentUpgrade: {
      if (self.viewController.presentingViewController) {
        [self.viewController.presentingViewController
            dismissViewControllerAnimated:YES
                               completion:completion];
      } else {
        // When the user swipes to dismiss the view controller. The sequence is:
        //  * The user swipe the view controller
        //  * The view controller is dismissed
        //  * [self presentationControllerDidDismiss] is called
        //  * The mediator is canceled
        // And then this method is called by the mediator. Therefore the view
        // controller already dismissed, and should not be dismissed again.
        completion();
      }
      break;
    }
  }
}

- (void)userSigninMediatorSigninFailed {
  [self.unifiedConsentCoordinator resetSettingLinkTapped];
  self.unifiedConsentCoordinator.uiDisabled = NO;
  [self.viewController signinDidStop];
  [self.viewController setConfirmationButtonProperties];
}

#pragma mark - Private

// Cancels the sign-in flow if it is in progress, or dismiss the sign-in view
// if the sign-in is not in progress.
- (void)cancelSignin {
  [self.mediator cancelSignin];
}

// Notifies the observers that the user is attempting sign-in.
- (void)notifyUserSigninAttempted {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kUserSigninAttemptedNotification
                    object:self];
}

// Called when |self.viewController| is dismissed. If |settingsWasTapped| is
// NO, the sign-in is finished and
// |runCompletionCallbackWithSigninResult:identity:| is called.
// Otherwise, the advanced settings sign-in is presented.
- (void)viewControllerDismissedWithResult:(SigninCoordinatorResult)signinResult
                                 identity:(ChromeIdentity*)identity
                    settingsLinkWasTapped:(BOOL)settingsWasTapped {
  DCHECK(!self.addAccountSigninCoordinator);
  DCHECK(!self.advancedSettingsSigninCoordinator);
  DCHECK(self.unifiedConsentCoordinator);
  DCHECK(self.mediator);
  DCHECK(self.viewController);

  [self.unifiedConsentCoordinator stop];
  self.unifiedConsentCoordinator = nil;
  self.mediator = nil;
  self.viewController = nil;

  switch (self.signinIntent) {
    case UserSigninIntentFirstRun: {
      // The UserSigninCoordinator owner is responsible for dismissing views and
      // opening the advanced Settings for the first-run experience.
      break;
    }
    case UserSigninIntentUpgrade:
    case UserSigninIntentSignin: {
      DCHECK(!self.viewController.presentingViewController);
      if (settingsWasTapped) {
        [self displayAdvancedSettings];
        return;
      }
      break;
    }
  }

  [self runCompletionCallbackWithSigninResult:signinResult
                                     identity:identity
                   showAdvancedSettingsSignin:settingsWasTapped];
}

// Displays the Advanced Settings screen of the sign-in flow.
- (void)displayAdvancedSettings {
  self.advancedSettingsSigninCoordinator = [SigninCoordinator
      advancedSettingsSigninCoordinatorWithBaseViewController:
          self.baseViewController
                                                      browser:self.browser];
  __weak UserSigninCoordinator* weakSelf = self;
  self.advancedSettingsSigninCoordinator.signinCompletion = ^(
      SigninCoordinatorResult advancedSigninResult,
      SigninCompletionInfo* signinCompletionInfo) {
    [weakSelf
        advancedSettingsSigninCoordinatorFinishedWithResult:advancedSigninResult
                                                   identity:signinCompletionInfo
                                                                .identity];
  };
  [self.advancedSettingsSigninCoordinator start];
}

// Displays the user sign-in view controller using the available base
// controller. First run requires an additional transitional fade animation when
// presenting this view.
- (void)presentUserSigninViewController {
  // Always set the -UIViewController.modalPresentationStyle before accessing
  // -UIViewController.presentationController.
  self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  if (@available(iOS 13, *)) {
    self.viewController.presentationController.delegate = self;
  }
  switch (self.signinIntent) {
    case UserSigninIntentFirstRun: {
      // Displays the sign-in screen with transitions specific to first-run.
      DCHECK(self.baseNavigationController);

      CATransition* transition = [CATransition animation];
      transition.duration = kFadeOutAnimationDuration;
      transition.type = kCATransitionFade;
      [self.baseNavigationController.view.layer addAnimation:transition
                                                      forKey:kCATransition];
      [self.baseNavigationController pushViewController:self.viewController
                                               animated:NO];
      break;
    }
    case UserSigninIntentUpgrade: {
      // Avoid presenting the promo if the current device orientation is not
      // supported. The promo will be presented at a later moment, when the
      // device orientation is supported.
      UIInterfaceOrientation orientation = GetInterfaceOrientation();
      NSUInteger supportedOrientationsMask =
          [self.viewController supportedInterfaceOrientations];
      if (!((1 << orientation) & supportedOrientationsMask)) {
        [self
            runCompletionCallbackWithSigninResult:
                SigninCoordinatorResultInterrupted
                                         identity:self.unifiedConsentCoordinator
                                                      .selectedIdentity
                       showAdvancedSettingsSignin:NO];
        return;
      }
      [self presentUserViewControllerToBaseViewController];
      break;
    }
    case UserSigninIntentSignin: {
      [self presentUserViewControllerToBaseViewController];
      break;
    }
  }
}

// Presents |self.viewController|. This method is only relevant when
// |self.signinIntent| is not UserSigninIntentFirstRun.
- (void)presentUserViewControllerToBaseViewController {
  DCHECK_NE(UserSigninIntentFirstRun, self.signinIntent);
  DCHECK(self.baseViewController);
  self.viewControllerPresentingAnimation = YES;
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    weakSelf.viewControllerPresentingAnimation = NO;
    if (weakSelf.interruptCallback) {
      // The view controller is fully presented, the coordinator
      // can be dismissed. UIKit doesn't allow a view controller
      // to be dismissed during the animation.
      // See crbug.com/1126170
      ProceduralBlock interruptCallback = weakSelf.interruptCallback;
      weakSelf.interruptCallback = nil;
      interruptCallback();
    }
  };
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:completion];
}

// Interrupts the sign-in when |self.viewController| is presented, by dismissing
// it if needed (according to |action|). Then |completion| is called.
// This method should not be called if |self.addAccountSigninCoordinator| has
// not been stopped before.
- (void)interruptUserSigninUIWithAction:(SigninCoordinatorInterruptAction)action
                             completion:(ProceduralBlock)completion {
  if (self.viewControllerPresentingAnimation) {
    // UIKit doesn't allow a view controller to be dismissed during the
    // animation. The interruption has to be processed when the view controller
    // will be fully presented.
    // See crbug.com/1126170
    DCHECK(!self.interruptCallback);
    __weak __typeof(self) weakSelf = self;
    self.interruptCallback = ^() {
      [weakSelf interruptUserSigninUIWithAction:action completion:completion];
    };
    return;
  }
  DCHECK(self.viewController);
  DCHECK(self.mediator);
  DCHECK(self.unifiedConsentCoordinator);
  DCHECK(!self.addAccountSigninCoordinator);
  DCHECK(!self.advancedSettingsSigninCoordinator);
  __weak UserSigninCoordinator* weakSelf = self;
  ProceduralBlock runCompletionCallback = ^{
    [weakSelf
        viewControllerDismissedWithResult:SigninCoordinatorResultInterrupted
                                 identity:weakSelf.unifiedConsentCoordinator
                                              .selectedIdentity
                    settingsLinkWasTapped:NO];
    if (completion) {
      completion();
    }
  };
  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss: {
      [self.mediator cancelAndDismissAuthenticationFlowAnimated:NO];
      runCompletionCallback();
      break;
    }
    case SigninCoordinatorInterruptActionDismissWithAnimation: {
      [self.mediator cancelAndDismissAuthenticationFlowAnimated:YES];
      [self.viewController.presentingViewController
          dismissViewControllerAnimated:YES
                             completion:runCompletionCallback];
      break;
    }
    case SigninCoordinatorInterruptActionDismissWithoutAnimation: {
      [self.mediator cancelAndDismissAuthenticationFlowAnimated:NO];
      [self.viewController.presentingViewController
          dismissViewControllerAnimated:NO
                             completion:runCompletionCallback];
      break;
    }
  }
}

// Returns user policy used to handle existing data when switching signed in
// account.
- (ShouldClearData)shouldClearData {
  switch (self.signinIntent) {
    case UserSigninIntentFirstRun: {
      return SHOULD_CLEAR_DATA_MERGE_DATA;
    }
    case UserSigninIntentUpgrade:
    case UserSigninIntentSignin: {
      return SHOULD_CLEAR_DATA_USER_CHOICE;
    }
  }
}

// Triggers the sign-in workflow.
- (void)startSigninFlow {
  DCHECK(self.unifiedConsentCoordinator);
  DCHECK(self.unifiedConsentCoordinator.selectedIdentity);
  AuthenticationFlow* authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:self.browser
                      identity:self.unifiedConsentCoordinator.selectedIdentity
               shouldClearData:[self shouldClearData]
              postSignInAction:POST_SIGNIN_ACTION_NONE
      presentingViewController:self.viewController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);
  authenticationFlow.delegate = self.viewController;

  self.unifiedConsentCoordinator.uiDisabled = YES;
  [self.viewController signinWillStart];
  [self.mediator
      authenticateWithIdentity:self.unifiedConsentCoordinator.selectedIdentity
            authenticationFlow:authenticationFlow];
}

// Triggers |self.signinCompletion| by calling
// |runCompletionCallbackWithSigninResult:identity:| when
// |self.advancedSettingsSigninCoordinator| is done.
- (void)advancedSettingsSigninCoordinatorFinishedWithResult:
            (SigninCoordinatorResult)signinResult
                                                   identity:(ChromeIdentity*)
                                                                identity {
  DCHECK(self.advancedSettingsSigninCoordinator);
  [self.advancedSettingsSigninCoordinator stop];
  self.advancedSettingsSigninCoordinator = nil;
  [self runCompletionCallbackWithSigninResult:signinResult
                                     identity:identity
                   showAdvancedSettingsSignin:NO];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // The view should be dismissible only if there is no sign-in in progress.
  // See |presentationControllerShouldDismiss:|.
  DCHECK(!self.mediator.isAuthenticationInProgress);
  [self cancelSignin];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  switch (self.signinIntent) {
    case UserSigninIntentFirstRun: {
      return NO;
    }
    case UserSigninIntentUpgrade:
    case UserSigninIntentSignin: {
      // Don't dismiss the view controller while the sign-in is in progress.
      // To support this, the sign-in flow needs to be canceled after the view
      // controller is dimissed, and the UI needs to be blocked until the
      // sign-in flow is fully cancelled.
      return !self.mediator.isAuthenticationInProgress;
    }
  }
}

#pragma mark - Methods for unittests

// Returns a UserSigninViewController instance. This method is overriden for
// unittests.
- (UserSigninViewController*)generateUserSigninViewController {
  return [[UserSigninViewController alloc] init];
}

@end
