// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main_content/test/main_content_broadcast_test_util.h"

#import "ios/chrome/browser/ui/main_content/test/test_main_content_ui_observer.h"
#import "ios/chrome/browser/ui/main_content/test/test_main_content_ui_state.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {
// The delta by which the content offset is adjusted to verify broadcasting.
const CGFloat kOffsetDelta = 100.0;
// Helper class that modifies a TestMainContentUIState, then reverts those
// changes upon destruction.
class TestMainContentUIStateModifier {
 public:
  TestMainContentUIStateModifier(TestMainContentUIState* state)
      : state_(state), original_offset_(state.yContentOffset) {
    state_.yContentOffset += kOffsetDelta;
  }
  ~TestMainContentUIStateModifier() {
    state_.yContentOffset = original_offset_;
  }

  // The original values of the UI state.
  CGFloat original_offset() { return original_offset_; }

 private:
  __strong TestMainContentUIState* state_ = nil;
  CGFloat original_offset_ = 0.0;
};
}  // namespace

void VerifyMainContentUIBroadcast(TestMainContentUIState* ui_state,
                                  ChromeBroadcaster* broadcaster,
                                  bool should_broadcast) {
  ASSERT_TRUE(ui_state);
  ASSERT_TRUE(broadcaster);
  // Create an observer and modifier for `ui_state`.
  TestMainContentUIObserver* observer =
      [[TestMainContentUIObserver alloc] init];
  observer.broadcaster = broadcaster;
  TestMainContentUIStateModifier modifier(ui_state);
  // Verify whether the changed or original UI elements are observed.
  if (should_broadcast) {
    EXPECT_TRUE(AreCGFloatsEqual(observer.yOffset, ui_state.yContentOffset));
  } else {
    EXPECT_TRUE(AreCGFloatsEqual(observer.yOffset, modifier.original_offset()));
  }
  // Stop observing `broadcaster`.
  observer.broadcaster = nil;
}
