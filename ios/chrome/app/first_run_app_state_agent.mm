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
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/location_permissions_field_trial.h"
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

namespace {
// Histogram enum values for showing the experiment arms of the location
// permissions experiment. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
enum class LocationPermissionsUI {
  // The First Run native location prompt was not shown.
  kFirstRunPromptNotShown = 0,
  // The First Run location permissions modal was shown.
  kFirstRunModal = 1,
  // kMaxValue should share the value of the highest enumerator.
  kMaxValue = kFirstRunModal,
};
}

@interface FirstRunAppAgent () <AppStateObserver,
                                PolicyWatcherBrowserAgentObserving>

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

@property(nonatomic, weak)
    WelcomeToChromeViewController* welcomeToChromeController;

// The scene that is chosen for presenting the FRE on.
@property(nonatomic, strong) SceneState* presentingSceneState;

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
  if (self.appState.initStage != InitStageFirstRun) {
    return;
  }

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

  if (self.appState.initStage != InitStageFirstRun) {
    return;
  }

  if (!self.appState.startupInformation.isFirstRun) {
    // Skip the FRE because it wasn't determined to be needed.
    return;
  }

  [self showFirstRunUI];
}

#pragma mark - internal

- (void)setUpPolicyWatcher {
  _policyWatcherObserverBridge =
      std::make_unique<PolicyWatcherBrowserAgentObserverBridge>(self);

  Browser* mainBrowser =
      self.presentingSceneState.interfaceProvider.mainInterface.browser;
  PolicyWatcherBrowserAgent* policyWatcherAgent =
      PolicyWatcherBrowserAgent::FromBrowser(mainBrowser);

  // Sanity check that there is a PolicyWatcherBrowserAgent agent stashed in
  // the browser. This considers that the main browser for the scene was
  // initialized before showing the FRE, which should always be the case.
  DCHECK(policyWatcherAgent);

  policyWatcherAgent->AddObserver(_policyWatcherObserverBridge.get());
}

- (void)tearDownPolicyWatcher {
  PolicyWatcherBrowserAgent::FromBrowser(
      self.presentingSceneState.interfaceProvider.mainInterface.browser)
      ->RemoveObserver(_policyWatcherObserverBridge.get());
}

- (void)showFirstRunUI {
  if (![self ignoreFirstRunStageForTesting]) {
    DCHECK(self.appState.initStage == InitStageFirstRun);
  }

  // There must be a designated presenting scene before showing the first run
  // UI.
  DCHECK(self.presentingSceneState);

  [self setUpPolicyWatcher];

  if (base::FeatureList::IsEnabled(kEnableFREUIModuleIOS)) {
    [self.presentingSceneState.controller showFirstRunUI];
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
  // Register for the first run dismissal notification to reset
  // |sceneState.presentingFirstRunUI| flag;
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

  Browser* browser =
      self.presentingSceneState.interfaceProvider.mainInterface.browser;
  id<ApplicationCommands, BrowsingDataCommands> welcomeHandler =
      static_cast<id<ApplicationCommands, BrowsingDataCommands>>(
          browser->GetCommandDispatcher());

  WelcomeToChromeViewController* welcomeToChrome =
      [[WelcomeToChromeViewController alloc]
          initWithBrowser:browser
                presenter:self.presentingSceneState.interfaceProvider
                              .currentInterface.bvc
               dispatcher:welcomeHandler];
  self.welcomeToChromeController = welcomeToChrome;
  UINavigationController* navController =
      [[OrientationLimitingNavigationController alloc]
          initWithRootViewController:welcomeToChrome];
  [navController setModalTransitionStyle:UIModalTransitionStyleCrossDissolve];
  navController.modalPresentationStyle = UIModalPresentationFullScreen;
  CGRect appFrame = [[UIScreen mainScreen] bounds];
  [[navController view] setFrame:appFrame];
  self.presentingSceneState.presentingFirstRunUI = YES;
  [self.presentingSceneState.interfaceProvider.currentInterface.viewController
      presentViewController:navController
                   animated:NO
                 completion:nil];
}

- (void)handleFirstRunUIWillFinish {
  DCHECK(self.presentingSceneState.presentingFirstRunUI);
  _firstRunUIBlocker.reset();
  self.presentingSceneState.presentingFirstRunUI = NO;
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

  [self maybePromptLocationWithSystemAlert];

  if (![self ignoreFirstRunStageForTesting]) {
    [self.appState queueTransitionToNextInitStage];
  }
}

- (void)logLocationPermissionsExperimentForGroupShown:
    (LocationPermissionsUI)experimentGroup {
  UMA_HISTOGRAM_ENUMERATION("IOS.LocationPermissionsUI", experimentGroup);
}

- (void)maybePromptLocationWithSystemAlert {
  if (!location_permissions_field_trial::IsInRemoveFirstRunPromptGroup() &&
      !location_permissions_field_trial::IsInFirstRunModalGroup()) {
    [self logLocationPermissionsExperimentForGroupShown:
              LocationPermissionsUI::kFirstRunPromptNotShown];
    // As soon as First Run has finished, give OmniboxGeolocationController an
    // opportunity to present the iOS system location alert.
    [[OmniboxGeolocationController sharedInstance] triggerSystemPrompt];
  } else if (location_permissions_field_trial::
                 IsInRemoveFirstRunPromptGroup()) {
    // If in RemoveFirstRunPrompt group, the system prompt will be delayed until
    // the site requests location information.
    [[OmniboxGeolocationController sharedInstance]
        systemPromptSkippedForNewUser];
  }
}

// TODO(crbug.com/1210246): Remove this hook once the chrome test fixture is
// adapted to startup testing.
//
// Determines whether the First Run stage has to be ignored because of
// testing. When testing with first_run_egtest.mm, the First Run UI is
// manually triggered after the browser is fully initialized, in which
// case the code that assumes that the app is in the First Run stage when
// showing the FRE has to be ignored to avoid unexepted failures (e.g., DCHECKs,
// unexpected init stage transition).
- (BOOL)ignoreFirstRunStageForTesting {
  return tests_hook::DisableFirstRun();
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
