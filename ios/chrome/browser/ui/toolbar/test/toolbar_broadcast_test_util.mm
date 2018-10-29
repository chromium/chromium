// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/test/toolbar_broadcast_test_util.h"

#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui.h"
#import "ios/chrome/browser/ui/toolbar/test/test_toolbar_ui_observer.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The delta by which the toolbar height is adjusted to verify broadcasting.
const CGFloat kHeightDelta = 100.0;
// Helper class that modifies a TestMainContentUIState, then reverts those
// changes upon destruction.
class TestToolbarUIStateModifier {
 public:
  TestToolbarUIStateModifier(ToolbarUIState* toolbar_ui)
      : toolbar_ui_(toolbar_ui),
        original_collapsed_height_(toolbar_ui_.collapsedHeight),
        original_expanded_height_(toolbar_ui_.expandedHeight) {
    toolbar_ui_.collapsedHeight += kHeightDelta;
    toolbar_ui_.expandedHeight += kHeightDelta;
  }
  ~TestToolbarUIStateModifier() {
    toolbar_ui_.collapsedHeight = original_collapsed_height_;
    toolbar_ui_.expandedHeight = original_expanded_height_;
  }

  // The original values of the UI state.
  CGFloat original_collapsed_height() { return original_collapsed_height_; }
  CGFloat original_expanded_height() { return original_expanded_height_; }

 private:
  __strong ToolbarUIState* toolbar_ui_ = nil;
  CGFloat original_collapsed_height_ = 0.0;
  CGFloat original_expanded_height_ = 0.0;
};
}  // namespace

void VerifyToolbarUIBroadcast(ToolbarUIState* toolbar_ui,
                              ChromeBroadcaster* broadcaster,
                              bool should_broadcast) {
  ASSERT_TRUE(toolbar_ui);
  ASSERT_TRUE(broadcaster);
  // Create an observer and modifier for |ui_state|.
  TestToolbarUIObserver* observer = [[TestToolbarUIObserver alloc] init];
  observer.broadcaster = broadcaster;
  TestToolbarUIStateModifier modifier(toolbar_ui);
  // Verify whether the changed or original UI elements are observed.
  if (should_broadcast) {
    EXPECT_TRUE(
        AreCGFloatsEqual(observer.collapsedHeight, toolbar_ui.collapsedHeight));
    EXPECT_TRUE(
        AreCGFloatsEqual(observer.expandedHeight, toolbar_ui.expandedHeight));
  } else {
    EXPECT_TRUE(AreCGFloatsEqual(observer.collapsedHeight,
                                 modifier.original_collapsed_height()));
    EXPECT_TRUE(AreCGFloatsEqual(observer.expandedHeight,
                                 modifier.original_expanded_height()));
  }
  // Stop observing |broadcaster|.
  observer.broadcaster = nil;
}
