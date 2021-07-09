// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_coordinator.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/account_consistency_method.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_navigation_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_presentation_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_slide_transition_animator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Metrics to record the number of times the web sign-in is displayed.
const char* kSigninAccountConsistencyPromoActionShownCount =
    "Signin.AccountConsistencyPromoAction.Shown.Count";
// Metrics to record how many times the web sign-in has been displayed before
// the user signs in.
const char* kSigninAccountConsistencyPromoActionSignedInCount =
    "Signin.AccountConsistencyPromoAction.SignedIn.Count";

}  // namespace

@interface ConsistencyPromoSigninCoordinator () <
    ConsistencyAccountChooserCoordinatorDelegate,
    ConsistencyDefaultAccountCoordinatorDelegate,
    IdentityManagerObserverBridgeDelegate,
    UINavigationControllerDelegate,
    UIViewControllerTransitioningDelegate>

// List of gaia IDs added by the user with the consistency view.
// This set is used for metrics reasons.
@property(nonatomic, strong) NSMutableSet* addedGaiaIDs;
// Navigation controller for the consistency promo.
@property(nonatomic, strong)
    ConsistencySheetNavigationController* navigationController;
// Interaction transition to swipe from left to right to pop a view controller
// from |self.navigationController|.
@property(nonatomic, strong)
    UIPercentDrivenInteractiveTransition* interactionTransition;
// Coordinator for the first screen.
@property(nonatomic, strong)
    ConsistencyDefaultAccountCoordinator* defaultAccountCoordinator;
// Coordinator to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// Chrome interface to the iOS shared authentication library.
@property(nonatomic, assign) AuthenticationService* authenticationService;
// Manager for user's Google identities.
@property(nonatomic, assign) signin::IdentityManager* identityManager;
// Coordinator to select another identity.
@property(nonatomic, strong)
    ConsistencyAccountChooserCoordinator* accountChooserCoordinator;
// |self.defaultAccountCoordinator.selectedIdentity|.
@property(nonatomic, strong, readonly) ChromeIdentity* selectedIdentity;
// Coordinator to add an account to the device.
@property(nonatomic, strong) SigninCoordinator* addAccountCoordinator;

@end

@implementation ConsistencyPromoSigninCoordinator {
  // Observer for changes to the user's Google identities.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
}

// Returns the number of times the web sign-in has been displayed.
+ (int)displayCountWithPrefService:(PrefService*)prefService {
  return prefService->GetInteger(prefs::kSigninBottomSheetShownCount);
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
  self.addedGaiaIDs = [[NSMutableSet alloc] init];
  self.defaultAccountCoordinator = [[ConsistencyDefaultAccountCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser];
  self.defaultAccountCoordinator.delegate = self;
  [self.defaultAccountCoordinator start];

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  self.identityManager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  _identityManagerObserverBridge.reset(
      new signin::IdentityManagerObserverBridge(self.identityManager, self));

  self.navigationController = [[ConsistencySheetNavigationController alloc]
      initWithRootViewController:self.defaultAccountCoordinator.viewController];
  self.navigationController.delegate = self;
  UIScreenEdgePanGestureRecognizer* edgeSwipeGesture =
      [[UIScreenEdgePanGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(swipeAction:)];
  edgeSwipeGesture.edges = UIRectEdgeLeft;
  [self.navigationController.view addGestureRecognizer:edgeSwipeGesture];
  self.navigationController.modalPresentationStyle = UIModalPresentationCustom;
  self.navigationController.transitioningDelegate = self;
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
  RecordConsistencyPromoUserAction(
      signin_metrics::AccountConsistencyPromoAction::SHOWN);
  PrefService* prefService = browserState->GetPrefs();
  int displayCount = [self.class displayCountWithPrefService:prefService] + 1;
  prefService->SetInteger(prefs::kSigninBottomSheetShownCount, displayCount);
  base::UmaHistogramExactLinear(kSigninAccountConsistencyPromoActionShownCount,
                                displayCount, 100);
}

- (void)stop {
  [super stop];
  [self.defaultAccountCoordinator stop];
  self.defaultAccountCoordinator = nil;
}

#pragma mark - Properties

- (ChromeIdentity*)selectedIdentity {
  return self.defaultAccountCoordinator.selectedIdentity;
}

#pragma mark - Private

// Dismisses the consistency sheet view controller.
- (void)dismissNavigationViewController {
  __weak __typeof(self) weakSelf = self;
  [self.navigationController
      dismissViewControllerAnimated:YES
                         completion:^() {
                           [weakSelf finishedWithResult:
                                         SigninCoordinatorResultCanceledByUser
                                               identity:nil];
                         }];
  _identityManagerObserverBridge.reset();
}

// Calls the sign-in completion block.
- (void)finishedWithResult:(SigninCoordinatorResult)signinResult
                  identity:(ChromeIdentity*)identity {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      PrefService* prefService = self.browser->GetBrowserState()->GetPrefs();
      int displayCount = [self.class displayCountWithPrefService:prefService];
      base::UmaHistogramExactLinear(
          kSigninAccountConsistencyPromoActionSignedInCount, displayCount, 100);
      break;
    }
    case SigninCoordinatorResultCanceledByUser:
    case SigninCoordinatorResultInterrupted:
      break;
  }
  DCHECK(!self.alertCoordinator);
  [self.defaultAccountCoordinator stop];
  self.defaultAccountCoordinator = nil;
  [self.accountChooserCoordinator stop];
  self.accountChooserCoordinator = nil;
  self.navigationController = nil;
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:identity];
  [self recordHistogramWithResult:signinResult identity:identity];
  [self runCompletionCallbackWithSigninResult:signinResult
                               completionInfo:completionInfo];
}

// Records histogram at the end of the consistency promo.
- (void)recordHistogramWithResult:(SigninCoordinatorResult)signinResult
                         identity:(ChromeIdentity*)identity {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      DCHECK(identity);
      ChromeAccountManagerService* accountManagerService =
          ChromeAccountManagerServiceFactory::GetForBrowserState(
              self.browser->GetBrowserState());
      ChromeIdentity* defaultIdentity =
          accountManagerService->GetDefaultIdentity();
      DCHECK(defaultIdentity);
      if ([self.addedGaiaIDs containsObject:identity.gaiaID]) {
        // Added identity.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_ADDED_ACCOUNT);
      } else if ([defaultIdentity isEqual:identity]) {
        // Default identity.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_DEFAULT_ACCOUNT);
      } else {
        // Other identity.
        RecordConsistencyPromoUserAction(
            signin_metrics::AccountConsistencyPromoAction::
                SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT);
      }
      break;
    }
    case SigninCoordinatorResultCanceledByUser: {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::DISMISSED_BUTTON);
      break;
    }
    case SigninCoordinatorResultInterrupted: {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::DISMISSED_OTHER);
      break;
    }
  }
}

// Displays the error panel.
- (void)displayGenericCookieErrorWithError:
    (const GoogleServiceAuthError&)error {
  DCHECK(!self.alertCoordinator);
  [self.defaultAccountCoordinator stopSigninSpinner];
  if (error.state() == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::AUTH_ERROR_SHOWN);
  } else {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::GENERIC_ERROR_SHOWN);
  }
  NSString* errorMessage = l10n_util::GetNSString(IDS_IOS_SIGN_IN_AUTH_FAILURE);
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                           title:l10n_util::GetNSString(
                                     IDS_IOS_SIGN_IN_FAILURE_TITLE)
                         message:errorMessage];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SIGN_IN_DISMISS)
                action:nil
                 style:UIAlertActionStyleCancel];
  [self.alertCoordinator start];
}

// Finishes the interrupt process. This method needs to be called once all
// other dialogs on top of ConsistencyPromoSigninCoordinator are properly
// dismissed.
- (void)finalizeInterruptWithAction:(SigninCoordinatorInterruptAction)action
                         completion:(ProceduralBlock)interruptCompletion {
  DCHECK(!self.alertCoordinator);
  DCHECK(!self.addAccountCoordinator);
  __weak ConsistencyPromoSigninCoordinator* weakSelf = self;
  ProceduralBlock finishCompletionBlock = ^() {
    [weakSelf finishedWithResult:SigninCoordinatorResultInterrupted
                        identity:nil];
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
      [self.navigationController
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
    [self.addedGaiaIDs addObject:completionInfo.identity.gaiaID];
  }
  RecordConsistencyPromoUserAction(
      signin_metrics::AccountConsistencyPromoAction::ADD_ACCOUNT_COMPLETED);
  [self.addAccountCoordinator stop];
  self.addAccountCoordinator = nil;
}

#pragma mark - SwipeGesture

// Called when the swipe gesture is active. This method controls the sliding
// between two view controls in |self.navigationController|.
- (void)swipeAction:(UIScreenEdgePanGestureRecognizer*)gestureRecognizer {
  if (!gestureRecognizer.view) {
    self.interactionTransition = nil;
    return;
  }
  UIView* view = gestureRecognizer.view;
  CGFloat percentage =
      [gestureRecognizer translationInView:view].x / view.bounds.size.width;
  switch (gestureRecognizer.state) {
    case UIGestureRecognizerStateBegan:
      self.interactionTransition =
          [[UIPercentDrivenInteractiveTransition alloc] init];
      [self.navigationController popViewControllerAnimated:YES];
      [self.interactionTransition updateInteractiveTransition:percentage];
      break;
    case UIGestureRecognizerStateChanged:
      [self.interactionTransition updateInteractiveTransition:percentage];
      break;
    case UIGestureRecognizerStateEnded:
      if (percentage > .5 &&
          gestureRecognizer.state != UIGestureRecognizerStateCancelled) {
        [self.interactionTransition finishInteractiveTransition];
      } else {
        [self.interactionTransition cancelInteractiveTransition];
      }
      self.interactionTransition = nil;
      break;
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
      break;
  }
}

- (void)signinWithIdentity:(ChromeIdentity*)identity {
  DCHECK([self.selectedIdentity isEqual:identity]);
  [self.defaultAccountCoordinator startSigninSpinner];
  self.authenticationService->SignIn(self.selectedIdentity);
  DCHECK(self.authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin));
}

#pragma mark - ConsistencyAccountChooserCoordinatorDelegate

- (void)consistencyAccountChooserCoordinatorChromeIdentitySelected:
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
                                      accessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_WEB_SIGNIN];
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

- (void)consistencyDefaultAccountCoordinatorSkip:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  [self dismissNavigationViewController];
}

- (void)consistencyDefaultAccountCoordinatorOpenIdentityChooser:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  self.accountChooserCoordinator = [[ConsistencyAccountChooserCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser];
  self.accountChooserCoordinator.delegate = self;
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
  [self signinWithIdentity:self.selectedIdentity];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // Since sign-in UI blocks all other Chrome screens until it is dismissed
      // an account change event must come from the consistency sheet.
      // TODO(crbug.com/1081764): Update if sign-in UI becomes non-blocking.
      ChromeIdentity* signedInIdentity =
          self.authenticationService->GetPrimaryIdentity(
              signin::ConsentLevel::kSignin);
      DCHECK([signedInIdentity isEqual:self.selectedIdentity]);
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      // Sign out can be triggered from |onAccountsInCookieUpdated:error:|,
      // if there is cookie fetch error.
      return;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
  }
}

- (void)onAccountsInCookieUpdated:
            (const signin::AccountsInCookieJarInfo&)accountsInCookieJarInfo
                            error:(const GoogleServiceAuthError&)error {
  DCHECK(!self.accountChooserCoordinator);
  if (self.alertCoordinator) {
    // TODO(crbug.com/1204528): This case should not happen, but
    // |onAccountsInCookieUpdated:error:| can be called twice when there is an
    // error. Once this bug is fixed, this |if| should be replaced with
    // |DCHECK(!self.alertCoordinator)|.
    return;
  }
  __weak __typeof(self) weakSelf = self;
  if (error.state() == GoogleServiceAuthError::State::NONE &&
      self.authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin) &&
      accountsInCookieJarInfo.signed_in_accounts.size() > 0) {
    [self.defaultAccountCoordinator stopSigninSpinner];
    [self.navigationController
        dismissViewControllerAnimated:YES
                           completion:^() {
                             [weakSelf
                                 finishedWithResult:
                                     SigninCoordinatorResultSuccess
                                           identity:weakSelf.selectedIdentity];
                           }];
    return;
  }
  self.authenticationService->SignOut(signin_metrics::ABORT_SIGNIN, false, ^() {
    [weakSelf displayGenericCookieErrorWithError:error];
  });
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
  return self.interactionTransition;
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

@end
