// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/first_run_app_state_agent.h"

#import "base/logging.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/browser/browser_view/ui_bundled/browser_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"

@interface FirstRunAppAgent () <AppStateObserver,
                                FirstRunCoordinatorDelegate,
                                SceneStateObserver>

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

// The scene that is chosen for presenting the FRE on.
@property(nonatomic, strong) SceneState* presentingSceneState;

// Coordinator of the First Run UI.
@property(nonatomic, strong) FirstRunCoordinator* firstRunCoordinator;

// The current browser interface of the scene that presents the FRE UI.
@property(nonatomic, weak) id<BrowserProvider> presentingInterface;

// Main browser used for browser operations that are not related to UI
// (e.g., authentication).
@property(nonatomic, assign) Browser* mainBrowser;

@end

@implementation FirstRunAppAgent {
  // UI blocker used while the FRE UI is shown in the scene controlled by this
  // object.
  std::unique_ptr<ScopedUIBlocker> _firstRunUIBlocker;
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
  self.firstRunCoordinator = nil;

  [sceneState removeObserver:self];
  self.presentingSceneState = nil;
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (self.appState.initStage == AppInitStage::kFirstRun) {
    [self handleFirstRunStage];
  }
  // Important: do not add code after this block because its purpose is to
  // clear `self` when not needed anymore.
  if (previousInitStage == AppInitStage::kFirstRun) {
    if (self.appState.startupInformation.isFirstRun) {
      [self unlockInterfaceOrientation];
    }
    // Clean up.
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
      self.presentingSceneState.browserProviderInterface.currentBrowserProvider;
  self.mainBrowser = self.presentingSceneState.browserProviderInterface
                         .mainBrowserProvider.browser;

  if (self.appState.initStage != AppInitStage::kFirstRun) {
    return;
  }

  if (!self.appState.startupInformation.isFirstRun) {
    // Skip the FRE because it wasn't determined to be needed.
    return;
  }

  [self showFirstRunUI];
}

#pragma mark - Getters and Setters

- (id<BrowserProvider>)presentingInterface {
  if (_presentingInterface) {
    // Check that the current interface hasn't changed because it must not be
    // changed during FRE.
    DCHECK(self.presentingSceneState.browserProviderInterface
               .currentBrowserProvider == _presentingInterface);
  }

  return _presentingInterface;
}

#pragma mark - internal

- (void)showFirstRunUI {
  DCHECK(self.appState.initStage == AppInitStage::kFirstRun);

  // There must be a designated presenting scene before showing the first run
  // UI.
  DCHECK(self.presentingSceneState);
  DCHECK(self.mainBrowser);

  DCHECK(!_firstRunUIBlocker);
  _firstRunUIBlocker =
      std::make_unique<ScopedUIBlocker>(self.presentingSceneState);

  // TODO(crbug.com/343699504): Remove pre-fetching capabilities once these are
  // loaded in iSL.
  RunSystemCapabilitiesPrefetch(
      ChromeAccountManagerServiceFactory::GetForProfile(
          self.mainBrowser->GetProfile())
          ->GetAllIdentities());

  FirstRunScreenProvider* provider = [[FirstRunScreenProvider alloc]
      initForProfile:self.mainBrowser->GetProfile()];

  self.firstRunCoordinator = [[FirstRunCoordinator alloc]
      initWithBaseViewController:self.presentingInterface.viewController
                         browser:self.mainBrowser
                  screenProvider:provider];
  self.firstRunCoordinator.delegate = self;
  [self.firstRunCoordinator start];
}

// The FRE only displays in "portrait" on iPhone. When the FRE is done, iOS
// must be notified that the supported interface orientations have changed.
- (void)unlockInterfaceOrientation {
  [self.presentingInterface
          .viewController setNeedsUpdateOfSupportedInterfaceOrientations];
}

#pragma mark - FirstRunCoordinatorDelegate

- (void)didFinishFirstRun {
  DCHECK(self.appState.initStage == AppInitStage::kFirstRun);
  _firstRunUIBlocker.reset();
  [self.firstRunCoordinator stop];
  self.firstRunCoordinator = nil;
  [self.appState queueTransitionToNextInitStage];
}

@end
