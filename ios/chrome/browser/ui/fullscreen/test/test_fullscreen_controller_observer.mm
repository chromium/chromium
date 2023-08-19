// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller_observer.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"

void TestFullscreenControllerObserver::FullscreenViewportInsetRangeChanged(
    FullscreenController* controller,
    UIEdgeInsets min_viewport_insets,
    UIEdgeInsets max_viewport_insets) {
  min_viewport_insets_ = min_viewport_insets;
  max_viewport_insets_ = max_viewport_insets;
  current_viewport_insets_ = controller->GetCurrentViewportInsets();
}

void TestFullscreenControllerObserver::FullscreenProgressUpdated(
    FullscreenController* controller,
    CGFloat progress) {
  progress_ = progress;
}

void TestFullscreenControllerObserver::FullscreenEnabledStateChanged(
    FullscreenController* controller,
    bool enabled) {
  enabled_ = enabled;
}

void TestFullscreenControllerObserver::FullscreenWillAnimate(
    FullscreenController* controller,
    FullscreenAnimator* animator) {
  animator_ = animator;
}

void TestFullscreenControllerObserver::FullscreenControllerWillShutDown(
    FullscreenController* controller) {
  is_shut_down_ = true;
  controller->RemoveObserver(this);
}
