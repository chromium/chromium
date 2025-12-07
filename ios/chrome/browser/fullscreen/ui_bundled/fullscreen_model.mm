// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model.h"

#import <algorithm>

#import "base/check_op.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_constants.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_metrics.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model_observer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/web/common/features.h"

namespace {

// Default value of the mount the scroll must exceed to begin entering and
// exiting fullscreen when the `kFullscreenScrollThreshold` feature is enabled.
constexpr CGFloat kScrollThresholdDefault = 10;

// Object that increments `counter` by 1 for its lifetime.
class ScopedIncrementer {
 public:
  explicit ScopedIncrementer(size_t* counter) : counter_(counter) {
    ++(*counter_);
  }
  ~ScopedIncrementer() { --(*counter_); }

 private:
  raw_ptr<size_t> counter_;
};
}  // namespace

FullscreenModel::FullscreenModel() {
  UpdateSpeed();
  if (web::features::IsFullscreenScrollThresholdEnabled()) {
    scroll_threshold_ = GetFieldTrialParamByFeatureAsDouble(
        web::features::kFullscreenScrollThreshold,
        web::features::kFullscreenScrollThresholdAmount,
        kScrollThresholdDefault);
  }
}
FullscreenModel::~FullscreenModel() {
  [toolbars_size_ removeObserver:this];
}

void FullscreenModel::AddObserver(FullscreenModelObserver* observer) {
  observers_.AddObserver(observer);
}

void FullscreenModel::RemoveObserver(FullscreenModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FullscreenModel::IncrementDisabledCounter() {
  if (++disabled_counter_ == 1U) {
    ScopedIncrementer disabled_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelEnabledStateChanged(this);
    }
    // Fullscreen observers are expected to show the toolbar when fullscreen is
    // disabled. Update the internal state to match this.
    SetProgress(1.0);
    UpdateBaseOffset();
  }
}

void FullscreenModel::DecrementDisabledCounter() {
  DCHECK_GT(disabled_counter_, 0U);
  if (!--disabled_counter_) {
    ScopedIncrementer enabled_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelEnabledStateChanged(this);
    }
  }
}

void FullscreenModel::ForceEnterFullscreen() {
  SetProgress(0.0);
}

void FullscreenModel::ResetForNavigation() {
  if (IsForceFullscreenMode()) {
    return;
  }
  base::UmaHistogramEnumeration(kExitFullscreenModeTransitionReasonHistogram,
                                FullscreenModeTransitionReason::kForcedByCode);
  progress_ = 1.0;
  scrolling_ = false;
  start_scrolling_time_ = std::nullopt;
  is_scrolling_time_recorded_ = false;
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    base_offset_ = NAN;
  }
  ScopedIncrementer reset_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelWasReset(this);
  }
}

void FullscreenModel::IgnoreRemainderOfCurrentScroll() {
  if (!scrolling_) {
    return;
  }
  ignoring_current_scroll_ = true;
}

void FullscreenModel::AnimationEndedWithProgress(CGFloat progress) {
  DCHECK_GE(progress, 0.0);
  DCHECK_LE(progress, 1.0);
  // Since this is being set by the animator instead of by scroll events, do not
  // broadcast the new progress value.
  progress_ = progress;
}

void FullscreenModel::ToolbarsHeightDidChange() {
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    base_offset_ = NAN;
  }
  ScopedIncrementer toolbar_height_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelToolbarHeightsUpdated(this);
  }
}

CGFloat FullscreenModel::GetCollapsedTopToolbarHeight() const {
  return toolbars_size_ ? toolbars_size_.collapsedTopToolbarHeight : 0.0;
}

CGFloat FullscreenModel::GetExpandedTopToolbarHeight() const {
  return toolbars_size_ ? toolbars_size_.expandedTopToolbarHeight : 0.0;
}

CGFloat FullscreenModel::GetExpandedBottomToolbarHeight() const {
  return toolbars_size_ ? toolbars_size_.expandedBottomToolbarHeight : 0.0;
}

CGFloat FullscreenModel::GetCollapsedBottomToolbarHeight() const {
  return toolbars_size_ ? toolbars_size_.collapsedBottomToolbarHeight : 0.0;
}

void FullscreenModel::SetScrollViewHeight(CGFloat scroll_view_height) {
  scroll_view_height_ = scroll_view_height;
  UpdateDisabledCounterForContentHeight();
}

CGFloat FullscreenModel::GetScrollViewHeight() const {
  return scroll_view_height_;
}

void FullscreenModel::SetContentHeight(CGFloat content_height) {
  content_height_ = content_height;
  UpdateDisabledCounterForContentHeight();
}

CGFloat FullscreenModel::GetContentHeight() const {
  return content_height_;
}

void FullscreenModel::SetTopContentInset(CGFloat top_inset) {
  top_inset_ = top_inset;
}

CGFloat FullscreenModel::GetTopContentInset() const {
  return top_inset_;
}

void FullscreenModel::SetYContentOffset(CGFloat y_content_offset) {
  CGFloat from_offset = y_content_offset_;
  y_content_offset_ = y_content_offset;
  if (y_content_offset_ == from_offset) {
    // The scroll did not change `y_content_offset_` (e.g., reached the bottom
    // of the page).
    SetLastScrollDirection(FullscreenModelScrollDirection::kNone);
  }
  switch (ActionForScrollFromOffset(from_offset)) {
    case ScrollAction::kUpdateBaseOffset:
      UpdateBaseOffset();
      break;
    case ScrollAction::kUpdateProgress:
      // Updates `scroll_direction_` according to the offset.
      if (y_content_offset_ > from_offset) {
        SetLastScrollDirection(FullscreenModelScrollDirection::kDown);
      } else if (y_content_offset_ < from_offset) {
        SetLastScrollDirection(FullscreenModelScrollDirection::kUp);
      }
      UpdateProgress();
      break;
    case ScrollAction::kUpdateBaseOffsetAndProgress:
      CHECK(
          base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
      UpdateBaseOffset();
      UpdateProgress();
      break;
    case ScrollAction::kIgnore:
      // no op.
      break;
  }
}

CGFloat FullscreenModel::GetYContentOffset() const {
  return y_content_offset_;
}

void FullscreenModel::SetScrollViewIsScrolling(bool scrolling) {
  if (scrolling_ == scrolling) {
    return;
  }
  scrolling_ = scrolling;
  if (scrolling_) {
    // Record the start time when the first scroll happened in the current
    // navigation.
    if (!start_scrolling_time_.has_value() && can_collapse_toolbar()) {
      is_scrolling_time_recorded_ = false;
      start_scrolling_time_ = base::TimeTicks::Now();
    }
    // Notify observers that the scroll event has begun.
    ScopedIncrementer scroll_started_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelScrollEventStarted(this);
    }
  } else {
    if (is_scrolled_to_bottom() && !is_scrolling_time_recorded_ &&
        start_scrolling_time_.has_value()) {
      // Record the time spent scrolling to the bottom of the page.
      base::UmaHistogramLongTimes(
          kFullscreenScrollToTheBottomTime,
          base::TimeTicks::Now() - start_scrolling_time_.value());
      // Avoid recording multiple time in the same page.
      is_scrolling_time_recorded_ = true;
    }
    // Stop ignoring the current scroll.
    ignoring_current_scroll_ = false;
    // Notify observers that the scroll event has ended.
    ScopedIncrementer scroll_ended_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelScrollEventEnded(this);
    }
  }
}

bool FullscreenModel::IsScrollViewScrolling() const {
  return scrolling_;
}

void FullscreenModel::SetScrollViewIsZooming(bool zooming) {
  zooming_ = zooming;
}

bool FullscreenModel::IsScrollViewZooming() const {
  return zooming_;
}

void FullscreenModel::SetScrollViewIsDragging(bool dragging) {
  if (dragging_ == dragging) {
    return;
  }
  dragging_ = dragging;
  if (dragging_) {
    SetLastScrollDirection(FullscreenModelScrollDirection::kNone);
    // Update the base offset for each new scroll event.
    UpdateBaseOffset();
    offset_at_start_of_drag_ = y_content_offset_;
    // Re-rendering events are ignored during scrolls since disabling the model
    // mid-scroll leads to choppy animations.  If the content was re-rendered
    // to be too short to collapse the toolbars, the model should be disabled
    // to prevent the subsequent scroll.
    UpdateDisabledCounterForContentHeight();
  }
}

bool FullscreenModel::IsScrollViewDragging() const {
  return dragging_;
}

void FullscreenModel::SetResizesScrollView(bool resizes_scroll_view) {
  resizes_scroll_view_ = resizes_scroll_view;
}

bool FullscreenModel::ResizesScrollView() const {
  return resizes_scroll_view_;
}

void FullscreenModel::SetWebViewSafeAreaInsets(UIEdgeInsets safe_area_insets) {
  if (UIEdgeInsetsEqualToEdgeInsets(safe_area_insets_, safe_area_insets)) {
    return;
  }
  safe_area_insets_ = safe_area_insets;
  UpdateDisabledCounterForContentHeight();
}

UIEdgeInsets FullscreenModel::GetWebViewSafeAreaInsets() const {
  return safe_area_insets_;
}

void FullscreenModel::SetForceFullscreenMode(bool force_fullscreen_mode) {
  if (force_fullscreen_mode) {
    force_fullscreen_mode_counter_++;
  } else {
    force_fullscreen_mode_counter_--;
  }
  DCHECK_GE(force_fullscreen_mode_counter_, 0U);
}

bool FullscreenModel::IsForceFullscreenMode() const {
  return force_fullscreen_mode_counter_ > 0;
}

void FullscreenModel::SetInsetsUpdateEnabled(bool enabled) {
  insets_update_enabled_ = enabled;
}

bool FullscreenModel::IsInsetsUpdateEnabled() const {
  return insets_update_enabled_;
}

FullscreenModel::ScrollAction FullscreenModel::ActionForScrollFromOffset(
    CGFloat from_offset) const {
  // Update the base offset but don't recalculate progress if:
  // - the model is disabled,
  // - the scroll is not triggered by a user action,
  // - the sroll view is zooming,
  // - the scroll is triggered from a FullscreenModelObserver callback,
  // - there is no toolbar,
  // - the scroll offset doesn't change,
  // - the scroll has not exceeded the required threshold.
  if (!enabled() || !scrolling_ || zooming_ || observer_callback_count_ ||
      AreCGFloatsEqual(get_toolbar_height_delta(), 0.0) ||
      AreCGFloatsEqual(y_content_offset_, from_offset) ||
      !ScrollThresholdExceeded()) {
    return ScrollAction::kUpdateBaseOffset;
  }

  // Ignore if:
  // - explicitly requested via IgnoreRemainderOfCurrentScroll(),
  // - the scroll is a bounce-up animation at the top,
  // - the scroll is attempting to scroll content up when it already fits,
  // - the scroll is attempting to scroll past the bottom of the content when
  //   the scroll view is being resized (the rebound scroll animation
  //   interferes with the frame resizing).
  bool scrolling_content_down = y_content_offset_ - from_offset < 0.0;
  bool scrolling_past_top = y_content_offset_ <= -top_inset_;
  bool content_fits = content_height_ <= scroll_view_height_ - top_inset_;
  bool scrolling_past_bottom =
      y_content_offset_ + scroll_view_height_ + top_inset_ +
          toolbars_size_.expandedTopToolbarHeight +
          toolbars_size_.expandedBottomToolbarHeight -
          (toolbars_size_.collapsedTopToolbarHeight -
           toolbars_size_.collapsedBottomToolbarHeight) >=
      content_height_;
  if (ignoring_current_scroll_ ||
      (scrolling_past_top && !scrolling_content_down) ||
      (content_fits && !scrolling_content_down) ||
      (resizes_scroll_view_ && scrolling_past_bottom)) {
    return ScrollAction::kIgnore;
  }

  // All other scrolls should result in an updated progress value.  If the model
  // doesn't have a base offset, it should also be updated.
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    return has_base_offset() ? ScrollAction::kUpdateProgress
                             : ScrollAction::kUpdateBaseOffsetAndProgress;
  } else {
    return ScrollAction::kUpdateProgress;
  }
}

void FullscreenModel::SetLastScrollDirection(
    FullscreenModelScrollDirection direction) {
  if (direction == fullscreen_scroll_direction_) {
    return;
  }
  if (fullscreen_scroll_direction_ != FullscreenModelScrollDirection::kNone) {
    UpdateBaseOffset();
  }
  fullscreen_scroll_direction_ = direction;
}

void FullscreenModel::UpdateBaseOffset() {
  base_offset_ = y_content_offset_ -
                 (1.0 - progress_) * get_toolbar_height_delta() / speed_;
}

void FullscreenModel::UpdateSpeed() {
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault) ||
      !base::FeatureList::IsEnabled(kFullscreenTransitionSpeed)) {
    return;
  }
  if (FullscreenTransitionSpeedParam() == FullscreenTransitionSpeed::kSlower) {
    speed_ = 0.50;
  }
  if (FullscreenTransitionSpeedParam() == FullscreenTransitionSpeed::kFaster) {
    speed_ = 1.50;
  }
}

void FullscreenModel::UpdateProgress() {
  const CGFloat delta = base_offset_ - y_content_offset_;
  const CGFloat toolbar_height_delta = get_toolbar_height_delta();
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    SetProgress(1.0 + delta / toolbar_height_delta);
    return;
  } else {
    if (IsFullscreenTransitionSpeedSet()) {
      SetProgress(1.0 + (delta * speed_) / toolbar_height_delta);
      return;
    }
    SetProgress(1.0 + delta / toolbar_height_delta);
  }
}

void FullscreenModel::UpdateDisabledCounterForContentHeight() {
  // Sometimes the content size and scroll view sizes are updated mid-scroll
  // such that the scroll view height is updated before the content is re-
  // rendered, causing the model to be disabled.  These changes should be
  // ignored while the content is scrolling.
  if (scrolling_) {
    return;
  }
  // The model should be disabled when the content fits.
  CGFloat disabling_threshold = scroll_view_height_;
  if (resizes_scroll_view_) {
    // When Smooth Scrolling is disabled, the scroll view can sometimes be
    // resized to account for the viewport insets after the page has been
    // rendered, so account for the maximum toolbar insets in the threshold.
    disabling_threshold +=
        GetExpandedTopToolbarHeight() + GetExpandedBottomToolbarHeight();
  } else {
    // After reloads, pages whose viewports fit the screen are sometimes
    // resized to account for the safe area insets.  Adding these to the
    // threshold helps prevent fullscreen from beeing re-enabled in this
    // case.
    // TODO(crbug.com/41437113): This logic can potentially disable
    // fullscreen for short pages in which this bug does not occur.  It
    // should be removed once the page can be reloaded without resizing.
    disabling_threshold += safe_area_insets_.top + safe_area_insets_.bottom;
  }

  // Don't disable fullscreen if both heights have not been received.
  bool areBothHeightsSet = !AreCGFloatsEqual(content_height_, 0.0) &&
                           !AreCGFloatsEqual(scroll_view_height_, 0.0);

  bool disable = areBothHeightsSet && content_height_ <= disabling_threshold;
  if (disabled_for_short_content_ == disable) {
    return;
  }
  disabled_for_short_content_ = disable;
  if (disable) {
    IncrementDisabledCounter();
  } else {
    DecrementDisabledCounter();
  }
}

void FullscreenModel::SetProgress(CGFloat progress) {
  progress = std::min(static_cast<CGFloat>(1.0), progress);
  progress = std::max(static_cast<CGFloat>(0.0), progress);
  if (AreCGFloatsEqual(progress_, progress)) {
    return;
  }

  if (progress == 0.0 && progress_ > 0.0) {
    base::UmaHistogramEnumeration(
        kEnterFullscreenModeTransitionReasonHistogram,
        FullscreenModeTransitionReason::kUserControlled);
  } else if (progress == 1.0 && progress_ < 1.0) {
    base::UmaHistogramEnumeration(
        kExitFullscreenModeTransitionReasonHistogram,
        FullscreenModeTransitionReason::kUserControlled);
  }

  progress_ = progress;

  // Prevent observer callbacks from recursively setting progress.
  if (setting_progress_) {
    return;
  }
  setting_progress_ = true;
  ScopedIncrementer progress_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelProgressUpdated(this);
  }
  setting_progress_ = false;
}

void FullscreenModel::OnScrollViewSizeBroadcasted(CGSize scroll_view_size) {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  SetScrollViewHeight(scroll_view_size.height);
}

void FullscreenModel::OnScrollViewContentSizeBroadcasted(CGSize content_size) {
  SetContentHeight(content_size.height);
}

void FullscreenModel::OnScrollViewContentInsetBroadcasted(
    UIEdgeInsets content_inset) {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  SetTopContentInset(content_inset.top);
}

void FullscreenModel::OnContentScrollOffsetBroadcasted(CGFloat offset) {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  SetYContentOffset(offset);
}

void FullscreenModel::OnScrollViewIsScrollingBroadcasted(bool scrolling) {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  SetScrollViewIsScrolling(scrolling);
}

void FullscreenModel::OnScrollViewIsZoomingBroadcasted(bool zooming) {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  SetScrollViewIsZooming(zooming);
}

void FullscreenModel::OnScrollViewIsDraggingBroadcasted(bool dragging) {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  SetScrollViewIsDragging(dragging);
}

void FullscreenModel::OnCollapsedTopToolbarHeightBroadcasted(CGFloat height) {
  CHECK(!IsRefactorToolbarsSize());
  ToolbarsHeightDidChange();
}

void FullscreenModel::OnExpandedTopToolbarHeightBroadcasted(CGFloat height) {
  CHECK(!IsRefactorToolbarsSize());
  ToolbarsHeightDidChange();
}

void FullscreenModel::OnCollapsedBottomToolbarHeightBroadcasted(
    CGFloat height) {
  CHECK(!IsRefactorToolbarsSize());
  ToolbarsHeightDidChange();
}

void FullscreenModel::OnExpandedBottomToolbarHeightBroadcasted(CGFloat height) {
  CHECK(!IsRefactorToolbarsSize());
  ToolbarsHeightDidChange();
}

void FullscreenModel::SetToolbarsSize(ToolbarsSize* toolbars_size) {
  toolbars_size_ = toolbars_size;
  ToolbarsHeightDidChange();
  if (IsRefactorToolbarsSize() && toolbars_size_) {
    [toolbars_size addObserver:this];
  }
}

void FullscreenModel::OnTopToolbarHeightChanged() {
  CHECK(IsRefactorToolbarsSize());
  ToolbarsHeightDidChange();
}

void FullscreenModel::OnBottomToolbarHeightChanged() {
  CHECK(IsRefactorToolbarsSize());
  ToolbarsHeightDidChange();
}

bool FullscreenModel::ScrollThresholdExceeded() const {
  if (web::features::IsFullscreenScrollThresholdEnabled()) {
    // When scrolled to the very top, the threshold should be ignored so that
    // fullscreen can be smoothly exited.
    if (y_content_offset_ <= 0.0) {
      return true;
    }
    return std::abs(y_content_offset_ - offset_at_start_of_drag_) >
           scroll_threshold_;
  }
  return true;
}
