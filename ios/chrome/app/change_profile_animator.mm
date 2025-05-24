// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/change_profile_animator.h"

#import <MaterialComponents/MaterialOverlayWindow.h>
#import <objc/runtime.h>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"

@interface ChangeProfileAnimation : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithWindow:(MDCOverlayWindow*)window
    NS_DESIGNATED_INITIALIZER;

// Captures a snapshot of the view presented by the window, install it as
// an overlay, and start an animation to blur that view during `duration`.
- (void)blurWithDuration:(base::TimeDelta)duration;

// Removes the snapshot overlay, and remove the blur effect in `duration`.
// If called while the blur animation is in progress, the unblur will wait
// for the blur animation to complete before starting.
- (void)unblurWithDuration:(base::TimeDelta)duration;

@end

namespace {

// Duration for the fade-in and fade-out of the change profile animation.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(250);

// Returns a callback that starts the unblur animation on `animator` with
// a `duration`, or does nothing if `animator` is nil.
void UnblurWithDuration(ChangeProfileAnimation* animator,
                        base::TimeDelta duration) {
  [animator unblurWithDuration:kAnimationDuration];
}

// Invokes `continuation` if `weak_scene_state` is not nil and the UI is
// enabled (to avoid crashing if the `SceneState` is disconnected while
// the callback was pending). Then stops the animation using `animator`
// once the continuation completes.
void InvokeChangeProfileContinuation(ChangeProfileContinuation continuation,
                                     __weak SceneState* weak_scene_state,
                                     base::OnceClosure closure) {
  if (SceneState* strong_scene_state = weak_scene_state) {
    if (strong_scene_state.UIEnabled) {
      std::move(continuation).Run(strong_scene_state, std::move(closure));
    }
  }
}

}  // namespace

@implementation ChangeProfileAnimation {
  // The window on which the animations should be played.
  __weak MDCOverlayWindow* _window;

  // Visual effect view used to animate the blur and unblur animations.
  UIVisualEffectView* _effectView;

  // Snapshot of the old UI captured when the blur animation is started.
  UIView* _snapshotView;

  // Records whether the blur animation is in progress. Used to delay
  // the unblur animation if the profile initialisation was faster than
  // the blur animation.
  BOOL _blurInProgress;

  // Store the duration passed if -unblurWithDuration: was called while
  // the blur animation was still in progress. If it has a value when
  // the blur animation completes, the unblur will start automatically
  // with that duration.
  std::optional<base::TimeDelta> _unblurDuration;
}

- (instancetype)initWithWindow:(MDCOverlayWindow*)window {
  if ((self = [super init])) {
    _window = window;
  }
  return self;
}

- (void)blurWithDuration:(base::TimeDelta)duration {
  UIView* view = _window.rootViewController.view;
  if (!view) {
    return;
  }

  _effectView = [[UIVisualEffectView alloc] initWithEffect:nil];
  _snapshotView = [view snapshotViewAfterScreenUpdates:NO];

  if (!_snapshotView) {
    _snapshotView = [[UIView alloc] initWithFrame:view.frame];
    _snapshotView.backgroundColor = [UIColor whiteColor];
  }

  // Install the snapshot and the effect view as overlays above the UIWindow.
  // The effect initially does nothing, but it is possible to animate it by
  // setting the -effect property in an animation block.
  [_window activateOverlay:_snapshotView withLevel:UIWindowLevelNormal];
  [_window activateOverlay:_effectView withLevel:(UIWindowLevelNormal + 1.0)];

  _blurInProgress = YES;

  // Use `self` to allow the block to retain the object until the animation
  // completes. This is required because the ChangeProfileAnimator drops its
  // reference after starting the fade out.
  [UIView animateWithDuration:duration.InSecondsF()
      animations:^{
        [self blurAnimations];
      }
      completion:^(BOOL) {
        [self blurComplete];
      }];
}

- (void)unblurWithDuration:(base::TimeDelta)duration {
  if (_blurInProgress) {
    _unblurDuration = duration;
    return;
  }

  if (!_snapshotView) {
    return;
  }

  [_window deactivateOverlay:_snapshotView];
  _snapshotView = nil;

  // Use `self` to allow the block to retain the object until the animation
  // completes. This is required because the ChangeProfileAnimator drops its
  // reference after starting the fade out.
  [UIView animateWithDuration:duration.InSecondsF()
      animations:^{
        [self unblurAnimations];
      }
      completion:^(BOOL) {
        [self unblurComplete];
      }];
}

// Performs the view changes to animate as part of the blur animation.
- (void)blurAnimations {
  _effectView.effect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
}

// Invoked when the blur animation is complete. Will invoke the unblur
// animation if it was requested while the blur animation was still in
// progress.
- (void)blurComplete {
  _blurInProgress = NO;
  if (_unblurDuration.has_value()) {
    base::TimeDelta duration = *_unblurDuration;
    _unblurDuration = std::nullopt;

    [self unblurWithDuration:duration];
    return;
  }
}

// Performs the view changes to animate as part of the unblur animation.
- (void)unblurAnimations {
  _effectView.effect = nil;
}

// Invoked when the unblur animation is complete. Should remove all the
// view used for the animations.
- (void)unblurComplete {
  [_window deactivateOverlay:_effectView];
  _effectView = nil;
}

@end

@interface ChangeProfileAnimator () <ProfileStateObserver,
                                     SceneStateAnimator,
                                     SceneStateObserver>
@end

@implementation ChangeProfileAnimator {
  ChangeProfileAnimation* _animation;
  __weak SceneState* _sceneState;
  ProfileInitStage _minimumInitStage;
  ChangeProfileContinuation _continuation;
}

- (instancetype)initWithWindow:(MDCOverlayWindow*)window {
  if ((self = [super init])) {
    if (window) {
      _animation = [[ChangeProfileAnimation alloc] initWithWindow:window];
    }
  }
  return self;
}

- (void)startAnimation {
  [_animation blurWithDuration:kAnimationDuration];
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

  _sceneState.animator = self;

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

#pragma mark SceneStateAnimator

- (void)cancelAnimation {
  [_animation unblurWithDuration:kAnimationDuration];
}

#pragma mark Private methods

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

  // Ensure that the completion is always invoked asynchronously, even if
  // the profile was already in the expected stage and that the animation
  // to unblur the view starts when the continuation is complete.
  //
  // The callback does not strongly retain the SceneState since the ivar
  // is declared as __weak SceneState* and base::BindOnce(...) correctly
  // use a weak pointer for its storage.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&InvokeChangeProfileContinuation,
                                std::move(_continuation), _sceneState,
                                base::BindOnce(&UnblurWithDuration, _animation,
                                               kAnimationDuration)));

  // Stop observing the ProfileState and the SceneState.
  [profileState removeObserver:self];
  [_sceneState removeObserver:self];

  _sceneState.animator = nil;

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
