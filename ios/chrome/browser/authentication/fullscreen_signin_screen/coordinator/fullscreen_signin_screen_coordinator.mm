// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_switches.h"
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
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/public/first_run_util.h"
#import "ios/chrome/browser/first_run/tos/coordinator/tos_coordinator.h"
#import "ios/chrome/browser/first_run/uma/coordinator/uma_coordinator.h"
#import "ios/chrome/browser/metrics/model/ios_profile_metrics_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
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
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

@interface FullscreenSigninScreenCoordinator () <
    ExternalPrivacyContextUIProvider,
    FullscreenSigninScreenMediatorDelegate,
    FullscreenSigninScreenViewControllerDelegate,
    IdentityChooserCoordinatorDelegate,
    SceneStateObserver,
    TOSCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate,
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
// Coordinator to show the metric reporting dialog.
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
  // YES if the coordinator is finishing the sign-in flow.
  BOOL _finishing;
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
  if (!self.canSwitchAccount &&
      self.authenticationService->GetPrimaryIdentity()) {
    // If account switch is not possible, don't show the sign-in screen when the
    // user is already signed in.
    [_delegate firstRunScreenCoordinatorWantsToBeStopped:self];
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
  metrics::ProfileMetricsService* profileMetricsService =
      IOSProfileMetricsServiceFactory::GetForProfile(profile);
  self.mediator = [[FullscreenSigninScreenMediator alloc]
          initWithAccountManagerService:self.accountManagerService
                  authenticationService:self.authenticationService
                        identityManager:identityManager
                       localPrefService:localPrefService
                            prefService:prefService
                            syncService:syncService
                       selectedIdentity:self.identity
                            accessPoint:_accessPoint
                            promoAction:_promoAction
                  profileMetricsService:profileMetricsService
      changeProfileContinuationProvider:_changeProfileContinuationProvider];
  self.mediator.consumer = self.viewController;
  self.mediator.delegate = self;
  if (self.mediator.ignoreDismissGesture) {
    self.viewController.modalInPresentation = YES;
  }
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
  if (base::FeatureList::IsEnabled(switches::kBuildExternalPrivacyContext)) {
    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->RegisterExternalPrivacyContextProvider(self);
    [self.browser->GetSceneState() addObserver:self];
  }
}

- (void)stop {
  if (base::FeatureList::IsEnabled(switches::kBuildExternalPrivacyContext)) {
    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->UnregisterExternalPrivacyContextProvider(self);
    [self.browser->GetSceneState() removeObserver:self];
  }
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
      self.accountManagerService->IsValidIdentity(
          signinCompletionIdentity.gaiaId)) {
    self.mediator.selectedIdentity = signinCompletionIdentity;
    self.mediator.addedAccount = YES;
  }
}

// Starts the sign in process.
- (void)startSignIn {
  if (self.mediator.signinInProgress) {
    // Skip sign-in if there is a double tap.
    // See crbug.com/478202195.
    return;
  }
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
  // During FRE, allows sign-in even if the account needs to be reauthenticated.
  authenticationFlow.skipReauthIfNeeded =
      _accessPoint == signin_metrics::AccessPoint::kStartPage;
  [self.mediator startSignInWithAuthenticationFlow:authenticationFlow];
}

// Calls the mediator and the delegate when the coordinator is finished.
- (void)finishPresentingWithSignIn:(BOOL)signIn {
  _finishing = YES;
  [self.mediator finishPresentingWithSignIn:signIn];
  [self.delegate firstRunScreenCoordinatorWantsToBeStopped:self];
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

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  DCHECK(!self.addAccountSigninCoordinator);
  [self stopIdentityChooserCoordinator];
  [self triggerAddAccount];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
      didCloseWithSelectedIdentity:(id<SystemIdentity>)identity {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  if (identity) {
    self.mediator.selectedIdentity = identity;
  }
  [self stopIdentityChooserCoordinator];
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
                         browser:self.browser
                 defaultIdentity:self.mediator.selectedIdentity];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
}

- (void)fullscreenSigninScreenViewControllerViewDidLoad:
    (FullscreenSigninScreenViewController*)viewController {
  [self notifyProviderReadyIfUIAvailable];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [self notifyProviderReadyIfUIAvailable];
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

#pragma mark - ExternalPrivacyContextUIProvider

- (UIViewController*)viewControllerForExternalPrivacyContext {
  if (![self isUIAvailableToShowIOSPrompt]) {
    return nil;
  }
  return self.viewController;
}

- (void)blockUIForExternalPrivacyContextBuild {
  CHECK([self isUIAvailableToShowIOSPrompt]);
  [self.viewController setUIEnabled:NO];
}

- (void)unblockUIOnExternalPrivacyContextBuilt {
  [self.viewController setUIEnabled:YES];
}

#pragma mark - Private

- (void)notifyProviderReadyIfUIAvailable {
  if (base::FeatureList::IsEnabled(switches::kBuildExternalPrivacyContext) &&
      [self isUIAvailableToShowIOSPrompt]) {
    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->ExternalPrivacyContextProviderReady(self);
  }
}

- (BOOL)isUIAvailableToShowIOSPrompt {
  if (_finishing) {
    return NO;
  }
  if (self.mediator.signinInProgress) {
    return NO;
  }
  if (self.TOSCoordinator || self.identityChooserCoordinator ||
      self.addAccountSigninCoordinator || self.UMACoordinator) {
    return NO;
  }
  if (!self.viewController) {
    return NO;
  }
  return YES;
}

@end
