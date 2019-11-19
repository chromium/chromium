// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MODEL_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MODEL_H_

#import <CoreGraphics/CoreGraphics.h>
#include <cmath>

#include "base/macros.h"
#include "base/observer_list.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"

class FullscreenModelObserver;

// Model object used to calculate fullscreen state.
class FullscreenModel : public ChromeBroadcastObserverInterface {
 public:
  FullscreenModel();
  ~FullscreenModel() override;

  // Adds and removes FullscreenModelObservers.
  void AddObserver(FullscreenModelObserver* observer) {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(FullscreenModelObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  // The progress value calculated by the model.
  CGFloat progress() const { return progress_; }

  // Whether fullscreen is disabled.  When disabled, the toolbar is completely
  // visible.
  bool enabled() const { return disabled_counter_ == 0U; }

  // Whether the base offset has been recorded after state has been invalidated
  // by navigations or toolbar height changes.
  bool has_base_offset() const { return !std::isnan(base_offset_); }

  // The base offset against which the fullscreen progress is being calculated.
  CGFloat base_offset() const { return base_offset_; }

  // Returns the difference between the max and min toolbar heights.
  CGFloat toolbar_height_delta() const {
    return expanded_toolbar_height_ - collapsed_toolbar_height_;
  }

  // Returns whether the page content is tall enough for the toolbar to be
  // scrolled to an entirely collapsed position.
  bool can_collapse_toolbar() const {
    return content_height_ > scroll_view_height_ + toolbar_height_delta();
  }

  // Whether the view is scrolled all the way to the top.
  bool is_scrolled_to_top() const {
    return y_content_offset_ <= -expanded_toolbar_height_;
  }

  // Whether the view is scrolled all the way to the bottom.
  bool is_scrolled_to_bottom() const {
    return y_content_offset_ + scroll_view_height_ >= content_height_;
  }

  // The min, max, and current insets caused by the toolbars.
  UIEdgeInsets min_toolbar_insets() const {
    return GetToolbarInsetsAtProgress(0.0);
  }
  UIEdgeInsets max_toolbar_insets() const {
    return GetToolbarInsetsAtProgress(1.0);
  }
  UIEdgeInsets current_toolbar_insets() const {
    return GetToolbarInsetsAtProgress(progress_);
  }

  // Returns the toolbar insets at |progress|.
  UIEdgeInsets GetToolbarInsetsAtProgress(CGFloat progress) const {
    return UIEdgeInsetsMake(
        collapsed_toolbar_height_ +
            progress * (expanded_toolbar_height_ - collapsed_toolbar_height_),
        0, progress * bottom_toolbar_height_, 0);
  }

  // Increments and decrements |disabled_counter_| for features that require the
  // toolbar be completely visible.
  void IncrementDisabledCounter();
  void DecrementDisabledCounter();

  // Recalculates the fullscreen progress for a new navigation.
  void ResetForNavigation();

  // Instructs the model to ignore broadcasted scroll updates for the remainder
  // of the current scroll.  Has no effect if not called while a scroll is
  // occurring.  The model will resume listening for scroll events when
  // |scrolling_| is reset to false.
  void IgnoreRemainderOfCurrentScroll();

  // Called when a scroll end animation finishes.  |progress| is the fullscreen
  // progress corresponding to the final state of the aniamtion.
  void AnimationEndedWithProgress(CGFloat progress);

  // Setter for the minimum toolbar height to use in calculations. Setting this
  // resets the model to a fully visible state.
  void SetCollapsedToolbarHeight(CGFloat height);
  CGFloat GetCollapsedToolbarHeight() const;

  // Setter for the maximum toolbar height to use in calculations. Setting this
  // resets the model to a fully visible state.
  void SetExpandedToolbarHeight(CGFloat height);
  CGFloat GetExpandedToolbarHeight() const;

  // Setter for the bottom toolbar height to use in calculations. Setting this
  // resets the model to a fully visible state
  void SetBottomToolbarHeight(CGFloat height);
  CGFloat GetBottomToolbarHeight() const;

  // Setter for the height of the scroll view displaying the main content.
  void SetScrollViewHeight(CGFloat scroll_view_height);
  CGFloat GetScrollViewHeight() const;

  // Setter for the current height of the rendered page.
  void SetContentHeight(CGFloat content_height);
  CGFloat GetContentHeight() const;

  // Setter for the top content inset of the scroll view displaying the main
  // content.
  void SetTopContentInset(CGFloat top_inset);
  CGFloat GetTopContentInset() const;

  // Setter for the current vertical content offset. Setting this will
  // recalculate the progress value.
  void SetYContentOffset(CGFloat y_content_offset);
  CGFloat GetYContentOffset() const;

  // Setter for whether the scroll view is scrolling. If a scroll event ends
  // and the progress value is not 0.0 or 1.0, the model will round to the
  // nearest value.
  void SetScrollViewIsScrolling(bool scrolling);
  bool IsScrollViewScrolling() const;

  // Setter for whether the scroll view is zooming.
  void SetScrollViewIsZooming(bool zooming);
  bool IsScrollViewZooming() const;

  // Setter for whether the scroll view is being dragged.
  void SetScrollViewIsDragging(bool dragging);
  bool IsScrollViewDragging() const;

  // Setter for whether the scroll view is resized for fullscreen events.
  void SetResizesScrollView(bool resizes_scroll_view);
  bool ResizesScrollView() const;

  // Setter for the safe area insets for the current WebState's view.
  void SetWebViewSafeAreaInsets(UIEdgeInsets safe_area_insets);
  UIEdgeInsets GetWebViewSafeAreaInsets() const;

 private:
  // Returns how a scroll to the current |y_content_offset_| from |from_offset|
  // should be handled.
  enum class ScrollAction : short {
    kIgnore,                       // Ignore the scroll.
    kUpdateBaseOffset,             // Update |base_offset_| only.
    kUpdateProgress,               // Update |progress_| only.
    kUpdateBaseOffsetAndProgress,  // Update |bse_offset_| and |progress_|.
  };
  ScrollAction ActionForScrollFromOffset(CGFloat from_offset) const;

  // Updates the base offset given the current y content offset, progress, and
  // toolbar height.
  void UpdateBaseOffset();

  // Updates the progress value given the current y content offset, base offset,
  // and toolbar height.
  void UpdateProgress();

  // Updates the disabled counter depending on the current values of
  // |scroll_view_height_| and |content_height_|.
  void UpdateDisabledCounterForContentHeight();

  // Setter for |progress_|.  Notifies observers of the new value if
  // |notify_observers| is true.
  void SetProgress(CGFloat progress);

  // ChromeBroadcastObserverInterface:
  void OnScrollViewSizeBroadcasted(CGSize scroll_view_size) override;
  void OnScrollViewContentSizeBroadcasted(CGSize content_size) override;
  void OnScrollViewContentInsetBroadcasted(UIEdgeInsets content_inset) override;
  void OnContentScrollOffsetBroadcasted(CGFloat offset) override;
  void OnScrollViewIsScrollingBroadcasted(bool scrolling) override;
  void OnScrollViewIsZoomingBroadcasted(bool zooming) override;
  void OnScrollViewIsDraggingBroadcasted(bool dragging) override;
  void OnCollapsedToolbarHeightBroadcasted(CGFloat height) override;
  void OnExpandedToolbarHeightBroadcasted(CGFloat height) override;
  void OnBottomToolbarHeightBroadcasted(CGFloat height) override;

  // The observers for this model.
  base::ObserverList<FullscreenModelObserver>::Unchecked observers_;
  // The percentage of the toolbar that should be visible, where 1.0 denotes a
  // fully visible toolbar and 0.0 denotes a completely hidden one.
  CGFloat progress_ = 0.0;
  // The base offset from which to calculate fullscreen state.  When |locked_|
  // is false, it is reset to the current offset after each scroll event.
  CGFloat base_offset_ = NAN;
  // The height of the toolbars being shown or hidden by this model.
  CGFloat collapsed_toolbar_height_ = 0.0;
  CGFloat expanded_toolbar_height_ = 0.0;
  CGFloat bottom_toolbar_height_ = 0.0;
  // The current vertical content offset of the main content.
  CGFloat y_content_offset_ = 0.0;
  // The height of the scroll view displaying the current page.
  CGFloat scroll_view_height_ = 0.0;
  // The height of the current page's rendered content.
  CGFloat content_height_ = 0.0;
  // The top inset of the scroll view displaying the current page.
  CGFloat top_inset_ = 0.0;
  // How many currently-running features require the toolbar be visible.
  size_t disabled_counter_ = 0;
  // Whether fullscreen is disabled for short content.
  bool disabled_for_short_content_ = false;
  // Whether the main content is being scrolled.
  bool scrolling_ = false;
  // Whether the scroll view is zooming.
  bool zooming_ = false;
  // Whether the main content is being dragged.
  bool dragging_ = false;
  // Whether the in-progress scroll is being ignored.
  bool ignoring_current_scroll_ = false;
  // Whether the scroll view is resized for fullscreen events.
  bool resizes_scroll_view_ = false;
  // The WebState view's safe area insets.
  UIEdgeInsets safe_area_insets_ = UIEdgeInsetsZero;
  // The number of FullscreenModelObserver callbacks currently being executed.
  size_t observer_callback_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FullscreenModel);
};

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MODEL_H_
