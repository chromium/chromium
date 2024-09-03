// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_STATE_OBSERVER_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_STATE_OBSERVER_H_

#import <Foundation/Foundation.h>

@class ProfileState;
@class SceneState;

enum class ProfileInitStage;

@protocol ProfileStateObserver <NSObject>

@optional

// Called when a scene is connected.
- (void)profileState:(ProfileState*)profileState
      sceneConnected:(SceneState*)sceneState;

// Called when the first scene initializes its UI.
- (void)profileState:(ProfileState*)profileState
    firstSceneHasInitializedUI:(SceneState*)sceneState;

// Called when Scene with activation level SceneActivationLevelForegroundActive
// is available.
- (void)profileState:(ProfileState*)profileState
    sceneDidBecomeActive:(SceneState*)sceneState;

// Called when the profile state is about to transition to `nextInitState`. The
// init stage of the profile state at that moment is still `fromInitStage`.
- (void)profileState:(ProfileState*)profileState
    willTransitionToInitStage:(ProfileInitStage)nextInitStage
                fromInitStage:(ProfileInitStage)fromInitStage;

// Called right after the profile stafe is transitioned out of to the
// `fromInitStage`. The init stage of the profile state at that moment is
// `nextInitStage`.
- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_STATE_OBSERVER_H_
