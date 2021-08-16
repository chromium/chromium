// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_coordinator.h"

#include "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_navigation_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mode.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_settings_view_state.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
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
    ChromeCoordinator<SyncSettingsViewState>* syncSettingsCoordinator;
// Confirm cancel sign-in/sync dialog.
@property(nonatomic, strong)
    ActionSheetCoordinator* cancelConfirmationAlertCoordinator;
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
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  self.advancedSettingsSigninMediator = [[AdvancedSettingsSigninMediator alloc]
      initWithSyncSetupService:syncSetupService
         authenticationService:authenticationService
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

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  DCHECK(self.advancedSettingsSigninNavigationController);
  [self.syncSettingsCoordinator stop];
  self.syncSettingsCoordinator = nil;

  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss:
      [self finishedWithSigninResult:SigninCoordinatorResultInterrupted];
      if (completion) {
        completion();
      }
      break;
    case SigninCoordinatorInterruptActionDismissWithoutAnimation:
      [self dismissViewControllerAndFinishWithResult:
                SigninCoordinatorResultInterrupted
                                            animated:NO
                                          completion:completion];
      break;
    case SigninCoordinatorInterruptActionDismissWithAnimation:
      [self dismissViewControllerAndFinishWithResult:
                SigninCoordinatorResultInterrupted
                                            animated:YES
                                          completion:completion];
      break;
  }
}

- (BOOL)isSettingsViewPresented {
  // This coordinator presents the Google services settings.
  return YES;
}

#pragma mark - Private

// Displays the Sync or Google services settings page.
- (void)startSyncSettingsCoordinator {
  DCHECK(!self.syncSettingsCoordinator);

  if (base::FeatureList::IsEnabled(signin::kMobileIdentityConsistency)) {
    ManageSyncSettingsCoordinator* manageSyncSettingsCoordinator =
        [[ManageSyncSettingsCoordinator alloc]
            initWithBaseNavigationController:
                self.advancedSettingsSigninNavigationController
                                     browser:self.browser];
    manageSyncSettingsCoordinator.delegate = self;
    self.syncSettingsCoordinator = manageSyncSettingsCoordinator;
  } else {
    // Init and start Google settings coordinator.
    GoogleServicesSettingsMode mode =
        GoogleServicesSettingsModeAdvancedSigninSettings;
    self.syncSettingsCoordinator = [[GoogleServicesSettingsCoordinator alloc]
        initWithBaseNavigationController:
            self.advancedSettingsSigninNavigationController
                                 browser:self.browser
                                    mode:mode];
  }
  [self.syncSettingsCoordinator start];
}

// Called when a button of |self.cancelConfirmationAlertCoordinator| is pressed.
- (void)cancelConfirmationWithShouldCancelSignin:(BOOL)shouldCancelSignin {
  DCHECK(self.cancelConfirmationAlertCoordinator);
  // -[ActionSheetCoordinator stop] should not be called since the action sheet
  // has been already dismissed. If it is called, the action sheet might dismiss
  // the advanced settings sign-in view controller (instead of doing nothing).
  // This case happens when tapping on the background of the action sheet on
  // iPad.
  self.cancelConfirmationAlertCoordinator = nil;
  if (shouldCancelSignin) {
    [self dismissViewControllerAndFinishWithResult:
              SigninCoordinatorResultCanceledByUser
                                          animated:YES
                                        completion:nil];
  } else {
    RecordAction(
        UserMetricsAction("Signin_Signin_CancelCancelAdvancedSyncSettings"));
  }
}

// Dismisses the current view controller with |animated|, triggers the
// coordinator cleanup and then calls |completion|.
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
// calls |runCompletionCallbackWithSigninResult:completionInfo:| to finish the
// sign-in.
- (void)finishedWithSigninResult:(SigninCoordinatorResult)signinResult {
  DCHECK(self.advancedSettingsSigninNavigationController);
  DCHECK(self.advancedSettingsSigninMediator);
  [self.advancedSettingsSigninMediator
      saveUserPreferenceForSigninResult:signinResult
                    originalSigninState:self.signinStateForCancel];
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
  ChromeIdentity* identity =
      (signinResult == SigninCoordinatorResultSuccess)
          ? authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin)
          : nil;
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:identity];
  [self runCompletionCallbackWithSigninResult:signinResult
                               completionInfo:completionInfo];
}

- (void)showCancelConfirmationAlert {
  DCHECK(!self.cancelConfirmationAlertCoordinator);
  RecordAction(UserMetricsAction("Signin_Signin_CancelAdvancedSyncSettings"));
  self.cancelConfirmationAlertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.advancedSettingsSigninNavigationController
                         browser:self.browser
                           title:nil
                         message:
                             GetNSString(
                                 IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_MESSAGE)
                   barButtonItem:self.syncSettingsCoordinator.navigationItem
                                     .leftBarButtonItem];
  __weak __typeof(self) weakSelf = self;
  [self.cancelConfirmationAlertCoordinator
      addItemWithTitle:
          GetNSString(
              IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_BACK_BUTTON)
                action:^{
                  [weakSelf cancelConfirmationWithShouldCancelSignin:NO];
                }
                 style:UIAlertActionStyleCancel];
  [self.cancelConfirmationAlertCoordinator
      addItemWithTitle:
          GetNSString(
              IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_CANCEL_SYNC_BUTTON)
                action:^{
                  [weakSelf cancelConfirmationWithShouldCancelSignin:YES];
                }
                 style:UIAlertActionStyleDestructive];
  [self.cancelConfirmationAlertCoordinator start];
}

#pragma mark - AdvancedSettingsSigninNavigationControllerNavigationDelegate

- (void)navigationCancelButtonWasTapped {
  [self showCancelConfirmationAlert];
}

- (void)navigationConfirmButtonWasTapped {
  DCHECK(!self.cancelConfirmationAlertCoordinator);
  [self dismissViewControllerAndFinishWithResult:SigninCoordinatorResultSuccess
                                        animated:YES
                                      completion:nil];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return NO;
}

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  // Only show cancel confirmation when "Sync and Google Services" is displayed.
  if (self.syncSettingsCoordinator.isSettingsViewShown) {
    [self showCancelConfirmationAlert];
  }
}

#pragma mark - ManageSyncSettingsCoordinatorDelegate

- (void)manageSyncSettingsCoordinatorWasRemoved:
    (ManageSyncSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.syncSettingsCoordinator, coordinator);
  [self.syncSettingsCoordinator stop];
  self.syncSettingsCoordinator = nil;
}

- (NSString*)manageSyncSettingsCoordinatorTitle {
  return l10n_util::GetNSString(IDS_IOS_MANAGE_SYNC_SETTINGS_TITLE);
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

@end
