// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_OBSERVING_SCENE_STATE_AGENT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_OBSERVING_SCENE_STATE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"

// A scene agent that acts as a scene state observer.
// Since most agents are also scene state observers, this is a convenience base
// class that provides universally useful functionality for scene agents.
@interface ObservingSceneAgent : NSObject <SceneAgent, SceneStateObserver>

// Scene state this agent serves and observes.
// See also allowsMultipleAgentsOfSameTypePerScene.
@property(nonatomic, weak) SceneState* sceneState;

// Returns the agent of this class iff one is already added to `sceneState`.
+ (instancetype)agentFromScene:(SceneState*)sceneState;

// You can override this in your subclass. The default is NO.
// When calling -sceneState, if this is NO, the setter will DCHECK if
// another agent of its class is already added to the scene.
+ (BOOL)allowsMultipleAgentsOfSameTypePerScene;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_OBSERVING_SCENE_STATE_AGENT_H_
