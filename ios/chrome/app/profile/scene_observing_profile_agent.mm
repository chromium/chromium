// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/scene_observing_profile_agent.h"

#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@implementation SceneObservingProfileAgent

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
      sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];

  const SceneActivationLevel activationLevel = sceneState.activationLevel;
  if (activationLevel > SceneActivationLevelUnattached) {
    [self sceneState:sceneState transitionedToActivationLevel:activationLevel];
  }
}

@end
