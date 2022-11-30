// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_TEST_FULLSCREEN_CONTROLLER_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_TEST_FULLSCREEN_CONTROLLER_OBSERVER_H_

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"

// Test version of FullscreenControllerObserver.
class TestFullscreenControllerObserver : public FullscreenControllerObserver {
 public:
  UIEdgeInsets min_viewport_insets() const { return min_viewport_insets_; }
  UIEdgeInsets max_viewport_insets() const { return max_viewport_insets_; }
  UIEdgeInsets current_viewport_insets() const {
    return current_viewport_insets_;
  }
  CGFloat progress() const { return progress_; }
  bool enabled() const { return enabled_; }
  FullscreenAnimator* animator() const { return animator_; }
  bool is_shut_down() const { return is_shut_down_; }

 private:
  // FullscreenControllerObserver:
  void FullscreenViewportInsetRangeChanged(
      FullscreenController* controller,
      UIEdgeInsets min_viewport_insets,
      UIEdgeInsets max_viewport_insets) override;
  void FullscreenProgressUpdated(FullscreenController* controller,
                                 CGFloat progress) override;
  void FullscreenEnabledStateChanged(FullscreenController* controller,
                                     bool enabled) override;
  void FullscreenWillAnimate(FullscreenController* controller,
                             FullscreenAnimator* animator) override;
  void FullscreenControllerWillShutDown(
      FullscreenController* controller) override;

  UIEdgeInsets min_viewport_insets_ = UIEdgeInsetsZero;
  UIEdgeInsets max_viewport_insets_ = UIEdgeInsetsZero;
  UIEdgeInsets current_viewport_insets_ = UIEdgeInsetsZero;
  CGFloat progress_ = 0.0;
  bool enabled_ = true;
  __weak FullscreenAnimator* animator_ = nil;
  bool is_shut_down_ = false;
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_TEST_TEST_FULLSCREEN_CONTROLLER_OBSERVER_H_
