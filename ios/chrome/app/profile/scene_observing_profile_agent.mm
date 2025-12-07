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

#pragma mark - SceneStateObserver

// Empty implementation of -sceneState:transitionedToActivationLevel: to
// simplify the implementation of -profileState:sceneConnected: (as the
// method is declared optional in SceneStateObserver, without this empty
// method we would have to check whether the instance responds to the
// corresponding selector).
- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)activationLevel {
}

@end
