// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/enterprise_app_agent.h"

#include "base/check.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/enterprise_loading_screen_view_controller.h"
#import "ios/chrome/browser/ui/main/scene_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface EnterpriseAppAgent () <SceneStateObserver>

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

@end

@implementation EnterpriseAppAgent

- (void)dealloc {
  for (SceneState* scene in _appState.connectedScenes) {
    [scene removeObserver:self];
  }
  [_appState removeObserver:self];
}

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];

  for (SceneState* scene in appState.connectedScenes) {
    [scene addObserver:self];
  }
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (appState.initStage == InitStageEnterprise) {
    // TODO(crbug.com/1178818): When fully implemented, the transition to the
    // next init stage will only happen when the policy is actually fetched.
    if ([self shouldShowEnterpriseLoadScreen]) {
      for (SceneState* scene in appState.connectedScenes) {
        if (scene.activationLevel > SceneActivationLevelBackground) {
          [self showUIInScene:scene];
        }
      }

      // TODO(crbug.com/1178818): remove this when the fetching is implemented.
      // This is just debug code to simulate a long fetch.
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW, (int64_t)(12 * NSEC_PER_SEC)),
          dispatch_get_main_queue(), ^{
            [self.appState queueTransitionToNextInitStage];
          });

    } else {
      [self.appState queueTransitionToNextInitStage];
    }
  }

  if (previousInitStage == InitStageEnterprise) {
    // Nothing left to do; clean up.
    [self.appState removeAgent:self];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (self.appState.initStage == InitStageEnterprise &&
      level > SceneActivationLevelBackground) {
    [self showUIInScene:sceneState];
  }
}

#pragma mark - private

- (void)showUIInScene:(SceneState*)sceneState {
  if ([sceneState.window.rootViewController
          isKindOfClass:[EnterpriseLoadScreenViewController class]]) {
    return;
  }

  sceneState.window.rootViewController =
      [[EnterpriseLoadScreenViewController alloc] init];
  [sceneState.window makeKeyAndVisible];
}

- (BOOL)shouldShowEnterpriseLoadScreen {
  // TODO(crbug.com/1178818): add actual logic here
  return NO;
}

@end
