// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/consistency_promo_signin/coordinator/consistency_promo_signin_coordinator.h"

#import <memory>
#import <optional>

#import "base/check_deref.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/browser/web_signin_tracker.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/consistency_promo_signin/coordinator/consistency_promo_signin_mediator.h"
#import "ios/chrome/browser/authentication/consistency_promo_signin/ui/consistency_layout_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_navigation_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_presentation_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_slide_transition_animator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/reauth/signin_reauth_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface ConsistencyPromoSigninCoordinator () <
    ConsistencyAccountChooserCoordinatorDelegate,
    ConsistencyDefaultAccountCoordinatorDelegate,
    ConsistencyPromoSigninMediatorDelegate,
    ConsistencyLayoutDelegate,
    SigninReauthCoordinatorDelegate,
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
// Coordinator to show a reauth screen.
@property(nonatomic, strong) SigninReauthCoordinator* reauthCoordinator;

@property(nonatomic, strong)
    ConsistencyPromoSigninMediator* consistencyPromoSigninMediator;

@end

@implementation ConsistencyPromoSigninCoordinator {
  ChangeProfileContinuationProvider _continuationProvider;
  // Block to execute before a change in profile.
  ProceduralBlock _prepareChangeProfile;
}

#pragma mark - Public

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                  contextStyle:(SigninContextStyle)contextStyle
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
          prepareChangeProfile:(ProceduralBlock)prepareChangeProfile
          continuationProvider:
              (const ChangeProfileContinuationProvider&)continuationProvider {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                              contextStyle:contextStyle
                               accessPoint:accessPoint];
  if (self) {
    _continuationProvider = continuationProvider;
    _prepareChangeProfile = prepareChangeProfile;
  }
  return self;
}

+ (instancetype)
    coordinatorWithBaseViewController:(UIViewController*)viewController
                              browser:(Browser*)browser
                         contextStyle:(SigninContextStyle)contextStyle
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
                 prepareChangeProfile:(ProceduralBlock)prepareChangeProfile
                 continuationProvider:(const ChangeProfileContinuationProvider&)
                                          continuationProvider {
  ProfileIOS* profile = browser->GetProfile();
  if (accessPoint == signin_metrics::AccessPoint::kWebSignin) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(profile);
    ChromeAccountManagerService* accountManagerService =
        ChromeAccountManagerServiceFactory::GetForProfile(profile);
    bool hasIdentities = [signin::GetIdentitiesOnDevice(
                             identityManager, accountManagerService) count] > 0;
    if (!hasIdentities) {
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::SUPPRESSED_NO_ACCOUNTS,
          accessPoint);
      return nil;
    }
  }
  return [[ConsistencyPromoSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
            prepareChangeProfile:prepareChangeProfile
            continuationProvider:continuationProvider];
}

- (void)dealloc {
  CHECK(!self.navigationController, base::NotFatalUntil::M142);
  CHECK(!self.defaultAccountCoordinator, base::NotFatalUntil::M142);
  CHECK(!self.alertCoordinator, base::NotFatalUntil::M142);
  CHECK(!self.accountChooserCoordinator, base::NotFatalUntil::M142);
  CHECK(!self.addAccountCoordinator, base::NotFatalUntil::M142);
  CHECK(!self.reauthCoordinator, base::NotFatalUntil::M142);
  CHECK(!self.consistencyPromoSigninMediator, base::NotFatalUntil::M142);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  signin_metrics::LogSignInStarted(self.accessPoint);
  base::RecordAction(base::UserMetricsAction("Signin_BottomSheet_Opened"));
  // Create ConsistencyPromoSigninMediator.
  ProfileIOS* profile = self.profile;
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  // The sign-in bottom sheet should not be opened if the user is already signed
  // in.
  CHECK(!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin),
        base::NotFatalUntil::M142);
  AccountReconcilor* accountReconcilor =
      ios::AccountReconcilorFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  self.consistencyPromoSigninMediator = [[ConsistencyPromoSigninMediator alloc]
      initWithAccountManagerService:accountManagerService
              authenticationService:authenticationService
                    identityManager:identityManager
                  accountReconcilor:accountReconcilor
                    userPrefService:profile->GetPrefs()
                        accessPoint:self.accessPoint];
  self.consistencyPromoSigninMediator.delegate = self;
  // Create ConsistencySheetNavigationController so it can be giving as the base
  // view controller for the account coordinator.
  self.navigationController =
      [[ConsistencySheetNavigationController alloc] initWithNibName:nil
                                                             bundle:nil];
  self.navigationController.delegate = self;
  self.navigationController.modalPresentationStyle = UIModalPresentationCustom;
  self.navigationController.transitioningDelegate = self;
  // Create ConsistencyDefaultAccountCoordinator.
  self.defaultAccountCoordinator = [[ConsistencyDefaultAccountCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                    contextStyle:self.contextStyle
                     accessPoint:self.accessPoint];
  self.defaultAccountCoordinator.delegate = self;
  self.defaultAccountCoordinator.layoutDelegate = self;
  [self.defaultAccountCoordinator start];
  self.navigationController.viewControllers =
      @[ self.defaultAccountCoordinator.viewController ];
  // Present the view.
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - SigninCoordinator

- (void)runCompletionWithSigninResult:(SigninCoordinatorResult)signinResult
                   completionIdentity:(id<SystemIdentity>)completionIdentity {
  switch (signinResult) {
    case SigninCoordinatorResultCanceledByUser:
      base::RecordAction(
          base::UserMetricsAction("Signin_BottomSheet_ClosedByCancel"));
      break;
    case SigninCoordinatorResultSuccess:
      base::RecordAction(
          base::UserMetricsAction("Signin_BottomSheet_ClosedBySignIn"));
      break;
    case SigninCoordinatorProfileSwitch:
      base::RecordAction(
          base::UserMetricsAction("Signin_BottomSheet_ClosedByProfileChange"));
      break;
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted:
      base::RecordAction(
          base::UserMetricsAction("Signin_BottomSheet_ClosedByInterrupt"));
      break;
    case SigninCoordinatorUINotAvailable:
      // ConsistencyPromoSigninCoordinator presents its child coordinators
      // directly and does not use `ShowSigninCommand`.
      NOTREACHED();
  }
  DCHECK(!self.alertCoordinator);
  [self disconnectMediatorWithResult:signinResult];
  [super runCompletionWithSigninResult:signinResult
                    completionIdentity:completionIdentity];
}

#pragma mark - BuggyAuthenticationViewOwner

- (BOOL)viewWillPersist {
  // The navigation controller is always presented.
  return YES;
}

#pragma mark - AnimatedCoordinator

- (void)stopAnimated:(BOOL)animated {
  [self stopAlertCoordinator];
  [self stopAddAccountCoordinatorAnimated:animated];
  if (self.navigationController) {
    // If `self` requested to be stopped, the navigation controller would
    // already be dismissed.
    base::RecordAction(
        base::UserMetricsAction("Signin_BottomSheet_ClosedByInterrupt"));
  }
  [self dismissViewControllerAnimated:animated];
  [self stopDefaultAccountCoordinator];
  // If the mediator was already disconnected, this second disconnect does
  // nothing.
  [self disconnectMediatorWithResult:SigninCoordinatorResultInterrupted];
  [self stopAccountChooserCoordinator];
  [self stopReauthCoordinator];
  [super stopAnimated:animated];
}

#pragma mark - Properties

- (id<SystemIdentity>)selectedIdentity {
  return self.defaultAccountCoordinator.selectedIdentity;
}

#pragma mark - Private

- (void)dismissViewControllerAnimated:(BOOL)animated {
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:nil];
  self.navigationController.delegate = nil;
  self.navigationController.transitioningDelegate = nil;
  self.navigationController = nil;
}

- (void)disconnectMediatorWithResult:(SigninCoordinatorResult)signinResult {
  self.consistencyPromoSigninMediator.delegate = nil;
  [self.consistencyPromoSigninMediator disconnectWithResult:signinResult];
  self.consistencyPromoSigninMediator = nil;
}

- (void)startReauthFlowWithIdentity:(id<SystemIdentity>)identity {
  // TODO(crbug.com/391342053): Add logging.
  CoreAccountInfo account;
  account.gaia = identity.gaiaId;
  account.email = base::SysNSStringToUTF8(identity.userEmail);
  if (self.reauthCoordinator.viewWillPersist) {
    // In case of double tap, let the first reauth proceed.
    return;
  }
  [self.reauthCoordinator stop];
  self.reauthCoordinator = [[SigninReauthCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                         account:account
               signinAccessPoint:self.accessPoint];
  self.reauthCoordinator.delegate = self;
  [self.reauthCoordinator start];
}

- (void)stopAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

- (void)stopAccountChooserCoordinator {
  self.accountChooserCoordinator.delegate = nil;
  self.accountChooserCoordinator.layoutDelegate = nil;
  [self.accountChooserCoordinator stop];
  self.accountChooserCoordinator = nil;
}

- (void)stopDefaultAccountCoordinator {
  self.defaultAccountCoordinator.delegate = nil;
  self.defaultAccountCoordinator.layoutDelegate = nil;
  [self.defaultAccountCoordinator stop];
  self.defaultAccountCoordinator = nil;
}

- (void)stopAddAccountCoordinatorAnimated:(BOOL)animated {
  [self.addAccountCoordinator stopAnimated:animated];
  self.addAccountCoordinator = nil;
}

- (void)stopReauthCoordinator {
  self.reauthCoordinator.delegate = nil;
  [self.reauthCoordinator stop];
  self.reauthCoordinator = nil;
}
// Does cleanup (metrics and remove coordinator) once the add-account flow is
// finished. If `hasAccounts == NO` and `signinResult` is successful , the
// function immediately signs in to Chrome with the identity acquired from the
// add-account flow after the cleanup.
- (void)
    addAccountCompletionWithCoordinator:(SigninCoordinator*)coordinator
                           SigninResult:(SigninCoordinatorResult)signinResult
                     completionIdentity:(id<SystemIdentity>)completionIdentity
                            hasAccounts:(BOOL)hasAccounts {
  CHECK_EQ(self.addAccountCoordinator, coordinator, base::NotFatalUntil::M151);
  if (hasAccounts) {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::ADD_ACCOUNT_COMPLETED,
        self.accessPoint);
  } else {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::
            ADD_ACCOUNT_COMPLETED_WITH_NO_DEVICE_ACCOUNT,
        self.accessPoint);
  }

  [self stopAddAccountCoordinatorAnimated:NO];

  if (signinResult != SigninCoordinatorResultSuccess) {
    return;
  }
  DCHECK(completionIdentity);
  [self.consistencyPromoSigninMediator systemIdentityAdded:completionIdentity];
  self.defaultAccountCoordinator.selectedIdentity = completionIdentity;

  if (hasAccounts) {
    [self.navigationController popViewControllerAnimated:YES];
    [self stopAccountChooserCoordinator];
    return;
  }
  [self startSignIn];
}

// Opens an AddAccountSigninCoordinator to add an account to the device.
// If `hasAccounts == NO`, the added account will be used to sign in to Chrome
// directly after the AddAccountSigninCoordinator finishes.
- (void)openAddAccountCoordinatorWithHasAccounts:(BOOL)hasAccounts {
  // In case of double-tap, we must stop the already started coordinator. This
  // may occur because, up to iOS 18, the view may have disappeared without
  // calling the signin completion. See crbug.com/395959814
  [self.addAccountCoordinator stop];
  if (hasAccounts) {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::ADD_ACCOUNT_STARTED,
        self.accessPoint);
  } else {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::
            ADD_ACCOUNT_STARTED_WITH_NO_DEVICE_ACCOUNT,
        self.accessPoint);
  }
  self.addAccountCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.navigationController
                                          browser:self.browser
                                     contextStyle:self.contextStyle
                                      accessPoint:self.accessPoint
                                   prefilledEmail:nil
                             continuationProvider:_continuationProvider];
  __weak ConsistencyPromoSigninCoordinator* weakSelf = self;
  self.addAccountCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult signinResult,
        id<SystemIdentity> signinCompletionIdentity) {
        [weakSelf addAccountCompletionWithCoordinator:coordinator
                                         SigninResult:signinResult
                                   completionIdentity:signinCompletionIdentity
                                          hasAccounts:hasAccounts];
      };
  [self.addAccountCoordinator start];
}

// Starts the sign-in flow.
- (void)startSignIn {
  AuthenticationFlow* authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:self.browser
                      identity:self.selectedIdentity
                   accessPoint:self.accessPoint
          precedingHistorySync:YES
             postSignInActions:
                 {PostSignInAction::kShowIdentityConfirmationSnackbar}
      presentingViewController:self.navigationController
                    anchorView:nil
                    anchorRect:CGRectNull];
  [self.consistencyPromoSigninMediator
      signinWithAuthenticationFlow:authenticationFlow];
}

#pragma mark - ConsistencyAccountChooserCoordinatorDelegate

- (void)consistencyAccountChooserCoordinatorIdentitySelected:
    (ConsistencyAccountChooserCoordinator*)coordinator {
  self.defaultAccountCoordinator.selectedIdentity =
      self.accountChooserCoordinator.selectedIdentity;
  [self stopAccountChooserCoordinator];
  [self.navigationController popViewControllerAnimated:YES];
}

- (void)consistencyAccountChooserCoordinatorOpenAddAccount:
    (ConsistencyAccountChooserCoordinator*)coordinator {
  [self openAddAccountCoordinatorWithHasAccounts:YES];
}

- (void)consistencyAccountChooserCoordinatorWantsToBeStopped:
    (ConsistencyAccountChooserCoordinator*)coordinator {
  CHECK_EQ(coordinator, self.accountChooserCoordinator,
           base::NotFatalUntil::M140);
  [self stopAccountChooserCoordinator];
  [self.navigationController popViewControllerAnimated:YES];
}

#pragma mark - ConsistencyDefaultAccountCoordinatorDelegate

- (void)consistencyDefaultAccountCoordinatorSkip:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  CHECK(!self.alertCoordinator, base::NotFatalUntil::M142)
      << base::SysNSStringToUTF8([self description]);
  PrefService* userPrefService = self.profile->GetPrefs();
  if (self.accessPoint == signin_metrics::AccessPoint::kWebSignin) {
    const int skipCounter =
        userPrefService->GetInteger(prefs::kSigninWebSignDismissalCount) + 1;
    userPrefService->SetInteger(prefs::kSigninWebSignDismissalCount,
                                skipCounter);
  }
  [self dismissViewControllerAnimated:YES];
  [self runCompletionWithSigninResult:SigninCoordinatorResultCanceledByUser
                   completionIdentity:nil];
}

- (void)consistencyDefaultAccountCoordinatorOpenIdentityChooser:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  if (self.accountChooserCoordinator) {
    // This can occur if the user double tap on the button.
    return;
  }
  self.accountChooserCoordinator = [[ConsistencyAccountChooserCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                selectedIdentity:self.defaultAccountCoordinator
                                     .selectedIdentity];
  self.accountChooserCoordinator.delegate = self;
  self.accountChooserCoordinator.layoutDelegate = self;
  [self.accountChooserCoordinator start];
  [self.navigationController
      pushViewController:self.accountChooserCoordinator.viewController
                animated:YES];
}

- (void)consistencyDefaultAccountCoordinatorSignin:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  DCHECK_EQ(coordinator, self.defaultAccountCoordinator);
  if (base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError) &&
      !self.selectedIdentity.hasValidAuth) {
    [self startReauthFlowWithIdentity:self.selectedIdentity];
    return;
  }
  [self startSignIn];
}

- (void)consistencyDefaultAccountCoordinatorOpenAddAccount:
    (ConsistencyDefaultAccountCoordinator*)coordinator {
  [self openAddAccountCoordinatorWithHasAccounts:NO];
}

#pragma mark - ConsistencyLayoutDelegate

- (ConsistencySheetDisplayStyle)displayStyle {
  return self.navigationController.displayStyle;
}

#pragma mark - SigninReauthCoordinatorDelegate

- (void)reauthFinishedWithResult:(ReauthResult)result
                          gaiaID:(const GaiaId*)gaiaID {
  [self stopReauthCoordinator];
  if (result == ReauthResult::kSuccess) {
    ChromeAccountManagerService* accountManagerService =
        ChromeAccountManagerServiceFactory::GetForProfile(self.profile);
    BOOL identityValid =
        accountManagerService->IsValidIdentity(self.selectedIdentity);
    BOOL identityEqual =
        self.defaultAccountCoordinator.selectedIdentity.gaiaId == *gaiaID;
    if (identityValid && identityEqual && result == ReauthResult::kSuccess) {
      [self startSignIn];
    }
  }
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
    base::RecordAction(base::UserMetricsAction(
        "Signin_BottomSheet_IdentityChooser_ClosedByUser"));
    [self stopAccountChooserCoordinator];
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
  id<SystemIdentity> completionIdentity = identity;
  [self dismissViewControllerAnimated:YES];
  [self runCompletionWithSigninResult:SigninCoordinatorResultSuccess
                   completionIdentity:completionIdentity];
}

- (void)consistencyPromoSigninMediatorSignInIsImpossible:
    (ConsistencyPromoSigninMediator*)mediator {
  CHECK_EQ(self.consistencyPromoSigninMediator, mediator,
           base::NotFatalUntil::M143);
  [self dismissViewControllerAnimated:YES];
  [self runCompletionWithSigninResult:SigninCoordinatorResultInterrupted
                   completionIdentity:nil];
}

- (void)consistencyPromoSigninMediatorSignInCancelled:
    (ConsistencyPromoSigninMediator*)mediator {
  [self.defaultAccountCoordinator stopSigninSpinner];
}

- (void)consistencyPromoSigninMediator:(ConsistencyPromoSigninMediator*)mediator
                        errorDidHappen:
                            (ConsistencyPromoSigninMediatorError)error
                          withIdentity:(id<SystemIdentity>)identity {
  CHECK([identity isEqual:self.selectedIdentity]);
  [self.defaultAccountCoordinator stopSigninSpinner];

  NSString* errorMessage = nil;
  switch (error) {
    case ConsistencyPromoSigninMediatorErrorTimeout:
      errorMessage =
          l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_TIMEOUT_ERROR);
      break;
    case ConsistencyPromoSigninMediatorErrorGeneric:
      errorMessage =
          l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_GENERIC_ERROR);
      break;
    case ConsistencyPromoSigninMediatorErrorAuth:
      [self startReauthFlowWithIdentity:identity];
      return;
  }
  DCHECK(!self.alertCoordinator);
  NSString* errorTitle = l10n_util::GetNSString(IDS_IOS_WEBSIGN_ERROR_TITLE);
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                           title:errorTitle
                         message:errorMessage];

  __weak __typeof(self) weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SIGN_IN_DISMISS)
                action:^() {
                  [weakSelf stopAlertCoordinator];
                }
                 style:UIAlertActionStyleCancel];

  [self.alertCoordinator start];
}

- (std::unique_ptr<signin::WebSigninTracker>)
    trackWebSigninWithIdentityManager:(signin::IdentityManager*)identityManager
                    accountReconcilor:(AccountReconcilor*)accountReconcilor
                        signinAccount:(const CoreAccountId&)signin_account
                         withCallback:
                             (const base::RepeatingCallback<void(
                                  signin::WebSigninTracker::Result)>*)callback
                          withTimeout:
                              (const std::optional<base::TimeDelta>&)timeout {
  return std::make_unique<signin::WebSigninTracker>(
      identityManager, accountReconcilor, signin_account, CHECK_DEREF(callback),
      timeout);
}

- (ChangeProfileContinuation)changeProfileContinuation {
  if (_prepareChangeProfile) {
    _prepareChangeProfile();
  };
  // TODO(crbug.com/375605572): Store the provider in the mediator.
  // This currently can’t be done, because OCMock raise exception when a mocked
  // method gets a parameter whose type is a once or repeating callback.
  return _continuationProvider.Run();
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, defaultAccountCoordinator: %p, alertCoordinator: %p, "
          @"accountChooserCoordinator %p, addAccountCoordinator %p, presented: "
          @"%@, base viewcontroller: %@ %@>",
          self.class.description, self, self.defaultAccountCoordinator,
          self.alertCoordinator, self.accountChooserCoordinator,
          self.addAccountCoordinator,
          ViewControllerPresentationStatusDescription(
              self.navigationController),
          NSStringFromClass(self.baseViewController.class),
          ViewControllerPresentationStatusDescription(self.baseViewController)];
}

@end
