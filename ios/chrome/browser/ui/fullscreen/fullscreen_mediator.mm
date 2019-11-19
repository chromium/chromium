// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_resizer.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullscreenMediator::FullscreenMediator(FullscreenController* controller,
                                       FullscreenModel* model)
    : controller_(controller),
      model_(model),
      resizer_([[FullscreenWebViewResizer alloc] initWithModel:model]) {
  DCHECK(controller_);
  DCHECK(model_);
  model_->AddObserver(this);
}

FullscreenMediator::~FullscreenMediator() {
  // Disconnect() is expected to be called before deallocation.
  DCHECK(!controller_);
  DCHECK(!model_);
}

void FullscreenMediator::SetWebState(web::WebState* webState) {
  resizer_.webState = webState;
}

void FullscreenMediator::SetIsBrowserTraitCollectionUpdating(bool updating) {
  if (updating_browser_trait_collection_ == updating)
    return;
  updating_browser_trait_collection_ = updating;
  if (updating_browser_trait_collection_) {
    resizer_.compensateFrameChangeByOffset = NO;
    scrolled_to_top_during_trait_collection_updates_ =
        model_->is_scrolled_to_top();
  } else {
    resizer_.compensateFrameChangeByOffset = YES;
    if (scrolled_to_top_during_trait_collection_updates_) {
      // If the content was scrolled to the top when the trait collection began
      // updating, changes in toolbar heights may cause the top of the page to
      // become hidden.  Ensure that the page remains scrolled to the top after
      // the trait collection finishes updating.
      web::WebState* web_state = resizer_.webState;
      if (web_state)
        MoveContentBelowHeader(web_state->GetWebViewProxy(), model_);
      scrolled_to_top_during_trait_collection_updates_ = false;
    }
  }
}

void FullscreenMediator::EnterFullscreen() {
  if (model_->enabled())
    AnimateWithStyle(FullscreenAnimatorStyle::ENTER_FULLSCREEN);
}

void FullscreenMediator::ExitFullscreen() {
  // Instruct the model to ignore the remainder of the current scroll when
  // starting this animator.  This prevents the toolbar from immediately being
  // hidden if AnimateModelReset() is called while a scroll view is
  // decelerating.
  model_->IgnoreRemainderOfCurrentScroll();
  AnimateWithStyle(FullscreenAnimatorStyle::EXIT_FULLSCREEN);
}

void FullscreenMediator::Disconnect() {
  for (auto& observer : observers_) {
    observer.FullscreenControllerWillShutDown(controller_);
  }
  resizer_.webState = nullptr;
  resizer_ = nil;
  [animator_ stopAnimation:YES];
  animator_ = nil;
  model_->RemoveObserver(this);
  model_ = nullptr;
  controller_ = nullptr;
}

void FullscreenMediator::FullscreenModelToolbarHeightsUpdated(
    FullscreenModel* model) {
  for (auto& observer : observers_) {
    observer.FullscreenViewportInsetRangeChanged(controller_,
                                                 model_->min_toolbar_insets(),
                                                 model_->max_toolbar_insets());
  }
}

void FullscreenMediator::FullscreenModelProgressUpdated(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  StopAnimating(true /* update_model */);
  for (auto& observer : observers_) {
    observer.FullscreenProgressUpdated(controller_, model_->progress());
  }

  [resizer_ updateForCurrentState];
}

void FullscreenMediator::FullscreenModelEnabledStateChanged(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  StopAnimating(true /* update_model */);
  for (auto& observer : observers_) {
    observer.FullscreenEnabledStateChanged(controller_, model->enabled());
  }
}

void FullscreenMediator::FullscreenModelScrollEventStarted(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  StopAnimating(true /* update_model */);
  // Show the toolbars if the user begins a scroll past the bottom edge of the
  // screen and the toolbars have been fully collapsed.
  if (model_->is_scrolled_to_bottom() &&
      AreCGFloatsEqual(model_->progress(), 0.0) &&
      model_->can_collapse_toolbar()) {
    ExitFullscreen();
  }
}

void FullscreenMediator::FullscreenModelScrollEventEnded(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  AnimateWithStyle(model_->progress() >= 0.5
                       ? FullscreenAnimatorStyle::EXIT_FULLSCREEN
                       : FullscreenAnimatorStyle::ENTER_FULLSCREEN);
}

void FullscreenMediator::FullscreenModelWasReset(FullscreenModel* model) {
  // Stop any in-progress animations.  Don't update the model because this
  // callback occurs after the model's state is reset, and updating the model
  // the with active animator's current value would overwrite the reset value.
  StopAnimating(false /* update_model */);
  // Update observers for the reset progress value.
  for (auto& observer : observers_) {
    observer.FullscreenProgressUpdated(controller_, model_->progress());
  }

  [resizer_ updateForCurrentState];
}

void FullscreenMediator::AnimateWithStyle(FullscreenAnimatorStyle style) {
  if (animator_ && animator_.style == style)
    return;
  StopAnimating(true);
  DCHECK(!animator_);

  // Create the animator and set up its completion block.
  animator_ =
      [[FullscreenAnimator alloc] initWithStartProgress:model_->progress()
                                                  style:style];
  __weak FullscreenAnimator* weakAnimator = animator_;
  FullscreenModel** modelPtr = &model_;
  [animator_ addAnimations:^{
    // Updates the WebView frame during the animation to have it animated.
    [resizer_ forceToUpdateToProgress:animator_.finalProgress];
  }];
  [animator_ addCompletion:^(UIViewAnimatingPosition finalPosition) {
    DCHECK_EQ(finalPosition, UIViewAnimatingPositionEnd);
    if (!weakAnimator || !*modelPtr)
      return;
    model_->AnimationEndedWithProgress(
        [weakAnimator progressForAnimatingPosition:finalPosition]);
    animator_ = nil;
  }];

  // Notify observers that the animation will occur.
  for (auto& observer : observers_) {
    observer.FullscreenWillAnimate(controller_, animator_);
  }

  // Only start the animator if animations have been added and it has a non-zero
  // progress change.
  if (animator_.hasAnimations &&
      !AreCGFloatsEqual(animator_.startProgress, animator_.finalProgress)) {
    [animator_ startAnimation];
  } else {
    animator_ = nil;
  }
}

void FullscreenMediator::StopAnimating(bool update_model) {
  if (!animator_)
    return;

  DCHECK_EQ(animator_.state, UIViewAnimatingStateActive);
  if (update_model)
    model_->AnimationEndedWithProgress(animator_.currentProgress);
  [animator_ stopAnimation:YES];
  animator_ = nil;
}
