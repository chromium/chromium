// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/signin_policy_scene_agent.h"

#include "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/policy_change_commands.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninPolicySceneAgent () <AppStateObserver,
                                      PrefObserverDelegate,
                                      IdentityManagerObserverBridgeDelegate> {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefsObserverBridge;
  // Registrar for pref change notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  // Observes changes in identity to make sure that the sign-in state matches
  // the BrowserSignin policy.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
}

@property(nonatomic, weak) CommandDispatcher* dispatcher;

// Browser of the main interface of the scene.
@property(nonatomic, assign) Browser* mainBrowser;

@end

@implementation SigninPolicySceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher {
  if ([super init])
    _dispatcher = dispatcher;
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
  [self tearDownObservers];
  [self.sceneState.appState removeObserver:self];
  [self.sceneState removeObserver:self];
  self.mainBrowser = nullptr;
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  // Setup objects that need the browser UI objects before being set.
  self.mainBrowser = self.sceneState.interfaceProvider.mainInterface.browser;
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
    didTransitionFromInitStage:(InitStage)previousInitStage {
  // Monitor the app intialization stages to consider showing the sign-in
  // prompts at a point in the initialization of the app that allows it.
  [self handleSigninPromptsIfUIAvailable];
}

#pragma mark - PrefObserverDelegate

// TODO(crbug.com/1244632): Use the Authentication Service sign-in status API
// instead of this when available.
- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Reconsider showing the sign-in prompts when the value of the sign-in
  // policy changes.
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

  // Set observer for policy changes.
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(prefService);
  _prefsObserverBridge = std::make_unique<PrefObserverBridge>(self);
  _prefsObserverBridge->ObserveChangesForPreference(prefs::kBrowserSigninPolicy,
                                                    _prefChangeRegistrar.get());

  // Set observer for primary account changes.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(
          self.mainBrowser->GetBrowserState());
  _identityObserverBridge =
      std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                              self);
}

- (void)tearDownObservers {
  _prefChangeRegistrar.reset();
  _prefsObserverBridge.reset();
  _identityObserverBridge.reset();
}

- (BOOL)isForcedSignInRequiredByPolicy {
  DCHECK(self.mainBrowser);

  if (!IsForceSignInEnabled()) {
    return NO;
  }

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.mainBrowser->GetBrowserState());
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
    [HandlerForProtocol(self.dispatcher, PolicyChangeCommands)
        showForceSignedOutPrompt];
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
    // policy (by keeping |sceneState.presentingModalOverlay| == YES). There
    // won't be issues with the superimpositions of the UI blockers because this
    // is done on the same SceneState target, which will only increase the
    // target counter. If the scene is dismissed, the count will be decremented
    // to zero leaving the way for another scene to take over the forced
    // sign-in prompt.
    __block std::unique_ptr<ScopedUIBlocker> uiBlocker =
        std::make_unique<ScopedUIBlocker>(self.sceneState);
    // Prompt to sign in if required by policy.
    id<ApplicationCommands> handler =
        HandlerForProtocol(self.dispatcher, ApplicationCommands);

    __weak __typeof(self) weakSelf = self;
    [handler dismissModalDialogsWithCompletion:^{
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
      initWithOperation:AUTHENTICATION_OPERATION_FORCED_SIGNIN
               identity:nil
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:nil];

  id<ApplicationCommands> handler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);
  [handler showSignin:command
      baseViewController:self.sceneState.controller.activeViewController];
}

// YES if the scene and the app are in a state where the UI of the scene is
// available to show sign-in related prompts.
- (BOOL)isUIAvailableToPrompt {
  if (self.sceneState.appState.initStage < InitStageFinal) {
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
    // have |self.sceneState.presentingModalOverlay| == YES which will stop
    // them from handling the policy as well. For example, this stops the scene
    // from rehandling the forced sign-in policy when foregrounded.
    return NO;
  }

  return YES;
}

@end
