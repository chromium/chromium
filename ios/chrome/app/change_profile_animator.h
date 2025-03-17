// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_CHANGE_PROFILE_ANIMATOR_H_
#define IOS_CHROME_APP_CHANGE_PROFILE_ANIMATOR_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback_forward.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/app/profile/profile_init_stage.h"

@class MDCOverlayWindow;
@class SceneState;

// Object responsible for animating the transition between profile for a
// given scene for MainController.
@interface ChangeProfileAnimator : NSObject

- (instancetype)initWithWindow:(MDCOverlayWindow*)window
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the transition animation. This method may not be called (if the
// profile won't change though it is not yet ready).
- (void)startAnimation;

// Starts waiting for the SceneState's owning ProfileState to reach the
// corresponding initialisation stage.
//
// When the initialisation stage is reached, it will stop the animation
// (if started) and then invoke the completion with the SceneState.
//
// Once this is called the animator will ensure it is kept alive until the
// ProfileState reach the stage or is deallocated.
- (void)waitForSceneState:(SceneState*)sceneState
         toReachInitStage:(ProfileInitStage)initStage
             continuation:(ChangeProfileContinuation)continuation;

@end

#endif  // IOS_CHROME_APP_CHANGE_PROFILE_ANIMATOR_H_
