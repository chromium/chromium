// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/signin_policy_scene_agent.h"

#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"

@interface SigninPolicySceneAgent () <AppStateObserver,
                                      AuthenticationServiceObserving,
                                      IdentityManagerObserverBridgeDelegate,
                                      UIBlockerManagerObserver> {
  // Observes changes in identity to make sure that the sign-in state matches
  // the BrowserSignin policy.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authenticationServiceObserverBridge;
}

// Handler of application commands.
@property(nonatomic, weak, readonly) id<ApplicationCommands>
    applicationCommandsHandler;

// Browser of the main interface of the scene.
@property(nonatomic, assign) Browser* mainBrowser;

// SceneUIProvider that provides the scene UI objects.
@property(nonatomic, weak, readonly) id<SceneUIProvider> sceneUIProvider;

// Handler of policy change commands.
@property(nonatomic, weak, readonly) id<PolicyChangeCommands>
    policyChangeCommandsHandler;

@end

@implementation SigninPolicySceneAgent

- (instancetype)initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider
             applicationCommandsHandler:
                 (id<ApplicationCommands>)applicationCommandsHandler
            policyChangeCommandsHandler:
                (id<PolicyChangeCommands>)policyChangeCommandsHandler {
  self = [super init];
  if (self) {
    _sceneUIProvider = sceneUIProvider;
    _applicationCommandsHandler = applicationCommandsHandler;
    _policyChangeCommandsHandler = policyChangeCommandsHandler;
  }
  return self;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];

  [self.sceneState.appState addObserver:self];
  [self.sceneState.appState addUIBlockerManagerObserver:self];
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  // Tear down objects tied to the scene state before it is deleted.
  [self tearDownObservers];
  [self.sceneState.appState removeObserver:self];
  [self.sceneState.appState removeUIBlockerManagerObserver:self];
  [self.sceneState removeObserver:self];
  self.mainBrowser = nullptr;
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  // Setup objects that need the browser UI objects before being set.
  self.mainBrowser =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser;
  [self setupObservers];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Monitor the scene activation level to consider showing the sign-in prompt
  // when the scene becomes active and in the foreground. In which case the
  // scene is visible and interactable.
  [self handleSigninPromptsIfUIAvailable];
}

- (void)sceneStateDidHideModalOverlay:(SceneState*)sceneState {
  // Reconsider showing the forced sign-in prompt if the UI blocker is
  // dismissed which might be because the scene that was displaying the
  // sign-in prompt previously was closed. Choosing a new scene to prompt
  // is needed in that case.
  [self handleSigninPromptsIfUIAvailable];
}

- (void)signinDidEnd:(SceneState*)sceneState {
  // Consider showing the forced sign-in prompt when the sign-in prompt is
  // dismissed/done because the browser may be signed out if sign-in is
  // cancelled.
  [self handleSigninPromptsIfUIAvailable];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  // Monitor the app intialization stages to consider showing the sign-in
  // prompts at a point in the initialization of the app that allows it.
  [self handleSigninPromptsIfUIAvailable];
}

#pragma mark - UIBlockerManagerObserver

- (void)currentUIBlockerRemoved {
  [self handleSigninPromptsIfUIAvailable];
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  [self handleSigninPromptsIfUIAvailable];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  // Consider showing the sign-in prompts when there is change in the
  // primary account.
  [self handleSigninPromptsIfUIAvailable];
}

#pragma mark - Internal

- (void)setupObservers {
  DCHECK(self.mainBrowser);

  ProfileIOS* profile = self.mainBrowser->GetProfile();
  // Set observer for service status changes.
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  _authenticationServiceObserverBridge =
      std::make_unique<AuthenticationServiceObserverBridge>(authService, self);

  // Set observer for primary account changes.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  _identityObserverBridge =
      std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                              self);
}

- (void)tearDownObservers {
  _authenticationServiceObserverBridge.reset();
  _identityObserverBridge.reset();
}

- (BOOL)isForcedSignInRequiredByPolicy {
  DCHECK(self.mainBrowser);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(
          self.mainBrowser->GetProfile());
  switch (authService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninAllowed:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
      return NO;
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
      break;
  }
  // Skip prompting to sign-in when there is already a primary account
  // signed in.
  return !authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

// Handle the policy sign-in prompts if the scene UI is available to show
// prompts.
- (void)handleSigninPromptsIfUIAvailable {
  if (![self isUIAvailableToPrompt]) {
    return;
  }

  if (self.sceneState.appState.shouldShowForceSignOutPrompt) {
    // Show the sign-out prompt if the user was signed out due to policy.
    [self.policyChangeCommandsHandler showForceSignedOutPrompt];
    self.sceneState.appState.shouldShowForceSignOutPrompt = NO;
  }

  if ([self isForcedSignInRequiredByPolicy]) {
    // Sanity check that when the policy is handled while there is a UIBlocker
    // that the scene that will show the sign-in prompt corresponds to the
    // target of the UI blocker.
    if (self.sceneState.appState.currentUIBlocker) {
      DCHECK(self.sceneState.appState.currentUIBlocker == self.sceneState);
    }

    // Put a UI blocker to stop the other scenes from handling the policy.
    // This UI blocker will be superimposed on the one of the sign-in prompt
    // command and maybe the existing sign-in prompt (to be dismissed) to not
    // leave any gap that would allow the other scenes to handle the sign-in
    // policy (by keeping `sceneState.presentingModalOverlay` == YES). There
    // won't be issues with the superimpositions of the UI blockers because this
    // is done on the same SceneState target, which will only increase the
    // target counter. If the scene is dismissed, the count will be decremented
    // to zero leaving the way for another scene to take over the forced
    // sign-in prompt.
    __block std::unique_ptr<ScopedUIBlocker> uiBlocker =
        std::make_unique<ScopedUIBlocker>(self.sceneState);

    __weak __typeof(self) weakSelf = self;
    [self.applicationCommandsHandler dismissModalDialogsWithCompletion:^{
      [weakSelf showForcedSigninPrompt];
      // Remove the blocker after the showSignin: command to make sure that the
      // blocker doesn't go down to 0 in which case the other scenes will try to
      // take over handling the force sign-in prompt.
      uiBlocker.reset();
    }];
  }
}

// Shows the forced sign-in prompt using the application command.
- (void)showForcedSigninPrompt {
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kForcedSigninAndSync
               identity:nil
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:nil];

  [self.applicationCommandsHandler
              showSignin:command
      baseViewController:[self.sceneUIProvider activeViewController]];
}

// YES if the scene and the app are in a state where the UI of the scene is
// available to show sign-in related prompts.
- (BOOL)isUIAvailableToPrompt {
  if (self.sceneState.appState.initStage < AppInitStage::kFinal) {
    return NO;
  }

  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return NO;
  }

  if (self.sceneState.appState.currentUIBlocker) {
    // Return NO when the scene cannot present views because it is blocked.
    return NO;
  }

  if (self.sceneState.signinInProgress) {
    // Prompting to sign-in is already in progress in that scene, no need to
    // present the forced sign-in prompt on top of that. The other scenes will
    // have `self.sceneState.presentingModalOverlay` == YES which will stop
    // them from handling the policy as well. For example, this stops the scene
    // from rehandling the forced sign-in policy when foregrounded.
    return NO;
  }

  return YES;
}

@end
