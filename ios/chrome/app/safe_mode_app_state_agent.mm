// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/safe_mode_app_state_agent.h"

#include "base/ios/ios_util.h"
#include "base/version.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_observer.h"
#import "ios/chrome/browser/ui/safe_mode/safe_mode_coordinator.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SafeModeAppAgent () <SafeModeCoordinatorDelegate,
                                AppStateObserver,
                                SceneStateObserver> {
  // Multiwindow UI blocker used when safe mode is active to only show the safe
  // mode UI on one window.
  std::unique_ptr<ScopedUIBlocker> _safeModeBlocker;
}

// This flag is set when the first scene has activated since the startup, and
// never reset.
@property(nonatomic, assign) BOOL firstSceneHasActivated;

// The app state the agent is connected to.
@property(nonatomic, weak, readonly) AppState* appState;

// Safe mode coordinator. If this is non-nil, the app is displaying the safe
// mode UI.
@property(nonatomic, strong) SafeModeCoordinator* safeModeCoordinator;

@end

@implementation SafeModeAppAgent

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];
}

#pragma mark - SafeModeCoordinatorDelegate Implementation

- (void)coordinatorDidExitSafeMode:(SafeModeCoordinator*)coordinator {
  DCHECK(coordinator);
  [self stopSafeMode];
  // Transition out of Safe Mode init stage to the next stage.
  [self.appState queueTransitionToNextInitStage];
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
  if (self.appState.initStage != InitStageSafeMode) {
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
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (self.appState.initStage != InitStageSafeMode) {
    return;
  }
  // Iterate further in the init stages when safe mode isn't needed; stop
  // and switch the app to safe mode otherwise.
  if ([SafeModeCoordinator shouldStart]) {
    return;
  }

  [self.appState queueTransitionToNextInitStage];
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
