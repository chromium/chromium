// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_ACTIVATION_LEVEL_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_ACTIVATION_LEVEL_H_

// Describes the possible activation level of SceneState.
// This is an iOS 12 compatible version of UISceneActivationState enum.
enum SceneActivationLevel : NSUInteger {
  // The scene is not connected and has no window.
  SceneActivationLevelUnattached = 0,
  // The scene has been disconnected. It also corresponds to
  // UISceneActivationStateUnattached.
  SceneActivationLevelDisconnected,
  // The scene is connected, and has a window associated with it. The window is
  // not visible to the user, except possibly in the app switcher.
  SceneActivationLevelBackground,
  // The scene is connected, and its window is on screen, but it's not active
  // for user input. For example, keyboard events would not be sent to this
  // window.
  SceneActivationLevelForegroundInactive,
  // The scene is connected, has a window, and receives user events.
  SceneActivationLevelForegroundActive,
};

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_ACTIVATION_LEVEL_H_
