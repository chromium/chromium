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
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
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
- (CGFloat)centeredFakeOmniboxTop;
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

// Tests that the view controller loads its view correctly with redesign
// background color.
TEST_F(NewTabPageRedesignViewControllerTest, TestLoadView) {
  [view_controller_ loadViewIfNeeded];
  EXPECT_NE(nil, view_controller_.view);
  EXPECT_NSEQ([UIColor colorNamed:kNTPRedesignBackgroundColor],
              view_controller_.view.backgroundColor);
}

// Tests that didUpdateTopOffset updates fakeLocationBar.alpha and calls
// NTPContentDelegate.
TEST_F(NewTabPageRedesignViewControllerTest, TestDidUpdateTopOffset) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

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
// restingOffset.
TEST_F(NewTabPageRedesignViewControllerTest, TestDidUpdateTopOffsetCollapsed) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

  [view_controller_ loadViewIfNeeded];
  NewTabPageBottomSheetViewController* sheet =
      [view_controller_ valueForKey:@"_bottomSheetViewController"];

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

// Tests that setOmniboxInBottomPosition updates state.
TEST_F(NewTabPageRedesignViewControllerTest, TestSetOmniboxInBottomPosition) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

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

// Tests that centeredFakeOmniboxTop and restingOffset remain identical between
// Logo and Doodle when kConsistentLogoDoodleHeight is enabled.
TEST_F(NewTabPageRedesignViewControllerTest, TestConsistentLogoDoodleHeight) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kConsistentLogoDoodleHeight);

  [view_controller_ loadViewIfNeeded];

  [view_controller_
      searchEngineLogoStateDidChange:SearchEngineLogoState::kLogo];
  CGFloat omnibox_top_with_logo = [view_controller_ centeredFakeOmniboxTop];
  CGFloat resting_offset_with_logo =
      [view_controller_ restingOffsetForBottomSheetViewController:nil];

  [view_controller_
      searchEngineLogoStateDidChange:SearchEngineLogoState::kDoodle];
  CGFloat omnibox_top_with_doodle = [view_controller_ centeredFakeOmniboxTop];
  CGFloat resting_offset_with_doodle =
      [view_controller_ restingOffsetForBottomSheetViewController:nil];

  EXPECT_EQ(omnibox_top_with_logo, omnibox_top_with_doodle);
  EXPECT_EQ(resting_offset_with_logo, resting_offset_with_doodle);
}

// Tests that fakebox subviews are created once and not re-created on setter
// calls.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestFakeboxSubviewsReusedOnStateChange) {
  [view_controller_ loadViewIfNeeded];

  UIView* plus_button = [view_controller_ valueForKey:@"_plusButton"];
  UIView* logo_view = [view_controller_ valueForKey:@"_logoView"];
  UIView* voice_button = [view_controller_ valueForKey:@"_voiceSearchButton"];
  UIView* hint_label = [view_controller_ valueForKey:@"_hintLabel"];

  ASSERT_TRUE(plus_button != nil);
  ASSERT_TRUE(logo_view != nil);
  ASSERT_TRUE(voice_button != nil);
  ASSERT_TRUE(hint_label != nil);

  // Trigger state updates
  [view_controller_ setDefaultSearchEngineName:@"Yahoo"];
  [view_controller_ setVoiceSearchIsEnabled:YES];
  [view_controller_ setVoiceSearchIsEnabled:NO];
  [view_controller_ setAIMAllowed:YES];
  [view_controller_ setFuseboxEligible:YES];

  // Subviews should be identical instances (not reallocated)
  EXPECT_EQ(plus_button, [view_controller_ valueForKey:@"_plusButton"]);
  EXPECT_EQ(logo_view, [view_controller_ valueForKey:@"_logoView"]);
  EXPECT_EQ(voice_button, [view_controller_ valueForKey:@"_voiceSearchButton"]);
  EXPECT_EQ(hint_label, [view_controller_ valueForKey:@"_hintLabel"]);
}

// Tests that setDefaultSearchEngineName updates hint label text and
// accessibility label.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestDefaultSearchEngineNameUpdatesHintLabel) {
  [view_controller_ loadViewIfNeeded];

  UILabel* hint_label = [view_controller_ valueForKey:@"_hintLabel"];
  UIView* fake_location_bar =
      [view_controller_ valueForKey:@"_fakeLocationBar"];
  ASSERT_TRUE(hint_label != nil);
  ASSERT_TRUE(fake_location_bar != nil);

  [view_controller_ setDefaultSearchEngineName:@"DuckDuckGo"];
  EXPECT_TRUE([hint_label.text containsString:@"DuckDuckGo"]);
  EXPECT_TRUE(
      [fake_location_bar.accessibilityLabel containsString:@"DuckDuckGo"]);
}

// Tests that toggling AIM and fusebox eligibility toggles plusButton vs
// logoView visibility.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestPlusButtonVsLogoViewVisibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kPlusButtonInFakebox},
      /*disabled_features=*/{});

  [view_controller_ loadViewIfNeeded];

  UIButton* plus_button = [view_controller_ valueForKey:@"_plusButton"];
  UIImageView* logo_view = [view_controller_ valueForKey:@"_logoView"];
  ASSERT_TRUE(plus_button != nil);
  ASSERT_TRUE(logo_view != nil);

  // Initially AIM and fusebox not allowed -> Logo shown, plus hidden
  EXPECT_TRUE(plus_button.hidden);
  EXPECT_FALSE(logo_view.hidden);

  // Enable AIM and Fusebox -> Plus shown, logo hidden
  [view_controller_ setAIMAllowed:YES];
  [view_controller_ setFuseboxEligible:YES];
  EXPECT_FALSE(plus_button.hidden);
  EXPECT_TRUE(logo_view.hidden);

  // Disable Fusebox -> Logo shown, plus hidden
  [view_controller_ setFuseboxEligible:NO];
  EXPECT_TRUE(plus_button.hidden);
  EXPECT_FALSE(logo_view.hidden);
}

// Tests that setVoiceSearchIsEnabled updates voice search button state.
TEST_F(NewTabPageRedesignViewControllerTest, TestSetVoiceSearchIsEnabled) {
  [view_controller_ loadViewIfNeeded];

  UIButton* voice_button = [view_controller_ valueForKey:@"_voiceSearchButton"];
  ASSERT_TRUE(voice_button != nil);

  [view_controller_ setVoiceSearchIsEnabled:YES];
  EXPECT_TRUE(voice_button.enabled);
  EXPECT_TRUE(voice_button.isAccessibilityElement);

  [view_controller_ setVoiceSearchIsEnabled:NO];
  EXPECT_FALSE(voice_button.enabled);
  EXPECT_FALSE(voice_button.isAccessibilityElement);
}

// Tests that tapping the plus button invokes openMultimodalActionsMenu on
// shortcuts handler.
TEST_F(NewTabPageRedesignViewControllerTest, TestPlusButtonAction) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kPlusButtonInFakebox},
      /*disabled_features=*/{});

  [view_controller_ loadViewIfNeeded];
  [view_controller_ setAIMAllowed:YES];
  [view_controller_ setFuseboxEligible:YES];

  id mock_shortcuts_handler =
      OCMProtocolMock(@protocol(NewTabPageShortcutsHandler));
  view_controller_.NTPShortcutsHandler = mock_shortcuts_handler;

  UIButton* plus_button = [view_controller_ valueForKey:@"_plusButton"];
  ASSERT_TRUE(plus_button != nil);

  OCMExpect([mock_shortcuts_handler openMultimodalActionsMenu]);
  [plus_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mock_shortcuts_handler);
}

// Tests that tapping the voice search button invokes loadVoiceSearchFromView on
// shortcuts handler.
TEST_F(NewTabPageRedesignViewControllerTest, TestVoiceSearchButtonAction) {
  [view_controller_ loadViewIfNeeded];
  [view_controller_ setVoiceSearchIsEnabled:YES];

  id mock_shortcuts_handler =
      OCMProtocolMock(@protocol(NewTabPageShortcutsHandler));
  view_controller_.NTPShortcutsHandler = mock_shortcuts_handler;

  UIButton* voice_button = [view_controller_ valueForKey:@"_voiceSearchButton"];
  ASSERT_TRUE(voice_button != nil);

  OCMExpect([mock_shortcuts_handler preloadVoiceSearch]);
  OCMExpect([mock_shortcuts_handler loadVoiceSearchFromView:voice_button]);
  [voice_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mock_shortcuts_handler);
}

// Tests that touching down on the voice search button invokes
// preloadVoiceSearch on shortcuts handler.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestVoiceSearchButtonTouchDownAction) {
  [view_controller_ loadViewIfNeeded];
  [view_controller_ setVoiceSearchIsEnabled:YES];

  id mock_shortcuts_handler =
      OCMProtocolMock(@protocol(NewTabPageShortcutsHandler));
  view_controller_.NTPShortcutsHandler = mock_shortcuts_handler;

  UIButton* voice_button = [view_controller_ valueForKey:@"_voiceSearchButton"];
  ASSERT_TRUE(voice_button != nil);

  OCMExpect([mock_shortcuts_handler preloadVoiceSearch]);
  [voice_button sendActionsForControlEvents:UIControlEventTouchDown];
  EXPECT_OCMOCK_VERIFY(mock_shortcuts_handler);
}

// Tests that tapping the Lens button invokes openLensViewFinder on shortcuts
// handler.
TEST_F(NewTabPageRedesignViewControllerTest, TestLensButtonAction) {
  [view_controller_ loadViewIfNeeded];

  id mock_shortcuts_handler =
      OCMProtocolMock(@protocol(NewTabPageShortcutsHandler));
  view_controller_.NTPShortcutsHandler = mock_shortcuts_handler;

  UIButton* lens_button = [view_controller_ valueForKey:@"_lensButton"];
  ASSERT_TRUE(lens_button != nil);

  OCMExpect([mock_shortcuts_handler openLensViewFinder]);
  [lens_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mock_shortcuts_handler);
}

// Tests that notifyLensBadgeDisplayed is not called when lensButton is hidden.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestLensBadgeNotNotifiedWhenLensButtonHidden) {
  id mock_mutator = OCMProtocolMock(@protocol(NewTabPageMutator));
  view_controller_.mutator = mock_mutator;
  view_controller_.useNewBadgeForLensButton = YES;

  [view_controller_ loadViewIfNeeded];

  UIButton* lens_button = [view_controller_ valueForKey:@"_lensButton"];
  ASSERT_TRUE(lens_button != nil);
  lens_button.hidden = YES;

  [[mock_mutator reject] notifyLensBadgeDisplayed];
  [view_controller_ viewDidAppear:NO];
  EXPECT_OCMOCK_VERIFY(mock_mutator);
}

// Tests that notifyLensBadgeDisplayed is called when lensButton is visible.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestLensBadgeNotifiedWhenLensButtonVisible) {
  id mock_mutator = OCMProtocolMock(@protocol(NewTabPageMutator));
  view_controller_.mutator = mock_mutator;
  view_controller_.useNewBadgeForLensButton = YES;

  [view_controller_ loadViewIfNeeded];

  UIButton* lens_button = [view_controller_ valueForKey:@"_lensButton"];
  ASSERT_TRUE(lens_button != nil);
  lens_button.hidden = NO;

  OCMExpect([mock_mutator notifyLensBadgeDisplayed]);
  [view_controller_ viewDidAppear:NO];
  EXPECT_OCMOCK_VERIFY(mock_mutator);
}
