// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SCENE_IDENTIFIER_MAP_H_
#define IOS_CHROME_APP_SCENE_IDENTIFIER_MAP_H_

#import <Foundation/Foundation.h>

@class SceneState;
@class UISceneSession;

// Class maintaining the mapping between UISceneSession and SceneState
// identifiers for the MainController.
class SceneIdentifierMap {
 public:
  SceneIdentifierMap() = default;

  // Non-copyable, non-moveable.
  SceneIdentifierMap(const SceneIdentifierMap&) = delete;
  SceneIdentifierMap& operator=(const SceneIdentifierMap&) = delete;

  virtual ~SceneIdentifierMap() = default;

  // Marks sessions as discarded for all profiles.
  virtual void OnSessionsDiscarded(NSSet<UISceneSession*>* sessions) = 0;

  // Invoked when the last SceneState is disconnected.
  virtual void OnLastSceneStateDisconnected(SceneState* scene_state) = 0;

  // Assigns an unique identifier to the SceneState on connection.
  //
  // If the SceneIdentifierMap implementation supports reconnecting
  // disconnected session then it must update the -currentOrigin of
  // the SceneState.
  virtual void AssignIdentifierToSceneState(SceneState* scene_state,
                                            UISceneSession* session,
                                            bool is_first_scene) = 0;
};

#endif  // IOS_CHROME_APP_SCENE_IDENTIFIER_MAP_H_
