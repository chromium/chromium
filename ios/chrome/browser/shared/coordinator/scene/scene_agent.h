// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_AGENT_H_

#import <Foundation/Foundation.h>

@class SceneState;

// Scene agents are objects owned by a scene state and providing some
// scene-scoped function. They can be driven by SceneStateObserver events.
@protocol SceneAgent <NSObject>

@required
// Sets the associated scene state. Called once and only once. Consider using
// this method to add the agent as an observer.
- (void)setSceneState:(SceneState*)scene;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_AGENT_H_
