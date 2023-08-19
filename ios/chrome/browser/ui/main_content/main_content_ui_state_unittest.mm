// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main_content/main_content_ui_state.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "testing/platform_test.h"

#pragma mark - MainContentUIForwarderTest

// Test fixture for MainContentUIStateUpdaters.
class MainContentUIStateUpdaterTest : public PlatformTest {
 public:
  MainContentUIStateUpdaterTest()
      : PlatformTest(),
        state_([[MainContentUIState alloc] init]),
        updater_([[MainContentUIStateUpdater alloc] initWithState:state_]) {}

  // The UI state.
  MainContentUIState* state() { return state_; }
  // The updater being tested.
  MainContentUIStateUpdater* updater() { return updater_; }

 private:
  __strong MainContentUIState* state_;
  __strong MainContentUIStateUpdater* updater_;
};

// Tests that the y content offset is correctly broadcast as the result of
// `-scrollViewDidScrollToOffset:`.
TEST_F(MainContentUIStateUpdaterTest, UpdateOffset) {
  const CGFloat kYOffset = 150.0;
  ASSERT_TRUE(AreCGFloatsEqual(0.0, state().yContentOffset));
  [updater() scrollViewDidScrollToOffset:CGPointMake(0.0, kYOffset)];
  EXPECT_TRUE(AreCGFloatsEqual(kYOffset, state().yContentOffset));
}

// Tests broadcasting drag and scroll events.
TEST_F(MainContentUIStateUpdaterTest, BroadcastScrollingAndDragging) {
  UIPanGestureRecognizer* pan = [[UIPanGestureRecognizer alloc] init];
  ASSERT_FALSE(state().scrolling);
  ASSERT_FALSE(state().dragging);
  [updater() scrollViewWillBeginDraggingWithGesture:pan];
  EXPECT_TRUE(state().scrolling);
  EXPECT_TRUE(state().dragging);
  [updater() scrollViewDidEndDraggingWithGesture:pan
                             targetContentOffset:CGPointMake(0, 5)];
  EXPECT_TRUE(state().scrolling);
  EXPECT_FALSE(state().dragging);
  [updater() scrollViewDidEndDecelerating];
  EXPECT_FALSE(state().scrolling);
  EXPECT_FALSE(state().dragging);
}

// Tests that pixel alignment don't count as residual deceleration.
TEST_F(MainContentUIStateUpdaterTest, IgnorePixelAligntment) {
  UIPanGestureRecognizer* pan = [[UIPanGestureRecognizer alloc] init];
  const CGFloat kSinglePixel = 1.0 / [UIScreen mainScreen].scale;
  const CGPoint kUnalignedOffset = CGPointMake(0.0, kSinglePixel / 2.0);
  ASSERT_FALSE(state().scrolling);
  ASSERT_FALSE(state().dragging);
  [updater() scrollViewWillBeginDraggingWithGesture:pan];
  EXPECT_TRUE(state().scrolling);
  EXPECT_TRUE(state().dragging);
  [updater() scrollViewDidScrollToOffset:kUnalignedOffset];
  [updater() scrollViewDidEndDraggingWithGesture:pan
                             targetContentOffset:CGPointZero];
  EXPECT_FALSE(state().scrolling);
  EXPECT_FALSE(state().dragging);
}

// Tests resetting state for interrupted scrolls.
TEST_F(MainContentUIStateUpdaterTest, ScrollInterruption) {
  UIPanGestureRecognizer* pan = [[UIPanGestureRecognizer alloc] init];
  ASSERT_FALSE(state().scrolling);
  ASSERT_FALSE(state().dragging);
  [updater() scrollViewWillBeginDraggingWithGesture:pan];
  EXPECT_TRUE(state().scrolling);
  EXPECT_TRUE(state().dragging);
  [updater() scrollWasInterrupted];
  ASSERT_FALSE(state().scrolling);
  ASSERT_FALSE(state().dragging);
}
