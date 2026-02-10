// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/task_updater_scene_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/task_orchestrator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@interface TaskUpdaterSceneAgent () <ProfileStateObserver>
@end

@implementation TaskUpdaterSceneAgent

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  [self.sceneState.profileState addObserver:self];

  // Make sure that the execution stage is updated also if a scene is connected
  // after the ProfileState has reached stage ProfileInitStage::kProfileLoaded
  // or higher.
  if (self.sceneState.profileState.initStage >=
      ProfileInitStage::kProfileLoaded) {
    [self updateToProfileLoaded];
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kProfileLoaded) {
    [self updateToProfileLoaded];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self.sceneState.profileState removeObserver:self];
  [self.sceneState removeObserver:self];
}

#pragma mark - Private

// Updates the scene to TaskExecutionProfileLoaded.
- (void)updateToProfileLoaded {
  [self.sceneState.profileState.appState.taskOrchestrator
      updateToStage:TaskExecutionStage::TaskExecutionProfileLoaded
           forScene:self.sceneState.sceneSessionID];
}

@end
