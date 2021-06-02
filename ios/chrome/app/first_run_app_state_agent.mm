// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/first_run_app_state_agent.h"

#import "base/logging.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#include "ios/chrome/app/application_delegate/startup_information.h"
#include "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_observer.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FirstRunAppAgent () <AppStateObserver>

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

// The scene that is chosen for presenting the FRE on.
@property(nonatomic, strong) SceneState* presentingSceneState;

@end

@implementation FirstRunAppAgent

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

  [self showFirstRun:self.presentingSceneState];
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

  [self showFirstRun:sceneState];
}

#pragma mark - internal

- (void)showFirstRun:(SceneState*)sceneState {
  DCHECK(self.appState.initStage == InitStageFirstRun);
  // There must be a designated presenting scene before showing the first run
  // UI.
  DCHECK(self.presentingSceneState);

  if (base::FeatureList::IsEnabled(kEnableFREUIModuleIOS)) {
    [sceneState.controller showFirstRunUI];
  } else {
    [sceneState.controller showLegacyFirstRunUI];
  }
}

@end
