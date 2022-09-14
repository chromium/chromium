// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"

#import "base/callback.h"
#import "base/check.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/common/material_timing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - AnimatedScopedFullscreenDisablerObserverListContainer

// An Objective-C container used to store observers.  This is used to allow
// correct memory management for use in UIView animation blocks.
@interface AnimatedScopedFullscreenDisablerObserverListContainer : NSObject {
  // The AnimatedScopedFullscreenDisablerObservers.
  base::ObserverList<AnimatedScopedFullscreenDisablerObserver>::Unchecked
      _observers;
}

// The disabler passed on initialization.
@property(nonatomic, readonly) AnimatedScopedFullscreenDisabler* disabler;

// Designated initializer for a container containing `disabler`'s observer list.
- (instancetype)initWithDisabler:(AnimatedScopedFullscreenDisabler*)disabler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Adds and removes observers.
- (void)addObserver:(AnimatedScopedFullscreenDisablerObserver*)observer;
- (void)removeObserver:(AnimatedScopedFullscreenDisablerObserver*)observer;

// Notifies observers when the animation starts and finishes.
- (void)onAnimationStarted;
- (void)onAnimationFinished;
- (void)onDisablerDestroyed;

@end

@implementation AnimatedScopedFullscreenDisablerObserverListContainer
@synthesize disabler = _disabler;

- (instancetype)initWithDisabler:(AnimatedScopedFullscreenDisabler*)disabler {
  if (self = [super init]) {
    _disabler = disabler;
    DCHECK(_disabler);
  }
  return self;
}

- (const base::ObserverList<
    AnimatedScopedFullscreenDisablerObserver>::Unchecked&)observers {
  return _observers;
}

- (void)addObserver:(AnimatedScopedFullscreenDisablerObserver*)observer {
  _observers.AddObserver(observer);
}

- (void)removeObserver:(AnimatedScopedFullscreenDisablerObserver*)observer {
  _observers.RemoveObserver(observer);
}

- (void)onAnimationStarted {
  for (auto& observer : _observers) {
    observer.FullscreenDisablingAnimationDidStart(_disabler);
  }
}

- (void)onAnimationFinished {
  for (auto& observer : _observers) {
    observer.FullscreenDisablingAnimationDidFinish(_disabler);
  }
}

- (void)onDisablerDestroyed {
  for (auto& observer : _observers) {
    observer.AnimatedFullscreenDisablerDestroyed(_disabler);
  }
}

@end

#pragma mark - AnimatedScopedFullscreenDisabler

AnimatedScopedFullscreenDisabler::AnimatedScopedFullscreenDisabler(
    FullscreenController* controller)
    : controller_(controller) {
  DCHECK(controller_);
  controller_->AddObserver(this);
  observer_list_container_ =
      [[AnimatedScopedFullscreenDisablerObserverListContainer alloc]
          initWithDisabler:this];
}

AnimatedScopedFullscreenDisabler::~AnimatedScopedFullscreenDisabler() {
  if (controller_) {
    if (disabling_)
      controller_->DecrementDisabledCounter();
    controller_->RemoveObserver(this);
    controller_ = nullptr;
  }
  [observer_list_container_ onDisablerDestroyed];
}

void AnimatedScopedFullscreenDisabler::AddObserver(
    AnimatedScopedFullscreenDisablerObserver* observer) {
  [observer_list_container_ addObserver:observer];
}

void AnimatedScopedFullscreenDisabler::RemoveObserver(
    AnimatedScopedFullscreenDisablerObserver* observer) {
  [observer_list_container_ removeObserver:observer];
}

void AnimatedScopedFullscreenDisabler::StartAnimation() {
  // StartAnimation() should be idempotent, so early return if this disabler has
  // already incremented the disabled counter.
  if (disabling_ || !controller_)
    return;
  disabling_ = true;

  if (controller_->IsEnabled()) {
    // Increment the disabled counter in an animation block if the controller is
    // not already disabled.
    [observer_list_container_ onAnimationStarted];

    base::WeakPtr<AnimatedScopedFullscreenDisabler> weak_ptr =
        weak_factory_.GetWeakPtr();

    base::RepeatingClosure animation_started = base::BindRepeating(
        &AnimatedScopedFullscreenDisabler::OnAnimationStart, weak_ptr);

    base::RepeatingClosure animation_completed = base::BindRepeating(
        &AnimatedScopedFullscreenDisabler::OnAnimationCompletion, weak_ptr);

    [UIView animateWithDuration:kMaterialDuration1
        animations:^{
          if (!animation_started.IsCancelled())
            animation_started.Run();
        }
        completion:^(BOOL finished) {
          if (!animation_completed.IsCancelled())
            animation_completed.Run();
        }];
  } else {
    // If `controller_` is already disabled, no animation is necessary.
    controller_->IncrementDisabledCounter();
  }
}

void AnimatedScopedFullscreenDisabler::FullscreenControllerWillShutDown(
    FullscreenController* controller) {
  DCHECK_EQ(controller, controller_);
  controller_->RemoveObserver(this);
  controller_ = nullptr;
}

void AnimatedScopedFullscreenDisabler::OnAnimationStart() {
  if (controller_)
    controller_->IncrementDisabledCounter();
}

void AnimatedScopedFullscreenDisabler::OnAnimationCompletion() {
  [observer_list_container_ onAnimationFinished];
}
