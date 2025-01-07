// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/change_profile_animator.h"

#import <objc/runtime.h>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@interface ChangeProfileAnimator () <ProfileStateObserver>
@end

@implementation ChangeProfileAnimator {
  __weak UIViewController* _viewController;
  __weak SceneState* _sceneState;
  ProfileInitStage _minimumInitStage;
  ChangeProfileCompletion _completion;
}

- (instancetype)initWithViewController:(UIViewController*)viewController {
  if ((self = [super init])) {
    _viewController = viewController;
  }
  return self;
}

- (void)startAnimation {
  // TODO(crbug.com/385091409): implement an animation. This is not possible
  // for the moment because the UIWindow's rootViewController is directly
  // accessed and changed by the UI code.
}

- (void)waitForSceneState:(SceneState*)sceneState
         toInitReachStage:(ProfileInitStage)initStage
               completion:(ChangeProfileCompletion)completion {
  DCHECK(completion);
  DCHECK(sceneState.profileState);

  _sceneState = sceneState;
  _completion = std::move(completion);
  _minimumInitStage = initStage;

  ProfileState* profileState = sceneState.profileState;
  if (profileState.initStage < _minimumInitStage) {
    // Attach self as an associated object of the ProfileState. This ensures
    // that the ChangeProfileAnimator will not be destroyed until the profile
    // is ready or destroyed.
    objc_setAssociatedObject(profileState, [self associationKey], self,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [profileState addObserver:self];
    return;
  }

  [self profileReachedInitStage];
}

#pragma mark ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage >= _minimumInitStage) {
    [self profileReachedInitStage];

    // Stop observing the ProfileState and detach self. This may cause the
    // object to be deallocated, thus nothing should happen after this line.
    [profileState removeObserver:self];
    objc_setAssociatedObject(profileState, [self associationKey], nil,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
}

#pragma mark Private methods

// Stops the animation (if it has been started). No-op if the animation
// has not been started or already stopped.
- (void)stopAnimation {
  // TODO(crbug.com/385091409): implement an animation. This is not possible
  // for the moment because the UIWindow's rootViewController is directly
  // accessed and changed by the UI code.
}

- (void)profileReachedInitStage {
  [self stopAnimation];

  // Ensure that the completion is always invoked asynchronously, even if
  // the profile was already in the expected stage. This does not strongly
  // captures SceneState since _sceneDelete is a weak pointer.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(_completion), _sceneState));
}

// Returns a unique pointer that can be used to attach the current instance
// to another object as an Objective-C associated object. This pointer has
// to be different for each instance of ChangeProfileAnimator.
- (void*)associationKey {
  return &_minimumInitStage;
}

@end
