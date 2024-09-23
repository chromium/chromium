// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

@implementation ObservingSceneAgent

#pragma mark - SceneAgent

+ (instancetype)agentFromScene:(SceneState*)sceneState {
  for (id agent in sceneState.connectedAgents) {
    if ([agent isMemberOfClass:[self class]]) {
      return agent;
    }
  }

  return nil;
}

+ (BOOL)allowsMultipleAgentsOfSameTypePerScene {
  return NO;
}

#pragma mark - public

- (void)setSceneState:(SceneState*)sceneState {
  DCHECK(!_sceneState);

  // Sanity check: most of the time only one object of the same type per scene
  // is required, and multiple of them is a programming error.
  if (![[self class] allowsMultipleAgentsOfSameTypePerScene]) {
    // Verify that no other agent of this class is added to the scene state.
    // Note that this object is already added, and it's ok.
    for (id agent in sceneState.connectedAgents) {
      if ([agent isKindOfClass:[self class]]) {
        DCHECK(agent == self);
      }
    }
  }

  _sceneState = sceneState;
  [sceneState addObserver:self];
}

#pragma mark - private

- (void)dealloc {
  [_sceneState removeObserver:self];
}

@end
