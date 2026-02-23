// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_STATE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_STATE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state_observer.h"

// C++ interface for objects that care about ProfileState events.
// This mirrors the Objective-C ProfileStateObserver protocol.
class ProfileStateObserver {
 public:
  virtual ~ProfileStateObserver() = default;

  // Called when a scene is connected.
  virtual void OnSceneConnected(ProfileState* profile_state,
                                SceneState* scene_state) {}

  // Called when a scene is disconnected.
  virtual void OnSceneDisconnected(ProfileState* profile_state,
                                   SceneState* scene_state) {}

  // Called when the first scene initializes its UI.
  virtual void OnFirstSceneHasInitializedUI(ProfileState* profile_state,
                                            SceneState* scene_state) {}

  // Called when a scene becomes active.
  virtual void OnSceneDidBecomeActive(ProfileState* profile_state,
                                      SceneState* scene_state) {}

  // Called when the profile state is about to transition to `next_init_stage`.
  virtual void OnProfileStateWillTransitionToInitStage(
      ProfileState* profile_state,
      ProfileInitStage next_init_stage,
      ProfileInitStage from_init_stage) {}

  // Called right after the profile state transitioned out of `from_init_stage`.
  virtual void OnProfileStateDidTransitionToInitStage(
      ProfileState* profile_state,
      ProfileInitStage next_init_stage,
      ProfileInitStage from_init_stage) {}
};

// Bridge object that forwards ProfileStateObserver events to a C++ observer.
@interface ProfileStateObserverBridge : NSObject <ProfileStateObserver>

// Initializer for a bridge that updates `observer`.
- (instancetype)initWithObserver:(ProfileStateObserver*)observer
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Resets the observer. No more observation will be sent. Must be called
// before dealloc.
- (void)resetObserver;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_STATE_OBSERVER_BRIDGE_H_
