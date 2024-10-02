// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/safe_mode_app_state_agent.h"

#import "base/ios/ios_util.h"
#import "base/version.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/safe_mode_app_state_agent+private.h"
#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"

@implementation SafeModeAppAgent {
  // Multiwindow UI blocker used when safe mode is active to only show the safe
  // mode UI on one window.
  std::unique_ptr<ScopedUIBlocker> _safeModeBlocker;

  // The app state the agent is connected to.
  __weak AppState* _appState;
}

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [_appState addObserver:self];
}

#pragma mark - SafeModeCoordinatorDelegate Implementation

- (void)coordinatorDidExitSafeMode:(SafeModeCoordinator*)coordinator {
  DCHECK(coordinator);
  [self stopSafeMode];
  // Transition out of Safe Mode init stage to the next stage. Tell the appState
  // that the app is resuming from safe mode.
  _appState.resumingFromSafeMode = YES;
  [_appState queueTransitionToNextInitStage];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Don't try to trigger Safe Mode when the scene is not yet active on the
  // foreground.
  if (level < SceneActivationLevelForegroundActive) {
    return;
  }
  // Don't try to trigger Safe Mode when the app has already passed the safe
  // mode stage when the scene transitions to foreground. If the init stage is
  // still Safe Mode at this moment it means that safe mode has to be triggered.
  if (_appState.initStage != AppInitStage::kSafeMode) {
    return;
  }
  // Don't try to show the safe mode UI on multiple scenes; one scene is
  // sufficient.
  if (self.firstSceneHasActivated) {
    return;
  }
  self.firstSceneHasActivated = YES;

  [self startSafeMode:sceneState];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (_appState.initStage != AppInitStage::kSafeMode) {
    return;
  }
  // Iterate further in the init stages when safe mode isn't needed; stop
  // and switch the app to safe mode otherwise.
  if ([SafeModeCoordinator shouldStart]) {
    return;
  }

  [_appState queueTransitionToNextInitStage];
}

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
}

#pragma mark - Internals

- (void)startSafeMode:(SceneState*)sceneState {
  DCHECK(sceneState);
  DCHECK(!_safeModeBlocker);

  self.safeModeCoordinator =
      [[SafeModeCoordinator alloc] initWithWindow:sceneState.window];
  self.safeModeCoordinator.delegate = self;

  // Activate the main window, which will prompt the views to load.
  [sceneState.window makeKeyAndVisible];

  [self.safeModeCoordinator start];

  if (base::ios::IsMultipleScenesSupported()) {
    _safeModeBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
  }
}

- (void)stopSafeMode {
  if (_safeModeBlocker) {
    _safeModeBlocker.reset();
  }
  self.safeModeCoordinator = nil;
}

@end
