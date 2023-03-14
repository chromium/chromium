// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"

#import "base/ios/block_types.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

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
@property(nonatomic, strong, readonly) id<SystemIdentity> defaultIdentity;
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
// User sign-in state when the coordinator starts. This is used as the
// state to revert to in case the user is interrupted during sign-in.
@property(nonatomic, assign) IdentitySigninState signinStateOnStart;
// Sign-in identity when the coordiantor starts. This is used as the
// identity to revert to in case the user is interrupted during sign-in.
@property(nonatomic, strong) id<SystemIdentity> signinIdentityOnStart;
// Account manager service to retrieve Chrome identities.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// YES if the user tapped on the managed, learn more link.
@property(nonatomic, assign) BOOL managedLearnMoreLinkWasTapped;

@end

@implementation UserSigninCoordinator

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  identity:(id<SystemIdentity>)identity
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
  self.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  [super start];

  self.signinStateOnStart =
      signin::GetPrimaryIdentitySigninState(self.browser->GetBrowserState());
  DCHECK_NE(IdentitySigninStateSignedInWithSyncEnabled,
            self.signinStateOnStart);
  self.signinIdentityOnStart =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  // Setup mediator.
  self.mediator = [[UserSigninMediator alloc]
      initWithAuthenticationService:authenticationService
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        self.browser->GetBrowserState())
              accountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForBrowserState(
                                            self.browser->GetBrowserState())
                     consentAuditor:ConsentAuditorFactory::GetForBrowserState(
                                        self.browser->GetBrowserState())
              unifiedConsentService:UnifiedConsentServiceFactory::
                                        GetForBrowserState(
                                            self.browser->GetBrowserState())
                   syncSetupService:SyncSetupServiceFactory::GetForBrowserState(
                                        self.browser->GetBrowserState())
                        syncService:SyncServiceFactory::GetForBrowserState(
                                        self.browser->GetBrowserState())];
  self.mediator.delegate = self;

  // Setup UnifiedConsentCoordinator.
  BOOL postRestoreSigninPromo =
      self.logger.accessPoint ==
      AccessPoint::ACCESS_POINT_POST_DEVICE_RESTORE_SIGNIN_PROMO;
  self.unifiedConsentCoordinator = [[UnifiedConsentCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser
          postRestoreSigninPromo:postRestoreSigninPromo];
  self.unifiedConsentCoordinator.delegate = self;
  if (self.defaultIdentity) {
    self.unifiedConsentCoordinator.selectedIdentity = self.defaultIdentity;
  }
  self.unifiedConsentCoordinator.autoOpenIdentityPicker =
      self.logger.promoAction == PromoAction::PROMO_ACTION_NOT_DEFAULT;
  [self.unifiedConsentCoordinator start];

  // Setup view controller.
  self.viewController =
      [self generateUserSigninViewControllerWithUnifiedConsentViewController:
                self.unifiedConsentCoordinator.viewController];
  self.viewController.delegate = self;

  // Start.
  [self presentUserSigninViewController];
  [self.logger logSigninStarted];
}

// Interrupts the sign-in flow.
// `signinCompletion(SigninCoordinatorResultInterrupted, nil)` is guaranteed to
// be called before `completion()`.
// `action` action describing how to interrupt the sign-in.
// `completion` called once the sign-in is fully interrupted.
- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  if (self.mediator.isAuthenticationInProgress) {
    [self.logger
        logSigninCompletedWithResult:SigninCoordinatorResultInterrupted
                        addedAccount:self.addAccountSigninCoordinator != nil
               advancedSettingsShown:self.advancedSettingsSigninCoordinator !=
                                     nil];
  }
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:nil];
  __weak UserSigninCoordinator* weakSelf = self;
  ProceduralBlock completionAction = ^{
    [weakSelf interruptUserSigninUIWithAction:action
                         signinCompletionInfo:completionInfo
                                   completion:completion];
  };
  if (self.addAccountSigninCoordinator) {
    // `self.addAccountSigninCoordinator` needs to be interupted before
    // interrupting `self.viewController`.
    // The add account view should not be dismissed since the
    // `self.viewController` will take care of that according to `action`.
    [self.addAccountSigninCoordinator
        interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
                 completion:^{
                   // `self.addAccountSigninCoordinator.signinCompletion`
                   // is expected to be called before this block.
                   // Therefore `weakSelf.addAccountSigninCoordinator` is
                   // expected to be nil.
                   DCHECK(!weakSelf.addAccountSigninCoordinator);
                   completionAction();
                 }];
    return;
  } else if (self.advancedSettingsSigninCoordinator) {
    // `self.viewController` has already been dismissed. The interruption should
    // be sent to `self.advancedSettingsSigninCoordinator`.
    [self.advancedSettingsSigninCoordinator
        interruptWithAction:action
                 completion:^{
                   // `self.advancedSettingsSigninCoordinator.signinCompletion`
                   // is expected to be called before this block.
                   // Therefore `weakSelf.advancedSettingsSigninCoordinator` is
                   // expected to be nil.
                   DCHECK(!weakSelf.advancedSettingsSigninCoordinator);
                   completionAction();
                 }];
    return;
  } else {
    completionAction();
  }
}

- (void)stop {
  DCHECK(!self.viewController);
  DCHECK(!self.mediator);
  DCHECK(!self.unifiedConsentCoordinator);
  DCHECK(!self.addAccountSigninCoordinator);
  DCHECK(!self.advancedSettingsSigninCoordinator);
  [super stop];
  [self.logger disconnect];
}

#pragma mark - UnifiedConsentCoordinatorDelegate

- (void)unifiedConsentCoordinatorDidTapSettingsLink:
    (UnifiedConsentCoordinator*)coordinator {
  [self startSigninFlow];
}

- (void)unifiedConsentCoordinatorDidTapLearnMoreLink:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK(!self.managedLearnMoreLinkWasTapped);
  self.managedLearnMoreLinkWasTapped = YES;
  [self cancelSignin];
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
  [self.viewController updatePrimaryActionButtonStyle];
}

#pragma mark - UserSigninViewControllerDelegate

- (BOOL)unifiedConsentCoordinatorHasIdentity {
  return self.unifiedConsentCoordinator.selectedIdentity != nil;
}

- (void)userSigninViewControllerDidTapOnAddAccount {
  DCHECK(!self.addAccountSigninCoordinator);

  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:self.logger.accessPoint];

  __weak UserSigninCoordinator* weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf addAccountSigninCompleteWithResult:signinResult
                                      completionInfo:signinCompletionInfo];
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
  [self startSigninFlow];
}

#pragma mark - UserSigninMediatorDelegate

- (BOOL)userSigninMediatorGetSettingsLinkWasTapped {
  return self.unifiedConsentCoordinator.settingsLinkWasTapped;
}

- (int)userSigninMediatorGetConsentConfirmationId {
  return self.viewController.acceptSigninButtonStringId;
}

- (const std::vector<int>&)userSigninMediatorGetConsentStringIds {
  return self.unifiedConsentCoordinator.consentStringIds;
}

- (void)userSigninMediatorSigninFinishedWithResult:
    (SigninCoordinatorResult)signinResult {
  [self.viewController signinDidStop];
  [self.logger logSigninCompletedWithResult:signinResult
                               addedAccount:self.addedAccount
                      advancedSettingsShown:self.unifiedConsentCoordinator
                                                .settingsLinkWasTapped];

  id<SystemIdentity> identity =
      (signinResult == SigninCoordinatorResultSuccess)
          ? self.unifiedConsentCoordinator.selectedIdentity
          : nil;
  SigninCompletionAction completionAction = SigninCompletionActionNone;
  if (self.managedLearnMoreLinkWasTapped) {
    completionAction = SigninCompletionActionShowManagedLearnMore;
  } else if (self.unifiedConsentCoordinator.settingsLinkWasTapped) {
    // Sign-in is finished but the advanced settings link was tapped.
    [self displayAdvancedSettings];
    return;
  }
  SigninCompletionInfo* completionInfo =
      [[SigninCompletionInfo alloc] initWithIdentity:identity
                              signinCompletionAction:completionAction];
  __weak UserSigninCoordinator* weakSelf = self;
  ProceduralBlock completion = ^void() {
    [weakSelf viewControllerDismissedWithResult:signinResult
                                 completionInfo:completionInfo];
  };
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
}

- (void)userSigninMediatorSigninFailed {
  [self.unifiedConsentCoordinator resetSettingLinkTapped];
  DCHECK(!self.managedLearnMoreLinkWasTapped);
  self.unifiedConsentCoordinator.uiDisabled = NO;
  [self.viewController signinDidStop];
  [self.viewController updatePrimaryActionButtonStyle];
}

#pragma mark - Private

// Cancels the sign-in flow if it is in progress, or dismiss the sign-in view
// if the sign-in is not in progress.
- (void)cancelSignin {
  [self.mediator cancelSignin];
}

// Called when `self.viewController` is dismissed. The sign-in is
// finished and `runCompletionCallbackWithSigninResult:completionInfo:` is
// called.
- (void)viewControllerDismissedWithResult:(SigninCoordinatorResult)signinResult
                           completionInfo:
                               (SigninCompletionInfo*)completionInfo {
  DCHECK(!self.addAccountSigninCoordinator);
  DCHECK(!self.advancedSettingsSigninCoordinator);
  DCHECK(self.unifiedConsentCoordinator);
  DCHECK(self.mediator);
  DCHECK(self.viewController);

  [self.unifiedConsentCoordinator stop];
  self.unifiedConsentCoordinator = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;

  [self runCompletionCallbackWithSigninResult:signinResult
                               completionInfo:completionInfo];
}

// Displays the Advanced Settings screen of the sign-in flow.
- (void)displayAdvancedSettings {
  self.advancedSettingsSigninCoordinator = [SigninCoordinator
      advancedSettingsSigninCoordinatorWithBaseViewController:
          self.viewController
                                                      browser:self.browser
                                                  signinState:
                                                      self.signinStateOnStart];
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
  self.viewController.presentationController.delegate = self;

  switch (self.signinIntent) {
    case UserSigninIntentUpgrade: {
      // Avoid presenting the promo if the current device orientation is not
      // supported. The promo will be presented at a later moment, when the
      // device orientation is supported.
      UIInterfaceOrientation orientation = GetInterfaceOrientation();
      NSUInteger supportedOrientationsMask =
          [self.viewController supportedInterfaceOrientations];
      if (!((1 << orientation) & supportedOrientationsMask)) {
        SigninCompletionInfo* completionInfo = [SigninCompletionInfo
            signinCompletionInfoWithIdentity:self.unifiedConsentCoordinator
                                                 .selectedIdentity];
        [self runCompletionCallbackWithSigninResult:
                  SigninCoordinatorResultInterrupted
                                     completionInfo:completionInfo];
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

// Presents `self.viewController`.
- (void)presentUserViewControllerToBaseViewController {
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

// Interrupts the sign-in when `self.viewController` is presented, by dismissing
// it if needed (according to `action`). Then `completion` is called.
// This method should not be called if `self.addAccountSigninCoordinator` has
// not been stopped before. `signinCompletionInfo` is used for the signin
// callback.
- (void)interruptUserSigninUIWithAction:(SigninCoordinatorInterruptAction)action
                   signinCompletionInfo:
                       (SigninCompletionInfo*)signinCompletinInfo
                             completion:(ProceduralBlock)completion {
  if (self.viewControllerPresentingAnimation) {
    // UIKit doesn't allow a view controller to be dismissed during the
    // animation. The interruption has to be processed when the view controller
    // will be fully presented.
    // See crbug.com/1126170
    DCHECK(!self.interruptCallback);
    __weak __typeof(self) weakSelf = self;
    self.interruptCallback = ^() {
      [weakSelf interruptUserSigninUIWithAction:action
                           signinCompletionInfo:signinCompletinInfo
                                     completion:completion];
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
                           completionInfo:signinCompletinInfo];
    if (completion) {
      completion();
    }
  };
  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss: {
      [self.mediator
          cancelAndDismissAuthenticationFlowAnimated:NO
                                          completion:runCompletionCallback];
      break;
    }
    case SigninCoordinatorInterruptActionDismissWithAnimation: {
      ProceduralBlock dismissViewController = ^() {
        [weakSelf.viewController.presentingViewController
            dismissViewControllerAnimated:YES
                               completion:runCompletionCallback];
      };
      [self.mediator
          cancelAndDismissAuthenticationFlowAnimated:YES
                                          completion:dismissViewController];
      break;
    }
    case SigninCoordinatorInterruptActionDismissWithoutAnimation: {
      ProceduralBlock dismissViewController = ^() {
        [weakSelf.viewController.presentingViewController
            dismissViewControllerAnimated:NO
                               completion:runCompletionCallback];
      };
      [self.mediator
          cancelAndDismissAuthenticationFlowAnimated:NO
                                          completion:dismissViewController];
      break;
    }
  }
}

// Triggers the sign-in workflow.
- (void)startSigninFlow {
  DCHECK(self.unifiedConsentCoordinator);
  DCHECK(self.unifiedConsentCoordinator.selectedIdentity);
  PostSignInAction postSignInAction =
      self.userSigninMediatorGetSettingsLinkWasTapped
          ? POST_SIGNIN_ACTION_NONE
          : POST_SIGNIN_ACTION_COMMIT_SYNC;
  AuthenticationFlow* authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:self.browser
                      identity:self.unifiedConsentCoordinator.selectedIdentity
              postSignInAction:postSignInAction
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

// Triggers `self.signinCompletion` by calling
// `runCompletionCallbackWithSigninResult:completionInfo:` when
// `self.advancedSettingsSigninCoordinator` is done.
- (void)advancedSettingsSigninCoordinatorFinishedWithResult:
            (SigninCoordinatorResult)signinResult
                                                   identity:(id<SystemIdentity>)
                                                                identity {
  DCHECK(self.advancedSettingsSigninCoordinator);
  [self.advancedSettingsSigninCoordinator stop];
  self.advancedSettingsSigninCoordinator = nil;
  [self.unifiedConsentCoordinator resetSettingLinkTapped];
  DCHECK(!self.managedLearnMoreLinkWasTapped);
  self.unifiedConsentCoordinator.uiDisabled = NO;
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
    self.unifiedConsentCoordinator.selectedIdentity =
        signinCompletionInfo.identity;
    self.addedAccount = YES;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // The view should be dismissible only if there is no sign-in in progress.
  // See `presentationControllerShouldDismiss:`.
  DCHECK(!self.mediator.isAuthenticationInProgress);
  [self cancelSignin];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  // Don't dismiss the view controller while the sign-in is in progress.
  // To support this, the sign-in flow needs to be canceled after the view
  // controller is dimissed, and the UI needs to be blocked until the
  // sign-in flow is fully cancelled.
  return !self.mediator.isAuthenticationInProgress;
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString stringWithFormat:@"<%@: %p, addAccountSigninCoordinator: "
                                    @"%p, advancedSettingsSigninCoordinator: "
                                    @"%p, signinIntent: %lu, accessPoint %d>",
                                    self.class.description, self,
                                    self.addAccountSigninCoordinator,
                                    self.advancedSettingsSigninCoordinator,
                                    self.signinIntent, self.logger.accessPoint];
}

#pragma mark - Methods for unittests

// Returns a UserSigninViewController instance. This method is overriden for
// unittests.
- (UserSigninViewController*)
    generateUserSigninViewControllerWithUnifiedConsentViewController:
        (UIViewController*)viewController {
  return [[UserSigninViewController alloc]
      initWithEmbeddedViewController:viewController];
}

@end
