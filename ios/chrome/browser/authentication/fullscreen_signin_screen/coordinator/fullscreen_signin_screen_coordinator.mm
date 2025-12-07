// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_mediator.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_mediator_delegate.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/ui/fullscreen_signin_screen_consumer.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/ui/fullscreen_signin_screen_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/uma/uma_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

@interface FullscreenSigninScreenCoordinator () <
    FullscreenSigninScreenMediatorDelegate,
    FullscreenSigninScreenViewControllerDelegate,
    IdentityChooserCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate,
    TOSCoordinatorDelegate,
    UMACoordinatorDelegate>

// First run screen delegate.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;
// Sign-in screen view controller.
@property(nonatomic, strong)
    FullscreenSigninScreenViewController* viewController;
// Sign-in screen mediator.
@property(nonatomic, strong) FullscreenSigninScreenMediator* mediator;
// Account manager service.
@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// Authentication service.
@property(nonatomic, assign) AuthenticationService* authenticationService;
// Coordinator used to manage the TOS page.
@property(nonatomic, strong) TOSCoordinator* TOSCoordinator;
// Coordinator to show the metric reportingn dialog.
@property(nonatomic, strong) UMACoordinator* UMACoordinator;
// Coordinator to choose an identity.
@property(nonatomic, strong)
    IdentityChooserCoordinator* identityChooserCoordinator;
// Coordinator to add an identity.
@property(nonatomic, strong) SigninCoordinator* addAccountSigninCoordinator;
@property(nonatomic, assign) BOOL UMAReportingUserChoice;

@end

@implementation FullscreenSigninScreenCoordinator {
  SigninContextStyle _contextStyle;
  signin_metrics::AccessPoint _accessPoint;
  signin_metrics::PromoAction _promoAction;
  ChangeProfileContinuationProvider _changeProfileContinuationProvider;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
     initWithBaseNavigationController:
         (UINavigationController*)navigationController
                              browser:(Browser*)browser
                             delegate:(id<FirstRunScreenDelegate>)delegate
                         contextStyle:(SigninContextStyle)contextStyle
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
                          promoAction:(signin_metrics::PromoAction)promoAction
    changeProfileContinuationProvider:(const ChangeProfileContinuationProvider&)
                                          changeProfileContinuationProvider {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    CHECK_EQ(browser->type(), Browser::Type::kRegular,
             base::NotFatalUntil::M145);
    CHECK(changeProfileContinuationProvider);
    _baseNavigationController = navigationController;
    _delegate = delegate;
    _UMAReportingUserChoice = kDefaultMetricsReportingCheckboxValue;
    _contextStyle = contextStyle;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
    _baseNavigationController.presentationController.delegate = self;
    _changeProfileContinuationProvider = changeProfileContinuationProvider;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.viewController = [[FullscreenSigninScreenViewController alloc]
      initWithContextStyle:_contextStyle];
  self.viewController.delegate = self;

  ProfileIOS* profile = self.profile->GetOriginalProfile();

  self.authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (self.authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // Don't show the sign-in screen since the user is already signed in.
    [_delegate screenWillFinishPresenting];
    return;
  }

  if (_accessPoint == signin_metrics::AccessPoint::kFullscreenSigninPromo) {
    base::UmaHistogramEnumeration(
        "IOS.SignInpromo.Fullscreen.PromoEvents",
        SigninFullscreenPromoEvents::kSigninUIStarted);
  }

  self.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  PrefService* localPrefService = GetApplicationContext()->GetLocalState();
  PrefService* prefService = profile->GetPrefs();
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  self.mediator = [[FullscreenSigninScreenMediator alloc]
          initWithAccountManagerService:self.accountManagerService
                  authenticationService:self.authenticationService
                        identityManager:identityManager
                       localPrefService:localPrefService
                            prefService:prefService
                            syncService:syncService
                            accessPoint:_accessPoint
                            promoAction:_promoAction
      changeProfileContinuationProvider:_changeProfileContinuationProvider];
  self.mediator.consumer = self.viewController;
  self.mediator.delegate = self;
  if (self.mediator.ignoreDismissGesture) {
    self.viewController.modalInPresentation = YES;
  }
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
}

- (void)stop {
  [self stopAddAccountCoordinator];
  [self stopIdentityChooserCoordinator];
  self.delegate = nil;
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.accountManagerService = nil;
  self.authenticationService = nil;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  CHECK(!self.mediator.ignoreDismissGesture);
  // Cancel the sign-in flow.
  __weak __typeof(self) weakSelf = self;
  [self.mediator cancelSignInScreenWithCompletion:^{
    [weakSelf finishPresentingWithSignIn:NO];
  }];
}

#pragma mark - Private

- (void)stopIdentityChooserCoordinator {
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator = nil;
}

- (void)stopAddAccountCoordinator {
  [self.addAccountSigninCoordinator stop];
  self.addAccountSigninCoordinator = nil;
}

- (void)stopUMACoordinator {
  [self.UMACoordinator stop];
  self.UMACoordinator.delegate = nil;
  self.UMACoordinator = nil;
}

// Starts the coordinator to present the Add Account module.
- (void)triggerAddAccount {
  [self.mediator userAttemptedToSignin];
  if (self.addAccountSigninCoordinator.viewWillPersist) {
    return;
  }
  [self.addAccountSigninCoordinator stop];
  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                     contextStyle:_contextStyle
                                      accessPoint:_accessPoint
                                   prefilledEmail:nil
                             continuationProvider:
                                 _changeProfileContinuationProvider];
  __weak __typeof(self) weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion = ^(
      SigninCoordinator* coordinator, SigninCoordinatorResult signinResult,
      id<SystemIdentity> signinCompletionIdentity) {
    [weakSelf addAccountSigninCompleteWithCoordinator:coordinator
                                         signinResult:signinResult
                                   completionIdentity:signinCompletionIdentity];
  };
  [self.addAccountSigninCoordinator start];
}

// Callback handling the completion of the AddAccount action.
- (void)addAccountSigninCompleteWithCoordinator:(SigninCoordinator*)coordinator
                                   signinResult:
                                       (SigninCoordinatorResult)signinResult
                             completionIdentity:
                                 (id<SystemIdentity>)signinCompletionIdentity {
  CHECK_EQ(self.addAccountSigninCoordinator, coordinator,
           base::NotFatalUntil::M151);
  [self stopAddAccountCoordinator];
  if (signinResult == SigninCoordinatorResultSuccess &&
      self.accountManagerService->IsValidIdentity(signinCompletionIdentity)) {
    self.mediator.selectedIdentity = signinCompletionIdentity;
    self.mediator.addedAccount = YES;
  }
}

// Starts the sign in process.
- (void)startSignIn {
  DCHECK(self.mediator.selectedIdentity);
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:self.mediator.selectedIdentity
                                      accessPoint:_accessPoint
                             precedingHistorySync:YES
                                postSignInActions:PostSignInActionSet()
                         presentingViewController:self.viewController
                                       anchorView:nil
                                       anchorRect:CGRectNull];
  [self.mediator startSignInWithAuthenticationFlow:authenticationFlow];
}

// Calls the mediator and the delegate when the coordinator is finished.
- (void)finishPresentingWithSignIn:(BOOL)signIn {
  [self.mediator finishPresentingWithSignIn:signIn];
  [self.delegate screenWillFinishPresenting];
}

// Shows the UMA dialog so the user can manage metric reporting.
- (void)showUMADialog {
  CHECK(!self.UMACoordinator, base::NotFatalUntil::M144);
  self.UMACoordinator = [[UMACoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
               UMAReportingValue:self.mediator.UMAReportingUserChoice];
  self.UMACoordinator.delegate = self;
  [self.UMACoordinator start];
}

- (void)showTOSPage {
  CHECK(!self.TOSCoordinator, base::NotFatalUntil::M144);
  self.mediator.TOSLinkWasTapped = YES;
  self.TOSCoordinator =
      [[TOSCoordinator alloc] initWithBaseViewController:self.viewController
                                                 browser:self.browser];
  self.TOSCoordinator.delegate = self;
  [self.TOSCoordinator start];
}

#pragma mark - FullscreenSigninScreenMediatorDelegate

- (void)fullscreenSigninScreenMediatorDidFinishSignin:
    (FullscreenSigninScreenMediator*)mediator {
  CHECK_EQ(mediator, self.mediator, base::NotFatalUntil::M140);
  [self finishPresentingWithSignIn:YES];
}

- (void)fullscreenSigninScreenMediatorWantsToBeDismissed:
    (FullscreenSigninScreenMediator*)mediator {
  CHECK_EQ(mediator, self.mediator, base::NotFatalUntil::M141);
  [self finishPresentingWithSignIn:NO];
}

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  [self stopIdentityChooserCoordinator];
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

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  if (self.authenticationService->SigninEnabled()) {
    if (self.mediator.selectedIdentity) {
      [self startSignIn];
    } else {
      [self triggerAddAccount];
    }
  } else {
    [self finishPresentingWithSignIn:NO];
  }
}

- (void)didTapSecondaryActionButton {
  __weak __typeof(self) weakSelf = self;
  [self.mediator cancelSignInScreenWithCompletion:^{
    [weakSelf finishPresentingWithSignIn:NO];
  }];
}

- (void)didTapURLInDisclaimer:(NSURL*)URL {
  if ([URL.absoluteString isEqualToString:first_run::kTermsOfServiceURL]) {
    [self showTOSPage];
  } else if ([URL.absoluteString
                 isEqualToString:first_run::kMetricReportingURL]) {
    self.mediator.UMALinkWasTapped = YES;
    [self showUMADialog];
  } else {
    NOTREACHED() << std::string("Unknown URL ")
                 << base::SysNSStringToUTF8(URL.absoluteString);
  }
}

#pragma mark - FullscreenSigninScreenViewControllerDelegate

- (void)showAccountPickerFromPoint:(CGPoint)point {
  if (self.identityChooserCoordinator) {
    // This may occur if the user double tap on the identity button.
    return;
  }
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
  self.identityChooserCoordinator.selectedIdentity =
      self.mediator.selectedIdentity;
}

#pragma mark - TOSCoordinatorDelegate

- (void)TOSCoordinatorWantsToBeStopped:(TOSCoordinator*)coordinator {
  CHECK_EQ(self.TOSCoordinator, coordinator, base::NotFatalUntil::M144);
  [self.TOSCoordinator stop];
  self.TOSCoordinator.delegate = nil;
  self.TOSCoordinator = nil;
}

#pragma mark - UMACoordinatorDelegate

- (void)UMACoordinatorDidRemoveWithCoordinator:(UMACoordinator*)coordinator
                        UMAReportingUserChoice:(BOOL)UMAReportingUserChoice {
  DCHECK(self.UMACoordinator);
  DCHECK_EQ(self.UMACoordinator, coordinator);
  [self stopUMACoordinator];
  DCHECK(self.mediator);
  self.mediator.UMAReportingUserChoice = UMAReportingUserChoice;
}

@end
