// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_setup_service.h"
#import "ios/chrome/browser/sync/model/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_navigation_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::UserMetricsAction;
using l10n_util::GetNSString;

@interface AdvancedSettingsSigninCoordinator () <
    AdvancedSettingsSigninNavigationControllerNavigationDelegate,
    ManageSyncSettingsCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>

// Advanced settings sign-in mediator.
@property(nonatomic, strong)
    AdvancedSettingsSigninMediator* advancedSettingsSigninMediator;
// View controller presented by this coordinator.
@property(nonatomic, strong) AdvancedSettingsSigninNavigationController*
    advancedSettingsSigninNavigationController;
// Coordinator to present Sync settings.
@property(nonatomic, strong)
    ManageSyncSettingsCoordinator* syncSettingsCoordinator;
// Manager for user's Google identities.
@property(nonatomic, assign) signin::IdentityManager* identityManager;
// State used to revert to if the user action is canceled during sign-in.
@property(nonatomic, assign) IdentitySigninState signinStateForCancel;
@end

@implementation AdvancedSettingsSigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               signinState:(IdentitySigninState)signinState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _signinStateForCancel = signinState;
  }
  return self;
}

#pragma mark - SigninCoordinator

- (void)start {
  [super start];
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  DCHECK(
      authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSignin));
  self.identityManager = IdentityManagerFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.advancedSettingsSigninNavigationController =
      [[AdvancedSettingsSigninNavigationController alloc] init];
  self.advancedSettingsSigninNavigationController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.advancedSettingsSigninNavigationController.navigationDelegate = self;

  [self startSyncSettingsCoordinator];

  // Create the mediator.
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  self.advancedSettingsSigninMediator = [[AdvancedSettingsSigninMediator alloc]
      initWithAuthenticationService:authenticationService
                        syncService:syncService
                        prefService:self.browser->GetBrowserState()->GetPrefs()
                    identityManager:self.identityManager];
  self.advancedSettingsSigninNavigationController.presentationController
      .delegate = self;

  // Present the navigation controller that now contains the Google settings
  // view controller.
  [self.baseViewController
      presentViewController:self.advancedSettingsSigninNavigationController
                   animated:YES
                 completion:nil];
}

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  DCHECK(self.advancedSettingsSigninNavigationController);
  [self.syncSettingsCoordinator stop];
  self.syncSettingsCoordinator = nil;

  switch (action) {
    case SigninCoordinatorInterrupt::UIShutdownNoDismiss:
      [self finishedWithSigninResult:SigninCoordinatorResultInterrupted];
      if (completion) {
        completion();
      }
      break;
    case SigninCoordinatorInterrupt::DismissWithoutAnimation:
      [self dismissViewControllerAndFinishWithResult:
                SigninCoordinatorResultInterrupted
                                            animated:NO
                                          completion:completion];
      break;
    case SigninCoordinatorInterrupt::DismissWithAnimation:
      [self dismissViewControllerAndFinishWithResult:
                SigninCoordinatorResultInterrupted
                                            animated:YES
                                          completion:completion];
      break;
  }
}

#pragma mark - Private

// Displays the Sync settings page.
- (void)startSyncSettingsCoordinator {
  DCHECK(!self.syncSettingsCoordinator);

  ManageSyncSettingsCoordinator* manageSyncSettingsCoordinator =
      [[ManageSyncSettingsCoordinator alloc]
          initWithBaseNavigationController:
              self.advancedSettingsSigninNavigationController
                                   browser:self.browser
                              accountState:SyncSettingsAccountState::
                                               kAdvancedInitialSyncSetup];
  manageSyncSettingsCoordinator.delegate = self;
  self.syncSettingsCoordinator = manageSyncSettingsCoordinator;
  [self.syncSettingsCoordinator start];
}

// Dismisses the current view controller with `animated`, triggers the
// coordinator cleanup and then calls `completion`.
- (void)dismissViewControllerAndFinishWithResult:(SigninCoordinatorResult)result
                                        animated:(BOOL)animated
                                      completion:(ProceduralBlock)completion {
  DCHECK_EQ(self.advancedSettingsSigninNavigationController,
            self.baseViewController.presentedViewController);
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock dismissCompletion = ^() {
    [weakSelf finishedWithSigninResult:result];
    if (completion) {
      completion();
    }
  };
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:dismissCompletion];
}

// Does the cleanup once the view has been dismissed, calls the metrics and
// calls `runCompletionCallbackWithSigninResult:completionInfo:` to finish the
// sign-in.
- (void)finishedWithSigninResult:(SigninCoordinatorResult)signinResult {
  DCHECK_NE(SigninCoordinatorResultCanceledByUser, signinResult);
  DCHECK(self.advancedSettingsSigninNavigationController);
  DCHECK(self.advancedSettingsSigninMediator);
  [self.advancedSettingsSigninMediator
      saveUserPreferenceForSigninResult:signinResult
                    originalSigninState:self.signinStateForCancel];
  [self.advancedSettingsSigninNavigationController cleanUpSettings];
  self.advancedSettingsSigninNavigationController.navigationDelegate = nil;
  self.advancedSettingsSigninNavigationController.presentationController
      .delegate = nil;
  self.advancedSettingsSigninNavigationController = nil;
  self.advancedSettingsSigninMediator = nil;
  [self.syncSettingsCoordinator stop];
  self.syncSettingsCoordinator = nil;
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  DCHECK(!syncSetupService->HasUncommittedChanges())
      << "-[GoogleServicesSettingsCoordinator stop] should commit sync "
         "changes.";
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  id<SystemIdentity> identity =
      (signinResult == SigninCoordinatorResultSuccess)
          ? authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin)
          : nil;
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:identity];
  [self runCompletionCallbackWithSigninResult:signinResult
                               completionInfo:completionInfo];
}

#pragma mark - AdvancedSettingsSigninNavigationControllerNavigationDelegate

- (void)navigationDoneButtonWasTapped:
    (AdvancedSettingsSigninNavigationController*)controller {
  [self dismissViewControllerAndFinishWithResult:SigninCoordinatorResultSuccess
                                        animated:YES
                                      completion:nil];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self finishedWithSigninResult:SigninCoordinatorResultSuccess];
}

#pragma mark - ManageSyncSettingsCoordinatorDelegate

- (void)manageSyncSettingsCoordinatorWasRemoved:
    (ManageSyncSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.syncSettingsCoordinator, coordinator);
  [self.syncSettingsCoordinator stop];
  self.syncSettingsCoordinator = nil;
}

- (NSString*)manageSyncSettingsCoordinatorTitle {
  return l10n_util::GetNSString(IDS_IOS_SYNC_SETTINGS_TITLE);
}

- (void)manageSyncSettingsCoordinatorNeedToOpenChromeSyncWebPage:
    (ManageSyncSettingsCoordinator*)coordinator {
  switch (self.signinStateForCancel) {
    case IdentitySigninStateSignedOut:
      // AdvancedSettingsSigninCoordinator will be interrupted to open the
      // Chrome Sync webpage URL.
      // We need to leave the user signed in so they can open this web page.
      self.signinStateForCancel = IdentitySigninStateSignedInWithSyncDisabled;
      break;
    case IdentitySigninStateSignedInWithSyncDisabled:
    case IdentitySigninStateSignedInWithSyncEnabled:
      // Nothing to do.
      break;
  }
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, syncSettingsCoordinator: %p, signinStateForCancel: %lu>",
          self.class.description, self, self.syncSettingsCoordinator,
          self.signinStateForCancel];
}

@end
