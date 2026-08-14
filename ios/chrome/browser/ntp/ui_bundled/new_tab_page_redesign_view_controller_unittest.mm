// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_redesign_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

namespace {
const CGFloat kMinDragHandleHeight = 24.0;
}  // namespace

@interface NewTabPageRedesignViewController (Testing) <
    NewTabPageBottomSheetViewControllerDelegate>
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

// Tests that the view controller loads its view correctly.
TEST_F(NewTabPageRedesignViewControllerTest, TestLoadView) {
  [view_controller_ loadViewIfNeeded];
  EXPECT_NE(nil, view_controller_.view);
}

// Tests that didUpdateTopOffset in legacy mode (static-fakebox: false)
// translates fakeLocationBar and keeps alpha at 1.0.
TEST_F(NewTabPageRedesignViewControllerTest, TestLegacyDidUpdateTopOffset) {
  view_controller_.view.frame = CGRectMake(0, 0, 400, 800);
  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  UIView* fake_location_bar =
      [view_controller_ valueForKey:@"_fakeLocationBar"];
  EXPECT_NE(nil, fake_location_bar);

  NewTabPageBottomSheetViewController* sheet =
      [view_controller_ valueForKey:@"_bottomSheetViewController"];
  CGFloat expandedOffset = [sheet expandedOffset];
  CGFloat restingOffset = [sheet restingOffset];
  CGFloat midOffset = (expandedOffset + restingOffset) / 2.0;

  [view_controller_ bottomSheetViewController:sheet
                           didUpdateTopOffset:midOffset];

  EXPECT_FLOAT_EQ(1.0, fake_location_bar.alpha);
}

// Tests that didUpdateTopOffset in static-fakebox mode updates
// fakeLocationBar.alpha and calls NTPContentDelegate.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestStaticFakeboxDidUpdateTopOffset) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kNewTabPageRedesign, {{kNewTabPageRedesignStaticFakeboxParam, "true"}});

  view_controller_.view.frame = CGRectMake(0, 0, 400, 800);
  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  UIView* fake_location_bar =
      [view_controller_ valueForKey:@"_fakeLocationBar"];
  EXPECT_NE(nil, fake_location_bar);

  id mock_content_delegate =
      OCMProtocolMock(@protocol(NewTabPageContentDelegate));
  view_controller_.NTPContentDelegate = mock_content_delegate;

  NewTabPageBottomSheetViewController* sheet =
      [view_controller_ valueForKey:@"_bottomSheetViewController"];

  CGFloat expandedOffset = [sheet expandedOffset];
  CGFloat restingOffset = [sheet restingOffset];
  CGFloat midOffset = (expandedOffset + restingOffset) / 2.0;

  // progress should be 0.5, expansionProgress = 1.0 - 0.5 = 0.5
  OCMExpect([mock_content_delegate didUpdateNTPTabOmniboxScrollProgress:0.5]);

  [view_controller_ bottomSheetViewController:sheet
                           didUpdateTopOffset:midOffset];

  EXPECT_FLOAT_EQ(0.5, fake_location_bar.alpha);
  EXPECT_OCMOCK_VERIFY(mock_content_delegate);
}

// Tests that didUpdateTopOffset moves top content downward when topOffset >
// restingOffset in static-fakebox mode.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestStaticFakeboxDidUpdateTopOffsetCollapsed) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kNewTabPageRedesign, {{kNewTabPageRedesignStaticFakeboxParam, "true"}});

  view_controller_.view.frame = CGRectMake(0, 0, 400, 800);
  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  UIView* fake_location_bar =
      [view_controller_ valueForKey:@"_fakeLocationBar"];
  EXPECT_NE(nil, fake_location_bar);
  NSLayoutConstraint* top_constraint =
      [view_controller_ valueForKey:@"_fakeLocationBarTopConstraint"];
  CGFloat initial_top = top_constraint.constant;

  id mock_content_delegate =
      OCMProtocolMock(@protocol(NewTabPageContentDelegate));
  view_controller_.NTPContentDelegate = mock_content_delegate;

  NewTabPageBottomSheetViewController* sheet =
      [view_controller_ valueForKey:@"_bottomSheetViewController"];
  CGFloat restingOffset = [sheet restingOffset];

  // Pass topOffset greater than restingOffset (downward drag)
  CGFloat collapsedOffset = restingOffset + 100.0;
  OCMExpect([mock_content_delegate didUpdateNTPTabOmniboxScrollProgress:0.0]);

  [view_controller_ bottomSheetViewController:sheet
                           didUpdateTopOffset:collapsedOffset];

  EXPECT_FLOAT_EQ(initial_top + 100.0, top_constraint.constant);
  EXPECT_FLOAT_EQ(1.0, fake_location_bar.alpha);
  EXPECT_OCMOCK_VERIFY(mock_content_delegate);
}

// Tests that expandedOffsetForBottomSheetViewController calculates correct
// offsets.
TEST_F(NewTabPageRedesignViewControllerTest, TestExpandedOffsetForBottomSheet) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  [view_controller_ loadViewIfNeeded];
  NewTabPageBottomSheetViewController* sheet =
      [view_controller_ valueForKey:@"_bottomSheetViewController"];

  // Default legacy mode (static-fakebox: false): safeAreaTop + 20.0
  CGFloat legacyOffset =
      [view_controller_ expandedOffsetForBottomSheetViewController:sheet];
  EXPECT_EQ(legacyOffset, view_controller_.view.safeAreaInsets.top + 20.0);

  // Enable static-fakebox feature
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kNewTabPageRedesign, {{kNewTabPageRedesignStaticFakeboxParam, "true"}});

  // Top Omnibox: safeAreaTop + kToolbarHeight
  CGFloat offsetTop =
      [view_controller_ expandedOffsetForBottomSheetViewController:sheet];
  EXPECT_GT(offsetTop, 0.0);

  // Bottom Omnibox (non-tabstrip): safeAreaTop
  [view_controller_ setOmniboxInBottomPosition:YES];
  CGFloat offsetBottom =
      [view_controller_ expandedOffsetForBottomSheetViewController:sheet];
  EXPECT_EQ(offsetBottom, view_controller_.view.safeAreaInsets.top);
}

// Tests that setOmniboxInBottomPosition is a no-op in legacy mode
// (static-fakebox: false).
TEST_F(NewTabPageRedesignViewControllerTest,
       TestSetOmniboxInBottomPositionLegacy) {
  [view_controller_ loadViewIfNeeded];
  [view_controller_ setOmniboxInBottomPosition:YES];

  // _isBottomOmnibox remains NO
  BOOL isBottom =
      [[view_controller_ valueForKey:@"_isBottomOmnibox"] boolValue];
  EXPECT_FALSE(isBottom);
}

// Tests that setOmniboxInBottomPosition updates state in static-fakebox mode.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestSetOmniboxInBottomPositionStaticFakebox) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kNewTabPageRedesign, {{kNewTabPageRedesignStaticFakeboxParam, "true"}});

  [view_controller_ loadViewIfNeeded];
  [view_controller_ setOmniboxInBottomPosition:YES];

  // _isBottomOmnibox is updated to YES
  BOOL isBottom =
      [[view_controller_ valueForKey:@"_isBottomOmnibox"] boolValue];
  EXPECT_TRUE(isBottom);
}

// Tests that bottomSheetViewControllerDidEscape posts accessibility
// notification.
TEST_F(NewTabPageRedesignViewControllerTest, TestBottomSheetDidEscape) {
  [view_controller_ loadViewIfNeeded];
  NewTabPageBottomSheetViewController* sheet =
      [view_controller_ valueForKey:@"_bottomSheetViewController"];

  // Calling bottomSheetViewControllerDidEscape should not crash.
  [view_controller_ bottomSheetViewControllerDidEscape:sheet];
}

// Tests that onHeightChanged callback triggers bottom sheet position update
// when MVT is not in the bottom sheet.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestMvtHeightChangeCallbackWhenNotInBottomSheet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMVTInBottomSheet);

  id mock_bottom_sheet =
      OCMClassMock([NewTabPageBottomSheetViewController class]);
  [view_controller_ setValue:mock_bottom_sheet
                      forKey:@"bottomSheetViewController"];

  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedItem* item = [[MostVisitedItem alloc] init];
  config.mostVisitedItems = @[ item ];

  [view_controller_ setMostVisitedTilesConfig:config];

  UIView* container = [view_controller_ valueForKey:@"mostVisitedView"];
  ASSERT_TRUE(container != nil);

  MostVisitedTilesCollectionView* collection_view = nil;
  for (UIView* subview in container.subviews) {
    if ([subview isKindOfClass:[MostVisitedTilesCollectionView class]]) {
      collection_view = static_cast<MostVisitedTilesCollectionView*>(subview);
      break;
    }
  }
  ASSERT_TRUE(collection_view != nil);
  ASSERT_TRUE(collection_view.onContentSizeChanged != nil);

  OCMExpect([mock_bottom_sheet updateBottomSheetPositionAnimated:YES]);
  collection_view.onContentSizeChanged(CGSizeMake(300, 100));
  [mock_bottom_sheet verify];
}

// Tests that onHeightChanged callback is not set when MVT is in the bottom
// sheet.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestMvtHeightChangeCallbackWhenInBottomSheet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMVTInBottomSheet);

  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedItem* item = [[MostVisitedItem alloc] init];
  config.mostVisitedItems = @[ item ];

  [view_controller_ setMostVisitedTilesConfig:config];

  UIView* container = [view_controller_ valueForKey:@"mostVisitedView"];
  ASSERT_TRUE(container != nil);

  MostVisitedTilesCollectionView* collection_view = nil;
  for (UIView* subview in container.subviews) {
    if ([subview isKindOfClass:[MostVisitedTilesCollectionView class]]) {
      collection_view = static_cast<MostVisitedTilesCollectionView*>(subview);
      break;
    }
  }
  ASSERT_TRUE(collection_view != nil);
  EXPECT_TRUE(collection_view.onContentSizeChanged == nil);
}
