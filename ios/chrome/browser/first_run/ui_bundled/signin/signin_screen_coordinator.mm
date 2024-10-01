// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/signin/signin_screen_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/signin/signin_screen_consumer.h"
#import "ios/chrome/browser/first_run/ui_bundled/signin/signin_screen_mediator.h"
#import "ios/chrome/browser/first_run/ui_bundled/signin/signin_screen_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/uma/uma_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/tos_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

@interface SigninScreenCoordinator () <IdentityChooserCoordinatorDelegate,
                                       SigninScreenViewControllerDelegate,
                                       TOSCommands,
                                       UMACoordinatorDelegate>

// First run screen delegate.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;
// Sign-in screen view controller.
@property(nonatomic, strong) SigninScreenViewController* viewController;
// Sign-in screen mediator.
@property(nonatomic, strong) SigninScreenMediator* mediator;
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

@implementation SigninScreenCoordinator {
  signin_metrics::AccessPoint _accessPoint;
  signin_metrics::PromoAction _promoAction;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                            delegate:(id<FirstRunScreenDelegate>)delegate
                         accessPoint:(signin_metrics::AccessPoint)accessPoint
                         promoAction:(signin_metrics::PromoAction)promoAction {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(!browser->GetProfile()->IsOffTheRecord());
    _baseNavigationController = navigationController;
    _delegate = delegate;
    _UMAReportingUserChoice = kDefaultMetricsReportingCheckboxValue;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
  }
  return self;
}

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TOSCommands)];
  id<TOSCommands> TOSHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), TOSCommands);
  self.viewController = [[SigninScreenViewController alloc] init];
  self.viewController.TOSHandler = TOSHandler;
  self.viewController.delegate = self;

  ProfileIOS* profile = self.browser->GetProfile();
  self.authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (self.authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // Don't show the sign-in screen since the user is already signed in.
    [_delegate screenWillFinishPresenting];
    return;
  }
  self.accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.browser->GetProfile());
  PrefService* localPrefService = GetApplicationContext()->GetLocalState();
  PrefService* prefService = profile->GetPrefs();
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  self.mediator = [[SigninScreenMediator alloc]
      initWithAccountManagerService:self.accountManagerService
              authenticationService:self.authenticationService
                    identityManager:identityManager
                   localPrefService:localPrefService
                        prefService:prefService
                        syncService:syncService
                        accessPoint:_accessPoint
                        promoAction:_promoAction];
  self.mediator.consumer = self.viewController;
  if (self.mediator.ignoreDismissGesture) {
    self.viewController.modalInPresentation = YES;
  }
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
}

- (void)stop {
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator = nil;
  self.delegate = nil;
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.accountManagerService = nil;
  self.authenticationService = nil;
  [super stop];
}

#pragma mark - InterruptibleChromeCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  if (self.addAccountSigninCoordinator) {
    [self.addAccountSigninCoordinator interruptWithAction:action
                                               completion:completion];
  } else if (completion) {
    completion();
  }
}

#pragma mark - Private

- (void)stopUMACoordinator {
  [self.UMACoordinator stop];
  self.UMACoordinator.delegate = nil;
  self.UMACoordinator = nil;
}

// Starts the coordinator to present the Add Account module.
- (void)triggerAddAccount {
  [self.mediator userAttemptedToSignin];
  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:_accessPoint];
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
  AuthenticationFlow* authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:self.browser
                      identity:self.mediator.selectedIdentity
                   accessPoint:_accessPoint
             postSignInActions:PostSignInActionSet({PostSignInAction::kNone})
      presentingViewController:self.viewController];
  authenticationFlow.precedingHistorySync = YES;
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^() {
    [weakSelf finishPresentingWithSignIn:YES];
  };
  [self.mediator startSignInWithAuthenticationFlow:authenticationFlow
                                        completion:completion];
}

// Calls the mediator and the delegate when the coordinator is finished.
- (void)finishPresentingWithSignIn:(BOOL)signIn {
  [self.mediator finishPresentingWithSignIn:signIn];
  [self.delegate screenWillFinishPresenting];
}

// Shows the UMA dialog so the user can manage metric reporting.
- (void)showUMADialog {
  DCHECK(!self.UMACoordinator);
  self.UMACoordinator = [[UMACoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
               UMAReportingValue:self.mediator.UMAReportingUserChoice];
  self.UMACoordinator.delegate = self;
  [self.UMACoordinator start];
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

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  switch (self.authenticationService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      if (self.mediator.selectedIdentity) {
        [self startSignIn];
      } else {
        [self triggerAddAccount];
      }
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      [self finishPresentingWithSignIn:NO];
      return;
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
    NOTREACHED_IN_MIGRATION() << std::string("Unknown URL ")
                              << base::SysNSStringToUTF8(URL.absoluteString);
  }
}

#pragma mark - SigninScreenViewControllerDelegate

- (void)showAccountPickerFromPoint:(CGPoint)point {
  DCHECK(!self.identityChooserCoordinator);
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
  self.identityChooserCoordinator.selectedIdentity =
      self.mediator.selectedIdentity;
}

#pragma mark - TOSCommands

- (void)showTOSPage {
  DCHECK(!self.TOSCoordinator);
  self.mediator.TOSLinkWasTapped = YES;
  self.TOSCoordinator =
      [[TOSCoordinator alloc] initWithBaseViewController:self.viewController
                                                 browser:self.browser];
  [self.TOSCoordinator start];
}

- (void)closeTOSPage {
  DCHECK(self.TOSCoordinator);
  [self.TOSCoordinator stop];
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
