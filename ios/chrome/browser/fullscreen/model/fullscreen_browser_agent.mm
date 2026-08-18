// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"

#import <algorithm>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_constants.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/material_timing.h"

namespace {

// Updates the fractional `progress` of the fullscreen UI layer by interpreting
// a `scroll` distance. Evaluates the `scroll` as a percentage of the total
// compressible space (`delta`) and clamps the result between 0.0 (fullscreen)
// and 1.0 (not fullscreen) to prevent overscroll distortion.
void UpdateProgress(CGFloat& progress, CGFloat scroll, CGFloat delta) {
  if (delta == 0) {
    return;
  }

  CGFloat incremental_progress = scroll / delta;
  progress = std::clamp<CGFloat>(progress - incremental_progress, 0, 1);
}

// Animation duration and initial spring velocity for an eased transition.
struct SpringAnimationParams {
  base::TimeDelta duration = kEasedTransitionMaxDuration;
  CGFloat initial_spring_velocity = 0.0;
};

// Calculates the animation duration and initial spring velocity for an eased
// transition based on the transition trigger, starting progress, and scroll
// velocity.
SpringAnimationParams CalculateSpringAnimationParams(
    FullscreenTransition transition,
    FullscreenModeTransitionTrigger trigger,
    CGFloat start_progress,
    CGFloat scroll_velocity) {
  if (trigger !=
      FullscreenModeTransitionTrigger::kUserInitiatedFinishedByCode) {
    return {.duration = base::Seconds(kMaterialDuration3),
            .initial_spring_velocity = 0.0};
  }

  CGFloat progress_delta =
      (transition == FullscreenTransition::kEnterFullscreen)
          ? start_progress
          : (1.0 - start_progress);
  CGFloat remaining_distance = progress_delta * kEasedTransitionScrollDistance;

  if (remaining_distance <= 0 || scroll_velocity <= 0) {
    return {.duration = kEasedTransitionMaxDuration,
            .initial_spring_velocity = 0.0};
  }

  CGFloat initial_spring_velocity = scroll_velocity / remaining_distance;
  base::TimeDelta calculated_duration =
      base::Seconds(remaining_distance / scroll_velocity);
  base::TimeDelta duration =
      std::clamp(calculated_duration, kEasedTransitionMinDuration,
                 kEasedTransitionMaxDuration);
  return {.duration = duration,
          .initial_spring_velocity = initial_spring_velocity};
}

// Returns the target progress for a given transition.
constexpr CGFloat TargetProgressForTransition(FullscreenTransition transition) {
  return (transition == FullscreenTransition::kEnterFullscreen) ? 0.0 : 1.0;
}

// Returns the settled FullscreenState corresponding to a completed transition.
constexpr FullscreenState SettledStateForTransition(
    FullscreenTransition transition) {
  return (transition == FullscreenTransition::kEnterFullscreen)
             ? FullscreenState::kUICollapsed
             : FullscreenState::kUIExpanded;
}
}  // namespace

FullscreenBrowserAgent::FullscreenBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}

FullscreenBrowserAgent::~FullscreenBrowserAgent() {
  for (auto& observer : observers_) {
    observer.WillShutDown(this);
  }
}

void FullscreenBrowserAgent::AddObserver(
    FullscreenBrowserAgentObserver* observer) {
  observers_.AddObserver(observer);
}

void FullscreenBrowserAgent::RemoveObserver(
    FullscreenBrowserAgentObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FullscreenBrowserAgent::IncrementalScroll(CGFloat amount,
                                               CGFloat velocity,
                                               PassKey) {
  if (!IsEnabled() || IsForceFullscreen()) {
    return;
  }

  if (IsFullscreenEasedTransitionsEnabled() && is_animating_) {
    return;
  }

  scroll_velocity_ = velocity;

  CGFloat pre_scroll_top_progress = top_progress_;
  CGFloat pre_scroll_bottom_progress = bottom_progress_;

  if (IsFullscreenEasedTransitionsEnabled()) {
    UpdateProgress(top_progress_, amount, kEasedTransitionScrollDistance);
    if (settled_state_ == FullscreenState::kUIExpanded) {
      top_progress_ =
          std::max(top_progress_, kEnterFullscreenProgressThreshold);
    } else if (settled_state_ == FullscreenState::kUICollapsed) {
      top_progress_ = std::min(top_progress_, kExitFullscreenProgressThreshold);
    }
    bottom_progress_ = top_progress_;
  } else {
    CGFloat top_delta = max_insets_.top - min_insets_.top;
    UpdateProgress(top_progress_, amount, top_delta);
    CGFloat bottom_delta = max_insets_.bottom - min_insets_.bottom;
    if (bottom_delta > 0) {
      UpdateProgress(bottom_progress_, amount, bottom_delta);
    } else {
      bottom_progress_ = top_progress_;
    }
  }

  if (pre_scroll_top_progress == top_progress_ &&
      pre_scroll_bottom_progress == bottom_progress_) {
    return;
  }

  if (top_progress_ == 0.0 && bottom_progress_ == 0.0) {
    base::UmaHistogramEnumeration(
        kEnterFullscreenModeTransitionTriggerHistogram,
        FullscreenModeTransitionTrigger::kUserControlled);
  } else if (top_progress_ == 1.0 && bottom_progress_ == 1.0) {
    base::UmaHistogramEnumeration(
        kExitFullscreenModeTransitionTriggerHistogram,
        FullscreenModeTransitionTrigger::kUserControlled);
  }

  NotifyObserversOfUpdatedState();
}

void FullscreenBrowserAgent::EnterFullscreen(
    PassKey pass_key,
    FullscreenModeTransitionTrigger trigger,
    bool animated) {
  base::UmaHistogramEnumeration(kEnterFullscreenModeTransitionTriggerHistogram,
                                trigger);
  UpdateProgressAndBroadcast(FullscreenTransition::kEnterFullscreen, trigger,
                             animated);
}

void FullscreenBrowserAgent::ExitFullscreen(
    PassKey pass_key,
    FullscreenModeTransitionTrigger trigger,
    bool animated) {
  base::UmaHistogramEnumeration(kExitFullscreenModeTransitionTriggerHistogram,
                                trigger);
  UpdateProgressAndBroadcast(FullscreenTransition::kExitFullscreen, trigger,
                             animated);
}

void FullscreenBrowserAgent::UpdateProgressAndBroadcast(
    FullscreenTransition transition,
    FullscreenModeTransitionTrigger trigger,
    bool animated) {
  CGFloat target_progress = TargetProgressForTransition(transition);
  if (top_progress_ == target_progress && bottom_progress_ == target_progress) {
    return;
  }

  CGFloat start_progress = top_progress_;
  top_progress_ = target_progress;
  bottom_progress_ = target_progress;

  if (!animated) {
    is_animating_ = false;
    settled_state_ = SettledStateForTransition(transition);
    NotifyObserversOfUpdatedState();
    NotifyFullscreenDidTransition(transition);
    return;
  }

  is_animating_ = true;
  base::TimeDelta duration = base::Seconds(kMaterialDuration1);
  animation_initial_velocity_ = 0.0;

  if (IsFullscreenEasedTransitionsEnabled()) {
    auto params = CalculateSpringAnimationParams(
        transition, trigger, start_progress, scroll_velocity_);
    duration = params.duration;
    animation_initial_velocity_ = params.initial_spring_velocity;
  }
  scroll_velocity_ = 0.0;

  auto update_state = base::CallbackToBlock(
      base::BindOnce(&FullscreenBrowserAgent::NotifyObserversOfUpdatedState,
                     weak_ptr_factory_.GetWeakPtr(), duration));
  auto completion_block = base::CallbackToBlock(
      base::BindOnce(&FullscreenBrowserAgent::AnimationDidComplete,
                     weak_ptr_factory_.GetWeakPtr(), transition));

  [UIView animateWithDuration:duration.InSecondsF()
                        delay:0.0
       usingSpringWithDamping:1.0
        initialSpringVelocity:animation_initial_velocity_
                      options:UIViewAnimationOptionAllowUserInteraction
                   animations:update_state
                   completion:completion_block];
}

void FullscreenBrowserAgent::NotifyObserversOfUpdatedState(
    base::TimeDelta duration) {
  // Prevent reentrant calls that can occur when layout changes or scroll
  // events are synchronously triggered while notifying observers.
  if (updating_insets_) {
    return;
  }
  animation_duration_ = duration;
  updating_insets_ = true;
  UIEdgeInsets old_insets = insets_;
  insets_ = UIEdgeInsetsZero;
  for (auto& observer : observers_) {
    observer.WillUpdateState(this);
  }

  // Apply keyboard height as overlapping.
  if (keyboard_obscured_inset_ > 0) {
    insets_.bottom = std::max(insets_.bottom, keyboard_obscured_inset_);
  }

  updating_insets_ = false;

  if (!UIEdgeInsetsEqualToEdgeInsets(old_insets, insets_)) {
    for (auto& observer : observers_) {
      observer.DidUpdateState(this);
    }
  }
  animation_duration_ = base::TimeDelta();
  animation_initial_velocity_ = 0.0;
}

void FullscreenBrowserAgent::AnimationDidComplete(
    FullscreenTransition transition,
    bool finished) {
  is_animating_ = false;
  if (finished) {
    settled_state_ = SettledStateForTransition(transition);
    NotifyFullscreenDidTransition(transition);
  }
}

void FullscreenBrowserAgent::NotifyFullscreenDidTransition(
    FullscreenTransition transition) {
  for (auto& observer : observers_) {
    observer.FullscreenDidTransition(this, transition);
  }
}

bool FullscreenBrowserAgent::IsEnabled() const {
  return disabled_count_ == 0;
}

FullscreenState FullscreenBrowserAgent::State() const {
  if (top_progress_ == 0.0 && bottom_progress_ == 0.0) {
    return FullscreenState::kUICollapsed;
  }
  if (top_progress_ == 1.0 && bottom_progress_ == 1.0) {
    return FullscreenState::kUIExpanded;
  }
  return FullscreenState::kInProgress;
}

void FullscreenBrowserAgent::IncrementDisabledCounter(PassKey pass_key,
                                                      bool animated) {
  disabled_count_++;
  if (disabled_count_ == 1) {
    ExitFullscreen(pass_key, FullscreenModeTransitionTrigger::kForcedByCode,
                   animated);
  }
}

void FullscreenBrowserAgent::DecrementDisabledCounter(PassKey pass_key) {
  if (disabled_count_ > 0) {
    disabled_count_--;
    if (disabled_count_ == 0 && IsForceFullscreen()) {
      EnterFullscreen(pass_key, FullscreenModeTransitionTrigger::kForcedByCode,
                      /*animated=*/true);
    }
  }
}

bool FullscreenBrowserAgent::IsForceFullscreen() const {
  return !forced_features_.empty();
}

void FullscreenBrowserAgent::ForceFullscreen(PassKey pass_key,
                                             bool enable,
                                             ForceFullscreenFeature feature) {
  const bool was_forced = IsForceFullscreen();
  if (enable) {
    forced_features_.Put(feature);
  } else {
    forced_features_.Remove(feature);
  }
  if (was_forced == IsForceFullscreen() || !IsEnabled()) {
    return;
  }
  if (IsForceFullscreen()) {
    EnterFullscreen(pass_key, FullscreenModeTransitionTrigger::kForcedByCode,
                    /*animated=*/true);
  } else {
    ExitFullscreen(pass_key, FullscreenModeTransitionTrigger::kForcedByCode,
                   /*animated=*/true);
  }
}

void FullscreenBrowserAgent::ExitForceFullscreen(PassKey pass_key) {
  if (!IsForceFullscreen()) {
    return;
  }
  forced_features_.Clear();
  if (IsEnabled()) {
    ExitFullscreen(pass_key, FullscreenModeTransitionTrigger::kForcedByCode,
                   /*animated=*/true);
  }
}

void FullscreenBrowserAgent::InvalidateInsetRange() {
  invalidating_inset_range_ = true;
  min_insets_ = UIEdgeInsetsZero;
  max_insets_ = UIEdgeInsetsZero;

  updating_obscured_insets_ = true;
  for (auto& observer : observers_) {
    observer.WillUpdateObscuredInsetRange(this);
  }

  // Apply keyboard height as overlapping.
  if (keyboard_obscured_inset_ > 0) {
    min_insets_.bottom = std::max(min_insets_.bottom, keyboard_obscured_inset_);
    max_insets_.bottom = std::max(max_insets_.bottom, keyboard_obscured_inset_);
  }

  updating_obscured_insets_ = false;

  for (auto& observer : observers_) {
    observer.DidUpdateObscuredInsetRange(this);
  }

  NotifyObserversOfUpdatedState();
  invalidating_inset_range_ = false;
}

void FullscreenBrowserAgent::AddObscuredInsetRange(UIRectEdge edge,
                                                   CGFloat min,
                                                   CGFloat max) {
  CHECK(updating_obscured_insets_);
  if (edge == UIRectEdgeTop) {
    min_insets_.top += min;
    max_insets_.top += max;
  } else if (edge == UIRectEdgeBottom) {
    min_insets_.bottom += min;
    max_insets_.bottom += max;
  } else if (edge == UIRectEdgeLeft) {
    min_insets_.left += min;
    max_insets_.left += max;
  } else if (edge == UIRectEdgeRight) {
    min_insets_.right += min;
    max_insets_.right += max;
  }
}

void FullscreenBrowserAgent::AddObscuredInset(UIRectEdge edge, CGFloat amount) {
  CHECK(updating_insets_);
  if (edge == UIRectEdgeTop) {
    insets_.top += amount;
  } else if (edge == UIRectEdgeBottom) {
    insets_.bottom += amount;
  } else if (edge == UIRectEdgeLeft) {
    insets_.left += amount;
  } else if (edge == UIRectEdgeRight) {
    insets_.right += amount;
  }
}

void FullscreenBrowserAgent::SetKeyboardObscuredInset(CGFloat inset) {
  if (keyboard_obscured_inset_ == inset) {
    return;
  }
  keyboard_obscured_inset_ = inset;
  InvalidateInsetRange();
}
