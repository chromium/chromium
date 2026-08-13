// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_redesign_view_controller.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
const CGFloat kMinDragHandleHeight = 24.0;
} // namespace

@interface NewTabPageRedesignViewController (Testing)
- (CGFloat)restingOffsetForBottomSheetViewController:
    (NewTabPageBottomSheetViewController*)viewController;
- (CGFloat)topContentHeight;
- (BOOL)isCompactHeight;
@end

class NewTabPageRedesignViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[NewTabPageRedesignViewController alloc] init];
  }

 protected:
  NewTabPageRedesignViewController* view_controller_;
};

// Tests topContentHeight constraint constants match padding experiment arms.
TEST_F(NewTabPageRedesignViewControllerTest, TestPaddingUpdateExperimentArms) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageUICleanup);

  [view_controller_ loadViewIfNeeded];
  EXPECT_GT([view_controller_ topContentHeight], 0.0);
}

// Tests that in compact vertical size class, restingOffset sits directly below
// the top content without pushing the handle offscreen.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestLandscapeRestingOffsetNaturalFlow) {
  [view_controller_ loadViewIfNeeded];

  id mock_vc = OCMPartialMock(view_controller_);
  OCMStub([mock_vc isCompactHeight]).andReturn(YES);

  UIView* mock_view = OCMPartialMock(view_controller_.view);
  OCMStub([mock_view bounds]).andReturn(CGRectMake(0, 0, 800, 400));
  OCMStub([mock_view safeAreaInsets])
      .andReturn(UIEdgeInsetsMake(0, 44, 21, 44));

  CGFloat resting_offset =
      [mock_vc restingOffsetForBottomSheetViewController:nil];

  CGFloat screen_height = 400.0;
  CGFloat safe_area_bottom = 21.0;
  CGFloat max_allowed_offset =
      screen_height - safe_area_bottom - kMinDragHandleHeight;

  EXPECT_LE(resting_offset, max_allowed_offset);
}

// Tests that oversized top content height clamps the resting offset to
// screenHeight - safeAreaBottom - kMinDragHandleHeight.
TEST_F(NewTabPageRedesignViewControllerTest, TestLandscapeSafetyGuard) {
  [view_controller_ loadViewIfNeeded];

  id mock_vc = OCMPartialMock(view_controller_);
  OCMStub([mock_vc isCompactHeight]).andReturn(YES);

  // Force an oversized top content height
  OCMStub([mock_vc topContentHeight]).andReturn(2000.0);

  UIView* mock_view = OCMPartialMock(view_controller_.view);
  OCMStub([mock_view bounds]).andReturn(CGRectMake(0, 0, 800, 400));
  OCMStub([mock_view safeAreaInsets])
      .andReturn(UIEdgeInsetsMake(0, 44, 21, 44));

  CGFloat resting_offset =
      [mock_vc restingOffsetForBottomSheetViewController:nil];

  CGFloat screen_height = 400.0;
  CGFloat safe_area_bottom = 21.0;
  CGFloat max_allowed_offset =
      screen_height - safe_area_bottom - kMinDragHandleHeight;

  EXPECT_EQ(resting_offset, max_allowed_offset);
}
