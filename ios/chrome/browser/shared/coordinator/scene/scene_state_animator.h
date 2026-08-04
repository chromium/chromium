// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_ANIMATOR_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_ANIMATOR_H_

#import <Foundation/Foundation.h>

// During profile switching, it is possible that an animation is displayed
// over the SceneState until the transition is complete. In that case the
// object responsible should implement this protocol to allow cancellation
// of the animation if the Profile initialisation needs to present wait for
// the user to interact with some mandatory interactive step.
@protocol SceneStateAnimator <NSObject>

// Cancel any in progress animation. The animation can be restarted with
// the -restartAnimation method.
- (void)cancelAnimation;

// Restart the animation if it has been cancelled. Does nothing if the
// animation has not been cancelled before.
- (void)restartAnimation;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_ANIMATOR_H_
