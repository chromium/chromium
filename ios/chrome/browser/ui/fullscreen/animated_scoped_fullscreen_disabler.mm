// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/animated_scoped_fullscreen_disabler.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/common/material_timing.h"

AnimatedScopedFullscreenDisabler::AnimatedScopedFullscreenDisabler(
    FullscreenController* controller)
    : controller_(controller) {
  DCHECK(controller_);
  controller_->AddObserver(this);
}

AnimatedScopedFullscreenDisabler::~AnimatedScopedFullscreenDisabler() {
  if (controller_) {
    if (disabling_) {
      controller_->DecrementDisabledCounter();
    }
    controller_->RemoveObserver(this);
    controller_ = nullptr;
  }
}

void AnimatedScopedFullscreenDisabler::StartAnimation() {
  // StartAnimation() should be idempotent, so early return if this disabler has
  // already incremented the disabled counter.
  if (disabling_ || !controller_) {
    return;
  }
  disabling_ = true;

  if (controller_->IsEnabled()) {
    // Increment the disabled counter in an animation block if the controller is
    // not already disabled.

    base::WeakPtr<AnimatedScopedFullscreenDisabler> weak_ptr =
        weak_factory_.GetWeakPtr();

    base::RepeatingClosure animation_started = base::BindRepeating(
        &AnimatedScopedFullscreenDisabler::OnAnimationStart, weak_ptr);

    [UIView animateWithDuration:kMaterialDuration1
                     animations:^{
                       if (!animation_started.IsCancelled()) {
                         animation_started.Run();
                       }
                     }
                     completion:nil];
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
