// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/signin_policy_scene_agent.h"

#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin/coordinator/fullscreen_signin_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_screen_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
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

@interface SigninPolicySceneAgent () <AuthenticationServiceObserving,
                                      IdentityManagerObserverBridgeDelegate,
                                      FullscreenSigninCoordinatorDelegate,
                                      ProfileStateObserver,
                                      UIBlockerManagerObserver> {
  // Observes changes in identity to make sure that the sign-in state matches
  // the BrowserSignin policy.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authenticationServiceObserverBridge;
  FullscreenSigninCoordinator* _fullscreenSigninCoordinator;
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

  [self.sceneState.profileState addObserver:self];
  [self.sceneState.profileState addUIBlockerManagerObserver:self];
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  // Tear down objects tied to the scene state before it is deleted.
  [self stopFullScreenSigninCoordinator];
  [self tearDownObservers];
  [self.sceneState.profileState removeObserver:self];
  [self.sceneState.profileState removeUIBlockerManagerObserver:self];
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

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  // Monitor the profile initialization stages to consider showing the sign-in
  // prompts at a point in the initialization of the profile that allows it.
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

- (void)dealloc {
  CHECK(!_authenticationServiceObserverBridge, base::NotFatalUntil::M145);
  CHECK(!_identityObserverBridge, base::NotFatalUntil::M145);
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
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.mainBrowser->GetProfile());
  return !identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

// Handle the policy sign-in prompts if the scene UI is available to show
// prompts.
- (void)handleSigninPromptsIfUIAvailable {
  if (![self isUIAvailableToPrompt]) {
    return;
  }

  if (self.sceneState.profileState.shouldShowForceSignOutPrompt) {
    // Show the sign-out prompt if the user was signed out due to policy.
    [self.policyChangeCommandsHandler showForceSignedOutPrompt];
    self.sceneState.profileState.shouldShowForceSignOutPrompt = NO;
  }

  if ([self isForcedSignInRequiredByPolicy]) {
    // Sanity check that when the policy is handled while there is a UIBlocker
    // that the scene that will show the sign-in prompt corresponds to the
    // target of the UI blocker.
    if (self.sceneState.profileState.currentUIBlocker) {
      CHECK_EQ(self.sceneState.profileState.currentUIBlocker, self.sceneState);
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
    //
    // Use the UIBlockerExtent::kApplication extent since the sign-in policies
    // have to be pushed through the platform which concerns the entire app in
    // itself including all profiles.
    __block std::unique_ptr<ScopedUIBlocker> uiBlocker =
        std::make_unique<ScopedUIBlocker>(self.sceneState,
                                          UIBlockerExtent::kApplication);

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
  // It's possible that the force-signin is *not* required anymore at this point
  // (either because the policy changed, or because the user is already signed
  // in now). In that case, nothing to do here.
  if (![self isForcedSignInRequiredByPolicy]) {
    return;
  }
  UIViewController* viewController =
      [self.sceneUIProvider activeViewController];
  SigninScreenProvider* signinScreenProvider =
      [[SigninScreenProvider alloc] init];
  _fullscreenSigninCoordinator = [[FullscreenSigninCoordinator alloc]
             initWithBaseViewController:viewController
                                browser:self.mainBrowser
                         screenProvider:signinScreenProvider
                           contextStyle:SigninContextStyle::kDefault
                            accessPoint:signin_metrics::AccessPoint::
                                            kForcedSignin
      changeProfileContinuationProvider:DoNothingContinuationProvider()];
  _fullscreenSigninCoordinator.delegate = self;
  [_fullscreenSigninCoordinator start];
}

// YES if the scene and the profile are in a state where the UI of the scene is
// available to show sign-in related prompts.
- (BOOL)isUIAvailableToPrompt {
  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return NO;
  }

  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return NO;
  }

  if (self.sceneState.profileState.currentUIBlocker) {
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

#pragma mark - FullscreenSigninCoordinatorDelegate

- (void)fullscreenSigninCoordinatorWantsToBeStopped:
            (FullscreenSigninCoordinator*)coordinator
                                             result:(SigninCoordinatorResult)
                                                        result {
  CHECK_EQ(coordinator, _fullscreenSigninCoordinator);
  [self stopFullScreenSigninCoordinator];
}

#pragma mark - Private

- (void)stopFullScreenSigninCoordinator {
  [_fullscreenSigninCoordinator stop];
  _fullscreenSigninCoordinator.delegate = nil;
  _fullscreenSigninCoordinator = nil;
}

@end
