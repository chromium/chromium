// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"

#include <algorithm>

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model_observer.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Object that increments |counter| by 1 for its lifetime.
class ScopedIncrementer {
 public:
  explicit ScopedIncrementer(size_t* counter) : counter_(counter) {
    ++(*counter_);
  }
  ~ScopedIncrementer() { --(*counter_); }

 private:
  size_t* counter_;
};
}

FullscreenModel::FullscreenModel() = default;
FullscreenModel::~FullscreenModel() = default;

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

void FullscreenModel::ResetForNavigation() {
  progress_ = 1.0;
  scrolling_ = false;
  base_offset_ = NAN;
  ScopedIncrementer reset_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelWasReset(this);
  }
}

void FullscreenModel::IgnoreRemainderOfCurrentScroll() {
  if (!scrolling_)
    return;
  ignoring_current_scroll_ = true;
}

void FullscreenModel::AnimationEndedWithProgress(CGFloat progress) {
  DCHECK_GE(progress, 0.0);
  DCHECK_LE(progress, 1.0);
  // Since this is being set by the animator instead of by scroll events, do not
  // broadcast the new progress value.
  progress_ = progress;
}

void FullscreenModel::SetCollapsedToolbarHeight(CGFloat height) {
  if (AreCGFloatsEqual(collapsed_toolbar_height_, height))
    return;
  DCHECK_GE(height, 0.0);
  collapsed_toolbar_height_ = height;
  ResetForNavigation();
}

CGFloat FullscreenModel::GetCollapsedToolbarHeight() const {
  return collapsed_toolbar_height_;
}

void FullscreenModel::SetExpandedToolbarHeight(CGFloat height) {
  if (AreCGFloatsEqual(expanded_toolbar_height_, height))
    return;
  DCHECK_GE(height, 0.0);
  expanded_toolbar_height_ = height;
  ResetForNavigation();
}

CGFloat FullscreenModel::GetExpandedToolbarHeight() const {
  return expanded_toolbar_height_;
}

void FullscreenModel::SetBottomToolbarHeight(CGFloat height) {
  if (AreCGFloatsEqual(bottom_toolbar_height_, height))
    return;
  DCHECK_GE(height, 0.0);
  bottom_toolbar_height_ = height;
  ResetForNavigation();
}

CGFloat FullscreenModel::GetBottomToolbarHeight() const {
  return bottom_toolbar_height_;
}

void FullscreenModel::SetScrollViewHeight(CGFloat scroll_view_height) {
  scroll_view_height_ = scroll_view_height;
}

CGFloat FullscreenModel::GetScrollViewHeight() const {
  return scroll_view_height_;
}

void FullscreenModel::SetContentHeight(CGFloat content_height) {
  content_height_ = content_height;
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
  switch (ActionForScrollFromOffset(from_offset)) {
    case ScrollAction::kUpdateBaseOffset:
      UpdateBaseOffset();
      break;
    case ScrollAction::kUpdateProgress:
      UpdateProgress();
      break;
    case ScrollAction::kUpdateBaseOffsetAndProgress:
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
  if (scrolling_ == scrolling)
    return;
  scrolling_ = scrolling;
  if (!scrolling_) {
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
  if (dragging_ == dragging)
    return;
  dragging_ = dragging;
  if (dragging_) {
    ScopedIncrementer scroll_started_incrementer(&observer_callback_count_);
    for (auto& observer : observers_) {
      observer.FullscreenModelScrollEventStarted(this);
    }
    UpdateBaseOffset();
  }
}

bool FullscreenModel::IsScrollViewDragging() const {
  return dragging_;
}

FullscreenModel::ScrollAction FullscreenModel::ActionForScrollFromOffset(
    CGFloat from_offset) const {
  // Update the base offset but don't recalculate progress if:
  // - the model is disabled,
  // - the scroll is not triggered by a user action,
  // - the sroll view is zooming,
  // - the scroll is triggered from a FullscreenModelObserver callback,
  // - there is no toolbar,
  // - the scroll offset doesn't change.
  if (!enabled() || !scrolling_ || zooming_ || observer_callback_count_ ||
      AreCGFloatsEqual(toolbar_height_delta(), 0.0) ||
      AreCGFloatsEqual(y_content_offset_, from_offset)) {
    return ScrollAction::kUpdateBaseOffset;
  }

  // Ignore if:
  // - explicitly requested via IgnoreRemainderOfCurrentScroll(),
  // - the scroll is a bounce-up animation at the top,
  // - the scroll is attempting to scroll past the bottom of the page,
  // - the scroll is attempting to scroll content up when it already fits.
  bool scrolling_content_down = y_content_offset_ - from_offset < 0.0;
  bool scrolling_past_top = y_content_offset_ <= -top_inset_;
  bool content_fits = content_height_ <= scroll_view_height_ - top_inset_;
  if (ignoring_current_scroll_ ||
      (scrolling_past_top && !scrolling_content_down) ||
      is_scrolled_to_bottom() || (content_fits && !scrolling_content_down)) {
    return ScrollAction::kIgnore;
  }

  // All other scrolls should result in an updated progress value.  If the model
  // doesn't have a base offset, it should also be updated.
  return has_base_offset() ? ScrollAction::kUpdateProgress
                           : ScrollAction::kUpdateBaseOffsetAndProgress;
}

void FullscreenModel::UpdateProgress() {
  CGFloat delta = base_offset_ - y_content_offset_;
  SetProgress(1.0 + delta / toolbar_height_delta());
}

void FullscreenModel::UpdateBaseOffset() {
  base_offset_ = y_content_offset_ - (1.0 - progress_) * toolbar_height_delta();
}

void FullscreenModel::SetProgress(CGFloat progress) {
  progress = std::min(static_cast<CGFloat>(1.0), progress);
  progress = std::max(static_cast<CGFloat>(0.0), progress);
  if (AreCGFloatsEqual(progress_, progress))
    return;
  progress_ = progress;

  ScopedIncrementer progress_incrementer(&observer_callback_count_);
  for (auto& observer : observers_) {
    observer.FullscreenModelProgressUpdated(this);
  }
}

void FullscreenModel::OnScrollViewSizeBroadcasted(CGSize scroll_view_size) {
  SetScrollViewHeight(scroll_view_size.height);
}

void FullscreenModel::OnScrollViewContentSizeBroadcasted(CGSize content_size) {
  SetContentHeight(content_size.height);
}

void FullscreenModel::OnScrollViewContentInsetBroadcasted(
    UIEdgeInsets content_inset) {
  SetTopContentInset(content_inset.top);
}

void FullscreenModel::OnContentScrollOffsetBroadcasted(CGFloat offset) {
  SetYContentOffset(offset);
}

void FullscreenModel::OnScrollViewIsScrollingBroadcasted(bool scrolling) {
  SetScrollViewIsScrolling(scrolling);
}

void FullscreenModel::OnScrollViewIsZoomingBroadcasted(bool zooming) {
  SetScrollViewIsZooming(zooming);
}

void FullscreenModel::OnScrollViewIsDraggingBroadcasted(bool dragging) {
  SetScrollViewIsDragging(dragging);
}

void FullscreenModel::OnCollapsedToolbarHeightBroadcasted(CGFloat height) {
  SetCollapsedToolbarHeight(height);
}

void FullscreenModel::OnExpandedToolbarHeightBroadcasted(CGFloat height) {
  SetExpandedToolbarHeight(height);
}

void FullscreenModel::OnBottomToolbarHeightBroadcasted(CGFloat height) {
  SetBottomToolbarHeight(height);
}
