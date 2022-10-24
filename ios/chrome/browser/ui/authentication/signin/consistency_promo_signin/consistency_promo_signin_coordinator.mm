// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_coordinator.h"

#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_layout_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_navigation_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_presentation_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_slide_transition_animator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencyPromoSigninCoordinator () <
    ConsistencyAccountChooserCoordinatorDelegate,
    ConsistencyDefaultAccountCoordinatorDelegate,
    ConsistencyPromoSigninMediatorDelegate,
    ConsistencyLayoutDelegate,
    UINavigationControllerDelegate,
    UIViewControllerTransitioningDelegate>

// Navigation controller for the consistency promo.
@property(nonatomic, strong)
    ConsistencySheetNavigationController* navigationController;
// Coordinator for the first screen.
@property(nonatomic, strong)
    ConsistencyDefaultAccountCoordinator* defaultAccountCoordinator;
// Coordinator to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// Coordinator to select another identity.
@property(nonatomic, strong)
    ConsistencyAccountChooserCoordinator* accountChooserCoordinator;
// `self.defaultAccountCoordinator.selectedIdentity`.
@property(nonatomic, strong, readonly) id<SystemIdentity> selectedIdentity;
// Coordinator to add an account to the device.
@property(nonatomic, strong) SigninCoordinator* addAccountCoordinator;
// The access point that triggered sign-in.
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

@property(nonatomic, strong)
    ConsistencyPromoSigninMediator* consistencyPromoSigninMediator;

@end

@implementation ConsistencyPromoSigninCoordinator

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _accessPoint = accessPoint;
  }
  return self;
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock consistencyCompletion = ^() {
    [weakSelf finalizeInterruptWithAction:action completion:completion];
  };
  if (self.addAccountCoordinator) {
    [self.addAccountCoordinator interruptWithAction:action
                                         completion:consistencyCompletion];
  } else {
    consistencyCompletion();
  }
}

- (void)start {
  [super start];
  // Create ConsistencyPromoSigninMediator.
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  self.consistencyPromoSigninMediator = [[ConsistencyPromoSigninMediator alloc]
      initWithAccountManagerService:accountManagerService
              authenticationService:authenticationService
                    identityManager:identityManager
                    userPrefService:browserState->GetPrefs()
                        accessPoint:self.accessPoint];
  self.consistencyPromoSigninMediator.delegate = self;
  // Create ConsistencyDefaultAccountCoordinator.
  self.defaultAccountCoordinator = [[ConsistencyDefaultAccountCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                     accessPoint:self.accessPoint];
  self.defaultAccountCoordinator.delegate = self;
  self.defaultAccountCoordinator.layoutDelegate = self;
  [self.defaultAccountCoordinator start];
  // Create ConsistencySheetNavigationController.
  self.navigationController = [[ConsistencySheetNavigationController alloc]
      initWithRootViewController:self.defaultAccountCoordinator.viewController];
  self.navigationController.delegate = self;
  self.navigationController.modalPresentationStyle = UIModalPresentationCustom;
  self.navigationController.transitioningDelegate = self;
  // Present the view.
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [self.defaultAccountCoordinator stop];
  self.defaultAccountCoordinator = nil;
}

#pragma mark - Properties

- (id<SystemIdentity>)selectedIdentity {
  return self.defaultAccountCoordinator.selectedIdentity;
}

#pragma mark - Private

// Finishes the interrupt process. This method needs to be called once all
// other dialogs on top of ConsistencyPromoSigninCoordinator are properly
// dismissed.
- (void)finalizeInterruptWithAction:(SigninCoordinatorInterruptAction)action
                         completion:(ProceduralBlock)interruptCompletion {
  DCHECK(!self.alertCoordinator);
  DCHECK(!self.addAccountCoordinator);
  __weak ConsistencyPromoSigninCoordinator* weakSelf = self;
  ProceduralBlock finishCompletionBlock = ^() {
    weakSelf.navigationController = nil;
    SigninCompletionInfo* completionInfo =
        [SigninCompletionInfo signinCompletionInfoWithIdentity:nil];
    [weakSelf coordinatorDoneWithResult:SigninCoordinatorResultInterrupted
                         completionInfo:completionInfo];
    if (interruptCompletion) {
      interruptCompletion();
    }
  };
  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss:
      finishCompletionBlock();
      break;
    case SigninCoordinatorInterruptActionDismissWithoutAnimation:
    case SigninCoordinatorInterruptActionDismissWithAnimation: {
      BOOL animated =
          action == SigninCoordinatorInterruptActionDismissWithAnimation;
      [self.navigationController.presentingViewController
          dismissViewControllerAnimated:animated
                             completion:finishCompletionBlock];
    }
  }
}

// Does cleanup (metrics and remove coordinator) once the add account is
// finished.
- (void)
    addAccountCompletionWithSigninResult:(SigninCoordinatorResult)signinResult
                          completionInfo:(SigninCompletionInfo*)completionInfo {
  if (signinResult == SigninCoordinatorResultSuccess) {
    DCHECK(completionInfo);
    [self.consistencyPromoSigninMediator
        systemIdentityAdded:completionInfo.identity];
  }
  RecordConsistencyPromoUserAction(
      signin_metrics::AccountConsistencyPromoAction::ADD_ACCOUNT_COMPLETED);
  [self.addAccountCoordinator stop];
  self.addAccountCoordinator = nil;
}

// Stops all the coordinators and mediator, and run the completion callback.
- (void)coordinatorDoneWithResult:(SigninCoordinatorResult)signinResult
                   completionInfo:(SigninCompletionInfo*)completionInfo {
  DCHECK(!self.alertCoordinator);
  DCHECK(!self.navigationController);
  [self.defaultAccountCoordinator stop];
  self.defaultAccountCoordinator = nil;
  [self.accountChooserCoordinator stop];
  self.accountChooserCoordinator = nil;
  [self.consistencyPromoSigninMediator disconnectWithResult:signinResult];
  self.consistencyPromoSigninMediator = nil;
  [self runCompletionCallbackWithSigninResult:signinResult
                               completionInfo:completionInfo];
}

#pragma mark - ConsistencyAccountChooserCoordinatorDelegate

- (void)consistencyAccountChooserCoordinatorIdentitySelected:
    (ConsistencyAccountChooserCoordinator*)coordinator {
  self.defaultAccountCoordinator.selectedIdentity =
      self.accountChooserCoordinator.selectedIdentity;
  [self.accountChooserCoordinator stop];
  self.accountChooserCoordinator = nil;
  [self.navigationController popViewControllerAnimated:YES];
}

- (void)consistencyAccountChooserCoordinatorOpenAddAccount:
    (ConsistencyAccountChooserCoordinator*)coordinator {
  RecordConsistencyPromoUserAction(
      signin_metrics::AccountConsistencyPromoAction::ADD_ACCOUNT_STARTED);
  DCHECK(!self.addAccountCoordinator);
  self.addAccountCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.navigationController
                                          browser:self.browser
                                      accessPoint:self.accessPoint];
  __weak ConsistencyPromoSigninCoordinator* weakSelf = self;
  self.addAccountCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf addAccountCompletionWithSigninResult:signinResult
                                        completionInfo:signinCompletionInfo];
      };
  [self.addAccountCoordinator start];
}

#pragma mark - ConsistencyDefaultAccountCoordinatorDelegate

- (void)consistencyDefaultAccountCoordinatorAllIdentityRemoved:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  [self interruptWithAction:SigninCoordinatorInterruptActionDismissWithAnimation
                 completion:nil];
}

- (void)consistencyDefaultAccountCoordinatorSkip:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  PrefService* userPrefService = browserState->GetPrefs();
  if (self.accessPoint ==
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN) {
    const int skipCounter =
        userPrefService->GetInteger(prefs::kSigninWebSignDismissalCount) + 1;
    userPrefService->SetInteger(prefs::kSigninWebSignDismissalCount,
                                skipCounter);
  }
  __weak __typeof(self) weakSelf = self;
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:nil];
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^() {
                           weakSelf.navigationController = nil;
                           [weakSelf coordinatorDoneWithResult:
                                         SigninCoordinatorResultCanceledByUser
                                                completionInfo:completionInfo];
                         }];
}

- (void)consistencyDefaultAccountCoordinatorOpenIdentityChooser:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  self.accountChooserCoordinator = [[ConsistencyAccountChooserCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser];
  self.accountChooserCoordinator.delegate = self;
  self.accountChooserCoordinator.layoutDelegate = self;
  [self.accountChooserCoordinator
      startWithSelectedIdentity:self.defaultAccountCoordinator
                                    .selectedIdentity];
  [self.navigationController
      pushViewController:self.accountChooserCoordinator.viewController
                animated:YES];
}

- (void)consistencyDefaultAccountCoordinatorSignin:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  DCHECK_EQ(coordinator, self.defaultAccountCoordinator);
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:self.selectedIdentity
                                 postSignInAction:POST_SIGNIN_ACTION_NONE
                         presentingViewController:self.navigationController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);
  [self.consistencyPromoSigninMediator
      signinWithAuthenticationFlow:authenticationFlow];
}

#pragma mark - ConsistencyLayoutDelegate

- (ConsistencySheetDisplayStyle)displayStyle {
  return self.navigationController.displayStyle;
}

#pragma mark - UINavigationControllerDelegate

- (id<UIViewControllerAnimatedTransitioning>)
               navigationController:
                   (UINavigationController*)navigationController
    animationControllerForOperation:(UINavigationControllerOperation)operation
                 fromViewController:(UIViewController*)fromVC
                   toViewController:(UIViewController*)toVC {
  DCHECK_EQ(navigationController, self.navigationController);
  switch (operation) {
    case UINavigationControllerOperationNone:
      return nil;
    case UINavigationControllerOperationPush:
      return [[ConsistencySheetSlideTransitionAnimator alloc]
             initWithAnimation:ConsistencySheetSlideAnimationPushing
          navigationController:self.navigationController];
    case UINavigationControllerOperationPop:
      return [[ConsistencySheetSlideTransitionAnimator alloc]
             initWithAnimation:ConsistencySheetSlideAnimationPopping
          navigationController:self.navigationController];
  }
  NOTREACHED();
  return nil;
}

- (id<UIViewControllerInteractiveTransitioning>)
                           navigationController:
                               (UINavigationController*)navigationController
    interactionControllerForAnimationController:
        (id<UIViewControllerAnimatedTransitioning>)animationController {
  return self.navigationController.interactionTransition;
}

- (void)navigationController:(UINavigationController*)navigationController
       didShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  DCHECK(navigationController == self.navigationController);
  DCHECK(navigationController.viewControllers.count > 0);
  DCHECK(navigationController.viewControllers[0] ==
         self.defaultAccountCoordinator.viewController);
  if (self.navigationController.viewControllers.count == 1 &&
      self.accountChooserCoordinator) {
    // AccountChooserCoordinator has been removed by "Back" button.
    [self.accountChooserCoordinator stop];
    self.accountChooserCoordinator = nil;
  }
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presentedViewController
                            presentingViewController:
                                (UIViewController*)presentingViewController
                                sourceViewController:(UIViewController*)source {
  DCHECK_EQ(self.navigationController, presentedViewController);
  return [[ConsistencySheetPresentationController alloc]
      initWithConsistencySheetNavigationController:self.navigationController
                          presentingViewController:presentingViewController];
}

#pragma mark - ConsistencyPromoSigninMediatorDelegate

- (void)consistencyPromoSigninMediatorSigninStarted:
    (ConsistencyPromoSigninMediator*)mediator {
  [self.defaultAccountCoordinator startSigninSpinner];
}

- (void)consistencyPromoSigninMediatorSignInDone:
            (ConsistencyPromoSigninMediator*)mediator
                                    withIdentity:(id<SystemIdentity>)identity {
  DCHECK([identity isEqual:self.selectedIdentity]);
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:identity];
  __weak __typeof(self) weakSelf = self;
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^() {
                           [weakSelf.defaultAccountCoordinator
                                   stopSigninSpinner];
                           weakSelf.navigationController = nil;
                           [weakSelf coordinatorDoneWithResult:
                                         SigninCoordinatorResultSuccess
                                                completionInfo:completionInfo];
                         }];
}

- (void)consistencyPromoSigninMediator:(ConsistencyPromoSigninMediator*)mediator
                        errorDidHappen:
                            (ConsistencyPromoSigninMediatorError)error {
  NSString* errorTitle = l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_TITLE);
  NSString* errorMessage = nil;
  switch (error) {
    case ConsistencyPromoSigninMediatorErrorGeneric:
    case ConsistencyPromoSigninMediatorErrorFailedToSignin:
      errorMessage =
          l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_GENERIC_ERROR);
      break;
    case ConsistencyPromoSigninMediatorErrorTimeout:
      errorMessage =
          l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_TIMEOUT_ERROR);
      break;
  }
  DCHECK(!self.alertCoordinator);
  [self.defaultAccountCoordinator stopSigninSpinner];
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                           title:errorTitle
                         message:errorMessage];

  __weak __typeof(self) weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SIGN_IN_DISMISS)
                action:^() {
                  weakSelf.alertCoordinator = nil;
                }
                 style:UIAlertActionStyleCancel];
  [self.alertCoordinator start];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, defaultAccountCoordinator: %p, alertCoordinator: %p, "
          @"accountChooserCoordinator %p, addAccountCoordinator %p>",
          self.class.description, self, self.defaultAccountCoordinator,
          self.alertCoordinator, self.accountChooserCoordinator,
          self.addAccountCoordinator];
}

@end
