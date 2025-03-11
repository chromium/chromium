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
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"

namespace {

// Invokes `continuation` if `weak_scene_state` is not nil and the UI is
// enabled (to avoid crashing if the `SceneState` is disconnected while
// the callback was pending).
void InvokeChangeProfileContinuation(ChangeProfileContinuation continuation,
                                     __weak SceneState* weak_scene_state) {
  if (SceneState* strong_scene_state = weak_scene_state) {
    if (strong_scene_state.UIEnabled) {
      std::move(continuation).Run(strong_scene_state, base::DoNothing());
    }
  }
}

}  // namespace

@interface ChangeProfileAnimator () <ProfileStateObserver, SceneStateObserver>
@end

@implementation ChangeProfileAnimator {
  __weak UIViewController* _viewController;
  __weak SceneState* _sceneState;
  ProfileInitStage _minimumInitStage;
  ChangeProfileContinuation _continuation;
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
         toReachInitStage:(ProfileInitStage)initStage
             continuation:(ChangeProfileContinuation)continuation {
  DCHECK(continuation);
  DCHECK(sceneState.profileState);
  DCHECK_GE(initStage, ProfileInitStage::kUIReady);

  _sceneState = sceneState;
  _continuation = std::move(continuation);
  _minimumInitStage = initStage;

  // Attach self as an associated object of the SceneState. This ensures
  // that the ChangeProfileAnimator will live as long as the SceneState.
  objc_setAssociatedObject(_sceneState, [self associationKey], self,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);

  // Observe both the SceneState and the ProfileState to detect when the
  // profile and the UI initialisations are complete. ProfileState calls
  // -profileState:didTransitionToInitStage:fromInitStage: when adding
  // an observer, so there is no need to check the initState here.
  [_sceneState addObserver:self];
  [_sceneState.profileState addObserver:self];
}

#pragma mark ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  [self initialisationProgressed];
}

#pragma mark SceneStateObserver

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  [self initialisationProgressed];
}

#pragma mark Private methods

// Stops the animation (if it has been started). No-op if the animation
// has not been started or already stopped.
- (void)stopAnimation {
  // TODO(crbug.com/385091409): implement an animation. This is not possible
  // for the moment because the UIWindow's rootViewController is directly
  // accessed and changed by the UI code.
}

// Called when the initialisation progressed (i.e. the state of any of the
// observed object changed).
- (void)initialisationProgressed {
  if (!_sceneState.UIEnabled) {
    return;
  }

  ProfileState* profileState = _sceneState.profileState;
  if (profileState.initStage < _minimumInitStage) {
    return;
  }

  [self stopAnimation];

  // Ensure that the completion is always invoked asynchronously, even if
  // the profile was already in the expected stage. This does not strongly
  // captures SceneState since _sceneDelete is a weak pointer.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&InvokeChangeProfileContinuation,
                                std::move(_continuation), _sceneState));

  // Stop observing the ProfileState and the SceneState.
  [profileState removeObserver:self];
  [_sceneState removeObserver:self];

  // Uninstall self as an associated object for the SceneState, as the wait
  // is complete and the object not needed anymore.
  objc_setAssociatedObject(_sceneState, [self associationKey], nil,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

// Returns a unique pointer that can be used to attach the current instance
// to another object as an Objective-C associated object. This pointer has
// to be different for each instance of ChangeProfileAnimator.
- (void*)associationKey {
  return &_minimumInitStage;
}

@end
