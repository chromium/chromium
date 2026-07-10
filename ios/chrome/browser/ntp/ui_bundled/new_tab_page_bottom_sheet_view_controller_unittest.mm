// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

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
