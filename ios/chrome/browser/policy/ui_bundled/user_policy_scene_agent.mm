// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/user_policy_scene_agent.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy/user_policy_prompt_coordinator.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy/user_policy_prompt_coordinator_delegate.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy_util.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "url/gurl.h"

@interface UserPolicySceneAgent () <AppStateObserver> {
  // Scoped UI blocker that blocks the other scenes/windows if the dialog is
  // shown on this scene.
  std::unique_ptr<ScopedUIBlocker> _uiBlocker;
}

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

// Coordinator for the User Policy prompt.
@property(nonatomic, strong)
    UserPolicyPromptCoordinator* userPolicyPromptCoordinator;

@end

@interface UserPolicySceneAgent () <UserPolicyPromptCoordinatorDelegate>
@end

// TODO(crbug.com/40225352): Remove the logic to show the notification dialog
// once we determined that this isn't needed anymore.

@implementation UserPolicySceneAgent {
  // Manager for user policies that provides access to the stored policy data.
  raw_ptr<policy::UserCloudPolicyManager> _userPolicyManager;
}

- (instancetype)initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider
                            authService:(AuthenticationService*)authService
             applicationCommandsHandler:
                 (id<ApplicationCommands>)applicationCommandsHandler
                            prefService:(PrefService*)prefService
                            mainBrowser:(Browser*)mainBrowser
                          policyService:
                              (policy::UserPolicySigninService*)policyService
                      userPolicyManager:
                          (policy::UserCloudPolicyManager*)userPolicyManager {
  self = [super init];
  if (self) {
    _sceneUIProvider = sceneUIProvider;
    _authService = authService;
    _applicationCommandsHandler = applicationCommandsHandler;
    _prefService = prefService;
    _mainBrowser = mainBrowser;
    _policyService = policyService;
    _userPolicyManager = userPolicyManager;
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
  [self stopUserPolicyPromptCoordinator];
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
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  // Monitor the app intialization stages to consider showing the sign-in
  // prompts at a point in the initialization of the app that allows it.
  [self maybeShowUserPolicyNotification];
}

#pragma mark - UserPolicyPromptCoordinatorDelegate

- (void)didCompletePresentationAndShowLearnMoreAfterward:
    (BOOL)showLearnMoreAfterward {
  // Mark the prompt as seen.
  self.prefService->SetBoolean(
      policy::policy_prefs::kUserPolicyNotificationWasShown, true);
  self.policyService->OnUserPolicyNotificationSeen();

  [self stopUserPolicyPromptCoordinator];

  if (showLearnMoreAfterward) {
    // Show the enterprise learn more page in a browser tab after dismissing the
    // notice. Not using modal so it won't interfere with the ongoing dismiss
    // animation.
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithURLFromChrome:GURL(kChromeUIManagementURL)];
    [self.applicationCommandsHandler openURLInNewTab:command];
  }
}

#pragma mark - Internal

// Returns YES if the scene UI is available to show the notification dialog.
- (BOOL)isUIAvailableToShowNotification {
  if (self.sceneState.appState.initStage < AppInitStage::kFinal) {
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

  if (!IsUserPolicyNotificationNeeded(self.authService, self.prefService,
                                      _userPolicyManager)) {
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
  DCHECK(self.sceneState.UIEnabled);

  _uiBlocker = std::make_unique<ScopedUIBlocker>(self.sceneState);

  __weak __typeof(self) weakSelf = self;
  [self.applicationCommandsHandler dismissModalDialogsWithCompletion:^{
    __typeof(self) strongSelf = weakSelf;
    [strongSelf
        showManagedConfirmationOnViewController:[strongSelf.sceneUIProvider
                                                        activeViewController]
                                        browser:strongSelf.mainBrowser];
  }];
}

// Shows the notification dialog for the account on the `viewController`.
- (void)showManagedConfirmationOnViewController:
            (UIViewController*)viewController
                                        browser:(Browser*)browser {
  self.userPolicyPromptCoordinator = [[UserPolicyPromptCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser];
  self.userPolicyPromptCoordinator.delegate = self;

  [self.userPolicyPromptCoordinator start];
}

- (void)stopUserPolicyPromptCoordinator {
  _uiBlocker.reset();

  [self.userPolicyPromptCoordinator stop];
  self.userPolicyPromptCoordinator = nil;
}

@end
