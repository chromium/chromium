// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"

#import <algorithm>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/fullscreen/public/fullscreen_metrics.h"
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

void FullscreenBrowserAgent::IncrementalScroll(CGFloat amount, PassKey) {
  CGFloat pre_scroll_top_progress = top_progress_;
  CGFloat pre_scroll_bottom_progress = bottom_progress_;

  CGFloat top_delta = max_insets_.top - min_insets_.top;
  UpdateProgress(top_progress_, amount, top_delta);
  CGFloat bottom_delta = max_insets_.bottom - min_insets_.bottom;
  UpdateProgress(bottom_progress_, amount, bottom_delta);

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
  UpdateProgressAndBroadcast(FullscreenTransition::kEnterFullscreen, animated);
}

void FullscreenBrowserAgent::ExitFullscreen(
    PassKey pass_key,
    FullscreenModeTransitionTrigger trigger,
    bool animated) {
  base::UmaHistogramEnumeration(kExitFullscreenModeTransitionTriggerHistogram,
                                trigger);
  UpdateProgressAndBroadcast(FullscreenTransition::kExitFullscreen, animated);
}

void FullscreenBrowserAgent::UpdateProgressAndBroadcast(
    FullscreenTransition transition,
    bool animated) {
  CGFloat top_progress =
      (transition == FullscreenTransition::kEnterFullscreen) ? 0.0 : 1.0;
  CGFloat bottom_progress =
      (transition == FullscreenTransition::kEnterFullscreen) ? 0.0 : 1.0;

  if (top_progress_ == top_progress && bottom_progress_ == bottom_progress) {
    return;
  }
  top_progress_ = top_progress;
  bottom_progress_ = bottom_progress;

  if (animated) {
    base::TimeDelta duration = base::Seconds(kMaterialDuration1);
    auto update_state = base::CallbackToBlock(
        base::BindOnce(&FullscreenBrowserAgent::NotifyObserversOfUpdatedState,
                       weak_ptr_factory_.GetWeakPtr(), duration));
    auto completion_block = base::CallbackToBlock(
        base::BindOnce(&FullscreenBrowserAgent::AnimationDidComplete,
                       weak_ptr_factory_.GetWeakPtr(), transition));
    [UIView animateWithDuration:kMaterialDuration1
                     animations:update_state
                     completion:completion_block];
  } else {
    NotifyObserversOfUpdatedState();
    NotifyFullscreenDidTransition(transition);
  }
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
}

void FullscreenBrowserAgent::AnimationDidComplete(
    FullscreenTransition transition,
    bool finished) {
  if (finished) {
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

void FullscreenBrowserAgent::DecrementDisabledCounter(PassKey) {
  if (disabled_count_ > 0) {
    disabled_count_--;
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
