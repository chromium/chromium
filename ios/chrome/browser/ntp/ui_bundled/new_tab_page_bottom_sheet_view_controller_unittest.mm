// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface NewTabPageBottomSheetViewController (Testing)
- (void)updateContentContainerInsetForOffset:(CGFloat)topOffset;
@end

class NewTabPageBottomSheetViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[NewTabPageBottomSheetViewController alloc] init];
  }

 protected:
  NewTabPageBottomSheetViewController* view_controller_;
};

// Tests that the view controller loads its view correctly.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestLoadView) {
  [view_controller_ loadViewIfNeeded];
  EXPECT_NE(nil, view_controller_.view);
}

// Tests that the feed view controller is correctly embedded as a child view
// controller.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestEmbedFeedViewController) {
  UIViewController* child_vc = [[UIViewController alloc] init];
  view_controller_.feedViewController = child_vc;

  [view_controller_ loadViewIfNeeded];

  EXPECT_EQ(child_vc.parentViewController, view_controller_);
  EXPECT_TRUE([child_vc.view isDescendantOfView:view_controller_.view]);
}

// Tests that the magic stack view controller is correctly embedded as a child
// view controller.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestEmbedMagicStackViewController) {
  UIViewController* child_vc = [[UIViewController alloc] init];
  view_controller_.magicStackViewController = child_vc;

  [view_controller_ loadViewIfNeeded];

  EXPECT_EQ(child_vc.parentViewController, view_controller_);
  UIView* container =
      [view_controller_ valueForKey:@"_magicStackContainerView"];
  EXPECT_NE(nil, container);
  EXPECT_TRUE([child_vc.view isDescendantOfView:container]);
}

// Tests that the magic stack container view alpha updates based on top offset.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestMagicStackContainerAlpha) {
  [view_controller_ loadViewIfNeeded];
  UIView* container =
      [view_controller_ valueForKey:@"_magicStackContainerView"];
  EXPECT_NE(nil, container);

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              collapsedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(600.0);

  CGFloat expanded = [view_controller_ expandedOffset];
  CGFloat resting = [view_controller_ restingOffset];

  // At resting offset, progress should be 1.0, meaning alpha is 1.0
  [view_controller_ updateContentContainerInsetForOffset:resting];
  EXPECT_FLOAT_EQ(1.0, container.alpha);

  // At expanded offset, progress should be 0.0, meaning alpha is 0.0
  [view_controller_ updateContentContainerInsetForOffset:expanded];
  EXPECT_FLOAT_EQ(0.0, container.alpha);

  // At halfway between expanded and resting, progress should be 0.5, alpha
  // should be 0.5
  [view_controller_
      updateContentContainerInsetForOffset:(expanded + resting) / 2.0];
  EXPECT_FLOAT_EQ(0.5, container.alpha);
}
