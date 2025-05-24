// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/test/toolbars_size_broadcast_test_util.h"

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"
#import "ios/chrome/browser/toolbar/ui_bundled/test/test_toolbars_size_observer.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {
// The delta by which the toolbar height is adjusted to verify broadcasting.
const CGFloat kHeightDelta = 100.0;
// Helper class that modifies a TestMainContentUIState, then reverts those
// changes upon destruction.
class TestToolbarsSizeModifier {
 public:
  TestToolbarsSizeModifier(ToolbarsSize* toolbars_size)
      : toolbars_size_(toolbars_size),
        original_collapsed_top_toolbar_height_(
            toolbars_size_.collapsedTopToolbarHeight),
        original_expanded_top_toolbar_height_(
            toolbars_size_.expandedTopToolbarHeight) {
    toolbars_size_.collapsedTopToolbarHeight += kHeightDelta;
    toolbars_size_.expandedTopToolbarHeight += kHeightDelta;
  }
  ~TestToolbarsSizeModifier() {
    toolbars_size_.collapsedTopToolbarHeight =
        original_collapsed_top_toolbar_height_;
    toolbars_size_.expandedTopToolbarHeight =
        original_expanded_top_toolbar_height_;
  }

  // The original values of the UI state.
  CGFloat original_collapsed_top_toolbar_height() {
    return original_collapsed_top_toolbar_height_;
  }
  CGFloat original_expanded_top_toolbar_height() {
    return original_expanded_top_toolbar_height_;
  }

 private:
  __strong ToolbarsSize* toolbars_size_ = nil;
  CGFloat original_collapsed_top_toolbar_height_ = 0.0;
  CGFloat original_expanded_top_toolbar_height_ = 0.0;
};
}  // namespace

void VerifyToolbarsSizeBroadcast(ToolbarsSize* toolbars_size,
                                 ChromeBroadcaster* broadcaster,
                                 bool should_broadcast) {
  ASSERT_TRUE(toolbars_size);
  ASSERT_TRUE(broadcaster);
  // Create an observer and modifier for `ui_state`.
  TestToolbarsSizeObserver* observer = [[TestToolbarsSizeObserver alloc] init];
  observer.broadcaster = broadcaster;
  TestToolbarsSizeModifier modifier(toolbars_size);
  // Verify whether the changed or original UI elements are observed.
  if (should_broadcast) {
    EXPECT_TRUE(AreCGFloatsEqual(observer.collapsedTopToolbarHeight,
                                 toolbars_size.collapsedTopToolbarHeight));
    EXPECT_TRUE(AreCGFloatsEqual(observer.expandedTopToolbarHeight,
                                 toolbars_size.expandedTopToolbarHeight));
  } else {
    EXPECT_TRUE(
        AreCGFloatsEqual(observer.collapsedTopToolbarHeight,
                         modifier.original_collapsed_top_toolbar_height()));
    EXPECT_TRUE(
        AreCGFloatsEqual(observer.expandedTopToolbarHeight,
                         modifier.original_expanded_top_toolbar_height()));
  }
  // Stop observing `broadcaster`.
  observer.broadcaster = nil;
}
