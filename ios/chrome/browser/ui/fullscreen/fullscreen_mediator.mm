// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"

#import "base/check_op.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_resizer.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/web/public/web_state.h"

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
  if (model_->IsForceFullscreenMode()) {
    return;
  }
  // Instruct the model to ignore the remainder of the current scroll when
  // starting this animator.  This prevents the toolbar from immediately being
  // hidden if AnimateModelReset() is called while a scroll view is
  // decelerating.
  model_->IgnoreRemainderOfCurrentScroll();
  AnimateWithStyle(FullscreenAnimatorStyle::EXIT_FULLSCREEN);
}

void FullscreenMediator::ForceEnterFullscreen() {
  model_->ForceEnterFullscreen();
}

void FullscreenMediator::ExitFullscreenWithoutAnimation() {
  model_->ResetForNavigation();
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
  // Changes in the toolbar heights modifies the visible viewport so the WebView
  // needs to be resized as needed.
  [resizer_ updateForCurrentState];
}

void FullscreenMediator::FullscreenModelProgressUpdated(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  // Stops the animation only if there is a current animation running.
  if (animator_ && animator_.state == UIViewAnimatingStateActive) {
    StopAnimating(true /* update_model */);
  }
  for (auto& observer : observers_) {
    observer.FullscreenProgressUpdated(controller_, model_->progress());
  }
  if (should_record_metrics_) {
    if (model_->progress() == 0) {
      base::RecordAction(base::UserMetricsAction("MobileFullscreenEntered"));
    } else if (model_->progress() == 1) {
      base::RecordAction(base::UserMetricsAction("MobileFullscreenExited"));
    }
    should_record_metrics_ = false;
  }

  [resizer_ updateForCurrentState];
}

void FullscreenMediator::FullscreenModelEnabledStateChanged(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  // Stops the animation only if there is a current animation running.
  if (animator_ && animator_.state == UIViewAnimatingStateActive) {
    StopAnimating(true /* update_model */);
  }
  for (auto& observer : observers_) {
    observer.FullscreenEnabledStateChanged(controller_, model->enabled());
  }
}

void FullscreenMediator::FullscreenModelScrollEventStarted(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  start_progress_ = model_->progress();
  StopAnimating(true /* update_model */);
  // Show the toolbars if the user begins a scroll past the bottom edge of the
  // screen and the toolbars have been fully collapsed.
  if (model_->enabled() && model_->is_scrolled_to_bottom() &&
      AreCGFloatsEqual(model_->progress(), 0.0) &&
      model_->can_collapse_toolbar()) {
    ExitFullscreen();
  }
}

void FullscreenMediator::FullscreenModelScrollEventEnded(
    FullscreenModel* model) {
  DCHECK_EQ(model_, model);
  should_record_metrics_ = true;
  FullscreenAnimatorStyle animatorStyle;
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    AnimateWithStyle(model_->progress() >= 0.5
                         ? FullscreenAnimatorStyle::EXIT_FULLSCREEN
                         : FullscreenAnimatorStyle::ENTER_FULLSCREEN);
  } else {
    // Compute the direction to ensure to not enter fullscreen when the website
    // is not long enough to have more than 0.5 progress and do not enter
    // fullscreen when we scroll up.
    float direction = model_->progress() - start_progress_;
    animatorStyle = animator_.style;
    if (model_->enabled() && model_->is_scrolled_to_bottom() &&
        AreCGFloatsEqual(model_->progress(), 0.0) &&
        model_->can_collapse_toolbar()) {
      animatorStyle = FullscreenAnimatorStyle::EXIT_FULLSCREEN;
      base::RecordAction(
          base::UserMetricsAction("MobileFullscreenExitedBottomReached"));
    } else if (model_->progress() >= 0.5) {
      animatorStyle = FullscreenAnimatorStyle::EXIT_FULLSCREEN;
    } else if (direction < 0) {
      animatorStyle = FullscreenAnimatorStyle::ENTER_FULLSCREEN;
    }
    AnimateWithStyle(animatorStyle);
  }
}

void FullscreenMediator::FullscreenModelWasReset(FullscreenModel* model) {
  // Stop any in-progress animations.  Don't update the model because this
  // callback occurs after the model's state is reset, and updating the model
  // the with active animator's current value would overwrite the reset value.
  StopAnimating(false /* update_model */);
  // Update observers for the reset progress value and set the inset range in
  // case this is a new WebState.
  for (auto& observer : observers_) {
    observer.FullscreenViewportInsetRangeChanged(
        controller_, controller_->GetMinViewportInsets(),
        controller_->GetMaxViewportInsets());
    observer.FullscreenProgressUpdated(controller_, model_->progress());
  }

  [resizer_ updateForCurrentState];
}

void FullscreenMediator::AnimateWithStyle(FullscreenAnimatorStyle style) {
  if (animator_ && animator_.style == style)
    return;
  StopAnimating(true);
  DCHECK(!animator_);

  // Early return if there is no progress change.
  CGFloat start_progress = model_->progress();
  CGFloat final_progress = GetFinalFullscreenProgressForAnimation(style);
  if (AreCGFloatsEqual(start_progress, final_progress))
    return;

  // Create the animator and set up its completion block.
  animator_ = [[FullscreenAnimator alloc] initWithStartProgress:start_progress
                                                          style:style];
  base::WeakPtr<FullscreenMediator> weak_mediator = weak_factory_.GetWeakPtr();
  [animator_ addAnimations:^{
    // Updates the WebView frame during the animation to have it animated.
    FullscreenMediator* mediator = weak_mediator.get();
    if (mediator)
      [mediator->resizer_ forceToUpdateToProgress:final_progress];
  }];
  [animator_ addCompletion:^(UIViewAnimatingPosition finalPosition) {
    DCHECK_EQ(finalPosition, UIViewAnimatingPositionEnd);
    FullscreenMediator* mediator = weak_mediator.get();
    if (!mediator)
      return;
    mediator->model_->AnimationEndedWithProgress(final_progress);
    mediator->animator_ = nil;

    for (auto& observer : mediator->observers_) {
      observer.FullscreenDidAnimate(mediator->controller_, style);
    }
  }];

  // Notify observers that the animation will occur.
  for (auto& observer : observers_) {
    observer.FullscreenWillAnimate(controller_, animator_);
  }

  // Only start the animator if animations have been added.
  if (animator_.hasAnimations) {
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

void FullscreenMediator::ResizeHorizontalInsets() {
  for (auto& observer : observers_) {
    observer.ResizeHorizontalInsets(controller_);
  }
}
