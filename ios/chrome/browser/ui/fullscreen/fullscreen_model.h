// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MODEL_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MODEL_H_

#include <CoreGraphics/CoreGraphics.h>
#include <cmath>

#include "base/observer_list.h"
#include "ios/chrome/browser/shared/public/features/features.h"
#include "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"
#include "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"
#import "ios/web/common/features.h"

class FullscreenModelObserver;

// Model object used to calculate fullscreen state.
class FullscreenModel : public ChromeBroadcastObserverInterface {
 public:
  FullscreenModel();

  FullscreenModel(const FullscreenModel&) = delete;
  FullscreenModel& operator=(const FullscreenModel&) = delete;

  ~FullscreenModel() override;

  // Adds and removes FullscreenModelObservers.
  void AddObserver(FullscreenModelObserver* observer);
  void RemoveObserver(FullscreenModelObserver* observer);

  // The progress value calculated by the model.
  CGFloat progress() const { return progress_; }

  // Whether fullscreen is disabled.  When disabled, the toolbar is completely
  // visible.
  bool enabled() const { return disabled_counter_ == 0U; }

  // Whether the base offset has been recorded after state has been invalidated
  // by navigations or toolbar height changes.
  bool has_base_offset() const {
    CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
    return !std::isnan(base_offset_);
  }

  // The base offset against which the fullscreen progress is being calculated.
  CGFloat base_offset() const { return base_offset_; }

  // Returns the difference between the max and min toolbar heights.
  CGFloat toolbar_height_delta() const {
    CGFloat top_delta =
        GetExpandedTopToolbarHeight() - GetCollapsedTopToolbarHeight();
    if (top_delta < FLT_EPSILON &&
        GetCollapsedBottomToolbarHeight() >= FLT_EPSILON) {
      CGFloat bottom_delta =
          GetExpandedBottomToolbarHeight() - GetCollapsedBottomToolbarHeight();
      return bottom_delta;
    }
    return top_delta;
  }

  // Returns whether the page content is tall enough for the toolbar to be
  // scrolled to an entirely collapsed position.
  bool can_collapse_toolbar() const {
    return content_height_ > scroll_view_height_ + toolbar_height_delta();
  }

  // Whether the view is scrolled all the way to the top.
  bool is_scrolled_to_top() const {
    return y_content_offset_ <= -GetExpandedTopToolbarHeight();
  }

  // Whether the view is scrolled all the way to the bottom.
  bool is_scrolled_to_bottom() const {
    if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
      return y_content_offset_ + scroll_view_height_ >= content_height_;
    } else {
      return y_content_offset_ -
                 (GetCollapsedTopToolbarHeight() +
                  GetCollapsedBottomToolbarHeight() + safe_area_insets_.bottom +
                  safe_area_insets_.top) +
                 (scroll_view_height_ + GetExpandedTopToolbarHeight() +
                  GetExpandedBottomToolbarHeight()) >=
             content_height_;
    }
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

  // Returns the toolbar insets at `progress`.
  UIEdgeInsets GetToolbarInsetsAtProgress(CGFloat progress) const {
    return UIEdgeInsetsMake(GetCollapsedTopToolbarHeight() +
                                progress * (GetExpandedTopToolbarHeight() -
                                            GetCollapsedTopToolbarHeight()),
                            0,
                            GetCollapsedBottomToolbarHeight() +
                                progress * (GetExpandedBottomToolbarHeight() -
                                            GetCollapsedBottomToolbarHeight()),
                            0);
  }

  // Increments and decrements `disabled_counter_` for features that require the
  // toolbar be completely visible.
  void IncrementDisabledCounter();
  void DecrementDisabledCounter();

  // Force enter fullscreen without animation. Setting the progress to 0.0 even
  // when fullscreen is disabled.
  void ForceEnterFullscreen();

  // Recalculates the fullscreen progress for a new navigation.
  void ResetForNavigation();

  // Instructs the model to ignore broadcasted scroll updates for the remainder
  // of the current scroll.  Has no effect if not called while a scroll is
  // occurring.  The model will resume listening for scroll events when
  // `scrolling_` is reset to false.
  void IgnoreRemainderOfCurrentScroll();

  // Called when a scroll end animation finishes.  `progress` is the fullscreen
  // progress corresponding to the final state of the aniamtion.
  void AnimationEndedWithProgress(CGFloat progress);

  // Setter for the minimum top toolbar height to use in calculations. Setting
  // this resets the model to a fully visible state.
  void SetCollapsedTopToolbarHeight(CGFloat height);
  CGFloat GetCollapsedTopToolbarHeight() const;

  // Setter for the maximum top toolbar height to use in calculations. Setting
  // this resets the model to a fully visible state.
  void SetExpandedTopToolbarHeight(CGFloat height);
  CGFloat GetExpandedTopToolbarHeight() const;

  // Setter for the maximum bottom toolbar height to use in calculations.
  // Setting this resets the model to a fully visible state
  void SetExpandedBottomToolbarHeight(CGFloat height);
  CGFloat GetExpandedBottomToolbarHeight() const;

  // Setter for the minimum bottom toolbar height to use in calculations.
  // Setting this resets the model to a fully visible state.
  void SetCollapsedBottomToolbarHeight(CGFloat height);
  CGFloat GetCollapsedBottomToolbarHeight() const;

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

  // Setter for whether force fullscreen mode is active. The mode is used when
  // the bottom toolbar is collapsed above the keyboard.
  void SetForceFullscreenMode(bool force_fullscreen_mode);
  bool IsForceFullscreenMode() const;

 private:
  // Returns how a scroll to the current `y_content_offset_` from `from_offset`
  // should be handled.
  enum class ScrollAction : short {
    kIgnore,                       // Ignore the scroll.
    kUpdateBaseOffset,             // Update `base_offset_` only.
    kUpdateProgress,               // Update `progress_` only.
    kUpdateBaseOffsetAndProgress,  // Update `bse_offset_` and `progress_`.
  };
  ScrollAction ActionForScrollFromOffset(CGFloat from_offset) const;

  // Updates the base offset given the current y content offset, progress, and
  // toolbar height.
  void UpdateBaseOffset();

  // Updates the progress value given the current y content offset, base offset,
  // and toolbar height.
  void UpdateProgress();

  // Updates the disabled counter depending on the current values of
  // `scroll_view_height_` and `content_height_`.
  void UpdateDisabledCounterForContentHeight();

  // Setter for `progress_`.  Notifies observers of the new value if
  // `notify_observers` is true.
  void SetProgress(CGFloat progress);

  // ChromeBroadcastObserverInterface:
  void OnScrollViewSizeBroadcasted(CGSize scroll_view_size) override;
  void OnScrollViewContentSizeBroadcasted(CGSize content_size) override;
  void OnScrollViewContentInsetBroadcasted(UIEdgeInsets content_inset) override;
  void OnContentScrollOffsetBroadcasted(CGFloat offset) override;
  void OnScrollViewIsScrollingBroadcasted(bool scrolling) override;
  void OnScrollViewIsZoomingBroadcasted(bool zooming) override;
  void OnScrollViewIsDraggingBroadcasted(bool dragging) override;
  void OnCollapsedTopToolbarHeightBroadcasted(CGFloat height) override;
  void OnExpandedTopToolbarHeightBroadcasted(CGFloat height) override;
  void OnExpandedBottomToolbarHeightBroadcasted(CGFloat height) override;
  void OnCollapsedBottomToolbarHeightBroadcasted(CGFloat height) override;

  // The observers for this model.
  base::ObserverList<FullscreenModelObserver, true> observers_;
  // The percentage of the toolbar that should be visible, where 1.0 denotes a
  // fully visible toolbar and 0.0 denotes a completely hidden one.
  CGFloat progress_ = 0.0;
  // The base offset from which to calculate fullscreen state.  When `locked_`
  // is false, it is reset to the current offset after each scroll event.
  CGFloat base_offset_ = 0.0;
  // The height of the toolbars being shown or hidden by this model.
  CGFloat collapsed_top_toolbar_height_ = 0.0;
  CGFloat expanded_top_toolbar_height_ = 0.0;
  CGFloat expanded_bottom_toolbar_height_ = 0.0;
  CGFloat collapsed_bottom_toolbar_height_ = 0.0;
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
  // Whether fullscreen is force enabled. Active when the bottom toolbar is
  // collapsed above the keyboard. When active, prevents fullscreen exit.
  // Fullscreen will be reset when exiting this mode.
  bool is_force_fullscreen_mode_ = false;
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
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MODEL_H_
