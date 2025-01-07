// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_CHANGE_PROFILE_CONTINUATION_H_
#define IOS_CHROME_APP_CHANGE_PROFILE_CONTINUATION_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"

@class SceneState;

// Represents an action to execute after the profile for a SceneState
// has been changed.
@protocol ChangeProfileContinuation <NSObject>

// Invoked when the SceneState has transitioned to the new Profile and
// the profile has reached the ProfileInitStage::kUIReady (or higher)
// stage.
//
// Must call the completion on the current sequence when the action this
// ChangeProfileContinuation represents is complete (e.g. sign-in, ...).
//
// The completion may be called synchronously or asynchronously if the
// operation needs to block (e.g. needs to wait for some other external
// condition before it can resume).
- (void)executeWithSceneState:(SceneState*)sceneState
                   completion:(base::OnceClosure)completion;

@end

#endif  // IOS_CHROME_APP_CHANGE_PROFILE_CONTINUATION_H_
