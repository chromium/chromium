// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/first_run_app_state_agent.h"

#import "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#include "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/first_run/first_run_coordinator.h"
#import "ios/chrome/browser/ui/first_run/first_run_screen_provider.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/first_run/orientation_limiting_navigation_controller.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_observer.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FirstRunAppAgent () <AppStateObserver,
                                PolicyWatcherBrowserAgentObserving,
                                FirstRunCoordinatorDelegate,
                                SceneStateObserver>

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

@property(nonatomic, weak)
    WelcomeToChromeViewController* welcomeToChromeController;

// The scene that is chosen for presenting the FRE on.
@property(nonatomic, strong) SceneState* presentingSceneState;

// Coordinator of the new First Run UI.
@property(nonatomic, strong) FirstRunCoordinator* firstRunCoordinator;

// The current browser interface of the scene that presents the FRE UI.
@property(nonatomic, weak) id<BrowserInterface> presentingInterface;

// Main browser used for browser operations that are not related to UI
// (e.g., authentication).
@property(nonatomic, assign) Browser* mainBrowser;

@end

@implementation FirstRunAppAgent {
  // UI blocker used while the FRE UI is shown in the scene controlled by this
  // object.
  std::unique_ptr<ScopedUIBlocker> _firstRunUIBlocker;

  // Observer for the sign-out policy changes.
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
}

- (void)dealloc {
  [_appState removeObserver:self];
}

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self.firstRunCoordinator stop];

  [self tearDownPolicyWatcher];

  [sceneState removeObserver:self];
  self.presentingSceneState = nil;
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    willTransitionToInitStage:(InitStage)nextInitStage {
  if (nextInitStage != InitStageNormalUI) {
    return;
  }

  // Determine whether the app has to go through startup at first run before
  // starting the UI initialization to make the information available on time.
  self.appState.startupInformation.isFirstRun =
      ShouldPresentFirstRunExperience();
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (self.appState.initStage == InitStageFirstRun) {
    [self handleFirstRunStage];
  }
  // Important: do not add code after this block because its purpose is to
  // clear `self` when not needed anymore.
  if (previousInitStage == InitStageFirstRun) {
    // Nothing left to do; clean up.
    [self.appState removeAgent:self];
  }
}

- (void)handleFirstRunStage {
  if (!self.appState.startupInformation.isFirstRun) {
    // Skip the FRE because it wasn't determined to be needed.
    [self.appState queueTransitionToNextInitStage];
    return;
  }

  // Cannot show the FRE UI immediately because there is no scene state to
  // present from.
  if (!self.presentingSceneState) {
    return;
  }

  [self showFirstRunUI];
}

- (void)appState:(AppState*)appState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  // Select the first scene that the app declares as initialized to present
  // the FRE UI on.
  self.presentingSceneState = sceneState;
  [self.presentingSceneState addObserver:self];

  self.presentingInterface =
      self.presentingSceneState.interfaceProvider.currentInterface;
  self.mainBrowser =
      self.presentingSceneState.interfaceProvider.mainInterface.browser;

  if (self.appState.initStage != InitStageFirstRun) {
    return;
  }

  if (!self.appState.startupInformation.isFirstRun) {
    // Skip the FRE because it wasn't determined to be needed.
    return;
  }

  [self showFirstRunUI];
}

#pragma mark - Getters and Setters

- (id<BrowserInterface>)presentingInterface {
  if (_presentingInterface) {
    // Check that the current interface hasn't changed because it must not be
    // changed during FRE.
    DCHECK(self.presentingSceneState.interfaceProvider.currentInterface ==
           _presentingInterface);
  }

  return _presentingInterface;
}

#pragma mark - internal

- (void)setUpPolicyWatcher {
  _policyWatcherObserverBridge =
      std::make_unique<PolicyWatcherBrowserAgentObserverBridge>(self);

  PolicyWatcherBrowserAgent* policyWatcherAgent =
      PolicyWatcherBrowserAgent::FromBrowser(self.mainBrowser);

  // Sanity check that there is a PolicyWatcherBrowserAgent agent stashed in
  // the browser. This considers that the main browser for the scene was
  // initialized before showing the FRE, which should always be the case.
  DCHECK(policyWatcherAgent);

  policyWatcherAgent->AddObserver(_policyWatcherObserverBridge.get());
}

- (void)tearDownPolicyWatcher {
  if (!_policyWatcherObserverBridge) {
    return;
  }

  PolicyWatcherBrowserAgent::FromBrowser(self.mainBrowser)
      ->RemoveObserver(_policyWatcherObserverBridge.get());
  _policyWatcherObserverBridge = nil;
}

- (void)showFirstRunUI {
  DCHECK(self.appState.initStage == InitStageFirstRun);

  // There must be a designated presenting scene before showing the first run
  // UI.
  DCHECK(self.presentingSceneState);
  DCHECK(self.mainBrowser);

  [self setUpPolicyWatcher];

  if (base::FeatureList::IsEnabled(kEnableFREUIModuleIOS) ||
      fre_field_trial::GetNewMobileIdentityConsistencyFRE() !=
          NewMobileIdentityConsistencyFRE::kOld) {
    [self showNewFirstRunUI];
  } else {
    [self showLegacyFirstRunUI];
  }
}

// Presents the first run UI to the user.
- (void)showLegacyFirstRunUI {
  DCHECK(![self.presentingSceneState.controller isPresentingSigninView]);

  // This should not be necessary because now it's an app-level object doing
  // this.
  DCHECK(!_firstRunUIBlocker);
  _firstRunUIBlocker =
      std::make_unique<ScopedUIBlocker>(self.presentingSceneState);
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleFirstRunUIWillFinish)
             name:kChromeFirstRunUIWillFinishNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleFirstRunUIDidFinish)
             name:kChromeFirstRunUIDidFinishNotification
           object:nil];

  id<ApplicationCommands, BrowsingDataCommands> welcomeHandler =
      static_cast<id<ApplicationCommands, BrowsingDataCommands>>(
          self.mainBrowser->GetCommandDispatcher());

  WelcomeToChromeViewController* welcomeToChrome =
      [[WelcomeToChromeViewController alloc]
          initWithBrowser:self.presentingInterface.browser
              mainBrowser:self.mainBrowser
                presenter:self.presentingInterface.syncPresenter
               dispatcher:welcomeHandler];
  self.welcomeToChromeController = welcomeToChrome;
  UINavigationController* navController =
      [[OrientationLimitingNavigationController alloc]
          initWithRootViewController:welcomeToChrome];
  [navController setModalTransitionStyle:UIModalTransitionStyleCrossDissolve];
  navController.modalPresentationStyle = UIModalPresentationFullScreen;
  CGRect appFrame = [[UIScreen mainScreen] bounds];
  [[navController view] setFrame:appFrame];
  [self.presentingInterface.viewController presentViewController:navController
                                                        animated:NO
                                                      completion:nil];
}

// Shows the new first run UI.
- (void)showNewFirstRunUI {
  DCHECK(!_firstRunUIBlocker);
  _firstRunUIBlocker =
      std::make_unique<ScopedUIBlocker>(self.presentingSceneState);

  FirstRunScreenProvider* provider = [[FirstRunScreenProvider alloc] init];

  self.firstRunCoordinator = [[FirstRunCoordinator alloc]
      initWithBaseViewController:self.presentingInterface.bvc
                         browser:self.mainBrowser
                  screenProvider:provider];
  self.firstRunCoordinator.delegate = self;
  [self.firstRunCoordinator start];
}

- (void)handleFirstRunUIWillFinish {
  DCHECK(self.appState.initStage == InitStageFirstRun);

  _firstRunUIBlocker.reset();
  [self tearDownPolicyWatcher];
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIWillFinishNotification
              object:nil];
}

// All of this can be triggered from will transition from init stage, or did
// transition to init stage, once the rest is done.
- (void)handleFirstRunUIDidFinish {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIDidFinishNotification
              object:nil];

  self.welcomeToChromeController = nil;

  [self.appState queueTransitionToNextInitStage];
}

#pragma mark - FirstRunCoordinatorDelegate

- (void)willFinishPresentingScreens {
  [self handleFirstRunUIWillFinish];

  [self.firstRunCoordinator stop];
}

- (void)didFinishPresentingScreens {
  [self.appState queueTransitionToNextInitStage];
}

#pragma mark - PolicyWatcherBrowserAgentObserving

- (void)policyWatcherBrowserAgentNotifySignInDisabled:
    (PolicyWatcherBrowserAgent*)policyWatcher {
  auto signinInterrupted = ^{
    policyWatcher->SignInUIDismissed();
  };

  if (self.welcomeToChromeController) {
    [self.welcomeToChromeController
        interruptSigninCoordinatorWithCompletion:signinInterrupted];
  }
}

@end
