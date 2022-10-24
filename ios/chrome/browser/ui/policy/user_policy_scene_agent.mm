// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/policy/user_policy_scene_agent.h"

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/main/scene_ui_provider.h"
#import "ios/chrome/browser/ui/policy/user_policy_util.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UserPolicySceneAgent () <AppStateObserver> {
  // Scoped UI blocker that blocks the other scenes/windows if the dialog is
  // shown on this scene.
  std::unique_ptr<ScopedUIBlocker> _uiBlocker;
}

// Alert coordinator that is used to display the notification dialog.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// Browser of the main interface of the scene.
@property(nonatomic, assign, readonly) Browser* mainBrowser;

// SceneUIProvider that provides the scene UI objects.
@property(nonatomic, weak) id<SceneUIProvider> sceneUIProvider;

// Authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;

// Handler for application commands.
@property(nonatomic, weak, readonly) id<ApplicationCommands>
    applicationCommandsHandler;

// Pref service.
@property(nonatomic, assign, readonly) PrefService* prefService;

// User Policy service.
@property(nonatomic, assign, readonly)
    policy::UserPolicySigninService* policyService;

@end

// TODO(crbug.com/1325115): Remove the logic to show the notification dialog
// once we determined that this isn't needed anymore.

@implementation UserPolicySceneAgent

- (instancetype)initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider
                            authService:(AuthenticationService*)authService
             applicationCommandsHandler:
                 (id<ApplicationCommands>)applicationCommandsHandler
                            prefService:(PrefService*)prefService
                            mainBrowser:(Browser*)mainBrowser
                          policyService:
                              (policy::UserPolicySigninService*)policyService {
  self = [super init];
  if (self) {
    _sceneUIProvider = sceneUIProvider;
    _authService = authService;
    _applicationCommandsHandler = applicationCommandsHandler;
    _prefService = prefService;
    _mainBrowser = mainBrowser;
    _policyService = policyService;
  }
  return self;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];

  [self.sceneState.appState addObserver:self];
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  // Tear down objects tied to the scene state before it is deleted.
  [self.sceneState.appState removeObserver:self];
  _uiBlocker.reset();
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Monitor the scene activation level to consider showing the sign-in prompt
  // when the scene becomes active and is in the foreground. In which case the
  // scene is visible and interactable.
  [self maybeShowUserPolicyNotification];
}

- (void)sceneStateDidHideModalOverlay:(SceneState*)sceneState {
  // Reconsider showing the forced sign-in prompt if the UI blocker is
  // dismissed which might be because the scene that was displaying the
  // sign-in prompt previously was closed. Choosing a new scene to prompt
  // is needed in that case.
  [self maybeShowUserPolicyNotification];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  // Monitor the app intialization stages to consider showing the sign-in
  // prompts at a point in the initialization of the app that allows it.
  [self maybeShowUserPolicyNotification];
}

#pragma mark - Internal

// Returns YES if the scene UI is available to show the notification dialog.
- (BOOL)isUIAvailableToShowNotification {
  if (self.sceneState.appState.initStage < InitStageFinal) {
    // Return NO when the app isn't yet fully initialized.
    return NO;
  }

  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    // Return NO when the scene isn't visible, active, and in the foreground.
    return NO;
  }

  // Return NO when the scene cannot present views because it is blocked. This
  // is what prevents showing more than one dialog at a time.
  return !self.sceneState.appState.currentUIBlocker;
}

// Shows the User Policy notification dialog if the requirements are fulfilled.
- (void)maybeShowUserPolicyNotification {
  if (![self isUIAvailableToShowNotification]) {
    return;
  }

  if (!IsUserPolicyNotificationNeeded(self.authService, self.prefService)) {
    // Skip notification if the notification isn't needed anymore. This
    // situation can happen when the user had already dismissed the
    // notification dialog during the same session.
    return;
  }

  [self showNotification];
}

// Shows the notification dialog on top of the active view controller (e.g. the
// browser view controller).
- (void)showNotification {
  DCHECK(!self.alertCoordinator);
  DCHECK(self.sceneState.UIEnabled);

  _uiBlocker = std::make_unique<ScopedUIBlocker>(self.sceneState);

  __weak __typeof(self) weakSelf = self;
  [self.applicationCommandsHandler dismissModalDialogsWithCompletion:^{
    __typeof(self) strongSelf = weakSelf;
    [strongSelf
        showManagedConfirmationForHostedDomain:[strongSelf hostedDomain]
                                viewController:[strongSelf.sceneUIProvider
                                                       activeViewController]
                                       browser:strongSelf.mainBrowser];
  }];
}

// Returns the hosted domain of the primary account. Returns an empty string if
// the account isn't managed OR isn't syncing.
- (NSString*)hostedDomain {
  return base::SysUTF16ToNSString(
      HostedDomainForPrimaryAccount(self.mainBrowser));
}

// Shows the notification dialog for the account in `hostedDomain` on the
// provided `viewController`.
- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                viewController:(UIViewController*)viewController
                                       browser:(Browser*)browser {
  DCHECK(!self.alertCoordinator);

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_USER_POLICY_NOTIFICATION_TITLE);
  NSString* subtitle =
      l10n_util::GetNSStringF(IDS_IOS_USER_POLICY_NOTIFICATION_SUBTITLE,
                              base::SysNSStringToUTF16(hostedDomain));
  NSString* continueLabel =
      l10n_util::GetNSString(IDS_IOS_USER_POLICY_CONTINUE);
  NSString* signOutAndClearDataLabel =
      l10n_util::GetNSString(IDS_IOS_USER_POLICY_SIGNOUT_AND_CLEAR_DATA);

  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:viewController
                                                   browser:browser
                                                     title:title
                                                   message:subtitle];

  __weak __typeof(self) weakSelf = self;
  __weak AlertCoordinator* weakAlert = self.alertCoordinator;
  ProceduralBlock acceptBlock = ^{
    [weakSelf didContinueFromNotification:weakAlert];
  };
  ProceduralBlock cancelBlock = ^{
    [weakSelf didSignoutFromNotification:weakAlert];
  };

  [self.alertCoordinator addItemWithTitle:signOutAndClearDataLabel
                                   action:cancelBlock
                                    style:UIAlertActionStyleDestructive];
  [self.alertCoordinator addItemWithTitle:continueLabel
                                   action:acceptBlock
                                    style:UIAlertActionStyleDefault];
  [self.alertCoordinator setCancelAction:cancelBlock];
  [self.alertCoordinator start];
}

- (void)alertControllerDidComplete:(AlertCoordinator*)alertCoordinator {
  DCHECK(self.alertCoordinator == alertCoordinator);

  self.alertCoordinator = nil;

  self.prefService->SetBoolean(
      policy::policy_prefs::kUserPolicyNotificationWasShown, true);
  self.policyService->OnUserPolicyNotificationSeen();

  // Release the UI blockers on the other scenes because the notification dialog
  // is now dismissed.
  _uiBlocker.reset();
}

- (void)didContinueFromNotification:(AlertCoordinator*)alertCoordinator {
  [self alertControllerDidComplete:alertCoordinator];
}

- (void)didSignoutFromNotification:(AlertCoordinator*)alertCoordinator {
  __weak __typeof(self) weakSelf = self;
  self.authService->SignOut(
      signin_metrics::USER_CLICKED_SIGNOUT_FROM_USER_POLICY_NOTIFICATION_DIALOG,
      false, ^{
        [weakSelf alertControllerDidComplete:alertCoordinator];
      });
}

@end
