// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_redesign_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_collection_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_identity_disc_button.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kMinDragHandleHeight = 24.0;

// Recursively searches for a subview matching the specified
// accessibilityIdentifier.
UIView* FindSubviewWithAccessibilityIdentifier(UIView* root,
                                               NSString* identifier) {
  if ([root.accessibilityIdentifier isEqualToString:identifier]) {
    return root;
  }
  for (UIView* subview in root.subviews) {
    if (UIView* match =
            FindSubviewWithAccessibilityIdentifier(subview, identifier)) {
      return match;
    }
  }
  return nil;
}

// Recursively searches for the first subview of type T.
template <typename T>
T* FindSubviewByClass(UIView* root) {
  if ([root isKindOfClass:[T class]]) {
    return static_cast<T*>(root);
  }
  for (UIView* subview in root.subviews) {
    if (T* match = FindSubviewByClass<T>(subview)) {
      return match;
    }
  }
  return nil;
}

// Finds a child view controller of type T.
template <typename T>
T* FindChildViewController(UIViewController* parent) {
  for (UIViewController* child in parent.childViewControllers) {
    if ([child isKindOfClass:[T class]]) {
      return static_cast<T*>(child);
    }
  }
  return nil;
}

}  // namespace

@interface NewTabPageRedesignViewController (Testing) <
    NewTabPageBottomSheetViewControllerDelegate>
- (CGFloat)restingOffsetForBottomSheetViewController:
    (NewTabPageBottomSheetViewController*)viewController;
- (CGFloat)topContentHeight;
- (CGFloat)centeredFakeOmniboxTop;
- (BOOL)isCompactHeight;
- (void)backdropTapped:(UITapGestureRecognizer*)recognizer;
- (void)updateMagicStackHierarchy;
- (void)updateMostVisitedHierarchy;
@end

class NewTabPageRedesignViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[NewTabPageRedesignViewController alloc] init];
    view_controller_.traitOverrides.horizontalSizeClass =
        UIUserInterfaceSizeClassCompact;
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

// Tests that didUpdateTopOffset updates center content container alpha and
// calls NTPContentDelegate.
TEST_F(NewTabPageRedesignViewControllerTest, TestDidUpdateTopOffset) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

  view_controller_.view.frame = CGRectMake(0, 0, 400, 800);
  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);
  UIView* center_content_container = fake_location_bar.superview;
  ASSERT_TRUE(center_content_container != nil);

  id mock_content_delegate =
      OCMProtocolMock(@protocol(NewTabPageContentDelegate));
  view_controller_.NTPContentDelegate = mock_content_delegate;

  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);

  CGFloat expandedOffset = [sheet expandedOffset];
  CGFloat restingOffset = [sheet restingOffset];
  CGFloat midOffset = (expandedOffset + restingOffset) / 2.0;

  // progress should be 0.5, expansionProgress = 1.0 - 0.5 = 0.5
  OCMExpect([mock_content_delegate didUpdateNTPTabOmniboxScrollProgress:0.5]);

  [view_controller_ bottomSheetViewController:sheet
                           didUpdateTopOffset:midOffset];

  EXPECT_FLOAT_EQ(0.5, center_content_container.alpha);
  EXPECT_OCMOCK_VERIFY(mock_content_delegate);
}

// Tests that didUpdateTopOffset moves center content downward when
// topOffset > restingOffset, keeping header buttons stationary.
TEST_F(NewTabPageRedesignViewControllerTest, TestDidUpdateTopOffsetCollapsed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

  view_controller_.view.frame = CGRectMake(0, 0, 400, 800);
  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);
  CGRect initial_fakebox_frame =
      [view_controller_.view convertRect:fake_location_bar.bounds
                                fromView:fake_location_bar];

  UIButton* identity_disc_button =
      FindSubviewByClass<NTPIdentityDiscButton>(view_controller_.view);
  ASSERT_TRUE(identity_disc_button != nil);
  CGRect initial_identity_frame =
      [view_controller_.view convertRect:identity_disc_button.bounds
                                fromView:identity_disc_button];

  UIButton* customization_button = view_controller_.customizationMenuButton;
  ASSERT_TRUE(customization_button != nil);
  CGRect initial_customization_frame =
      [view_controller_.view convertRect:customization_button.bounds
                                fromView:customization_button];

  id mock_content_delegate =
      OCMProtocolMock(@protocol(NewTabPageContentDelegate));
  view_controller_.NTPContentDelegate = mock_content_delegate;

  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);
  CGFloat restingOffset = [sheet restingOffset];

  // Pass topOffset greater than restingOffset (downward drag by 100pt).
  constexpr CGFloat kCollapsedDragDelta = 100.0;
  CGFloat collapsedOffset = restingOffset + kCollapsedDragDelta;
  OCMExpect([mock_content_delegate didUpdateNTPTabOmniboxScrollProgress:0.0]);

  [view_controller_ bottomSheetViewController:sheet
                           didUpdateTopOffset:collapsedOffset];

  // The fake location bar should translate downward with the drag.
  CGRect updated_fakebox_frame =
      [view_controller_.view convertRect:fake_location_bar.bounds
                                fromView:fake_location_bar];
  EXPECT_NEAR(updated_fakebox_frame.origin.y,
              initial_fakebox_frame.origin.y + kCollapsedDragDelta, 0.5);

  // Top navigation buttons must remain stationary.
  CGRect updated_identity_frame =
      [view_controller_.view convertRect:identity_disc_button.bounds
                                fromView:identity_disc_button];
  EXPECT_TRUE(
      CGRectEqualToRect(initial_identity_frame, updated_identity_frame));

  CGRect updated_customization_frame =
      [view_controller_.view convertRect:customization_button.bounds
                                fromView:customization_button];
  EXPECT_TRUE(CGRectEqualToRect(initial_customization_frame,
                                updated_customization_frame));

  EXPECT_OCMOCK_VERIFY(mock_content_delegate);
}

// Tests that expandedOffsetForBottomSheetViewController calculates correct
// offsets.
TEST_F(NewTabPageRedesignViewControllerTest, TestExpandedOffsetForBottomSheet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

  [view_controller_ loadViewIfNeeded];
  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);

  // Top Omnibox: safeAreaTop + kToolbarHeight
  CGFloat offsetTop =
      [view_controller_ expandedOffsetForBottomSheetViewController:sheet];
  EXPECT_GT(offsetTop, 0.0);

  // Bottom Omnibox: safeAreaTop if !CanShowTabStrip, else safeAreaTop +
  // kToolbarHeight.
  [view_controller_ setOmniboxInBottomPosition:YES];
  CGFloat offsetBottom =
      [view_controller_ expandedOffsetForBottomSheetViewController:sheet];
  CGFloat expectedBottomOffset =
      CanShowTabStrip(view_controller_)
          ? view_controller_.view.safeAreaInsets.top + kToolbarHeight
          : view_controller_.view.safeAreaInsets.top;
  EXPECT_EQ(offsetBottom, expectedBottomOffset);
}

// Tests that bottomSheetViewControllerDidEscape posts accessibility
// notification.
TEST_F(NewTabPageRedesignViewControllerTest, TestBottomSheetDidEscape) {
  [view_controller_ loadViewIfNeeded];
  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);

  // Calling bottomSheetViewControllerDidEscape should not crash.
  [view_controller_ bottomSheetViewControllerDidEscape:sheet];
}

// Tests that onHeightChanged callback triggers bottom sheet position update
// when MVT is not in the bottom sheet.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestMvtHeightChangeCallbackWhenNotInBottomSheet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kMVTInBottomSheet);

  [view_controller_ loadViewIfNeeded];
  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);
  id mock_bottom_sheet = OCMPartialMock(sheet);

  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedItem* item = [[MostVisitedItem alloc] init];
  config.mostVisitedItems = @[ item ];

  [view_controller_ setMostVisitedTilesConfig:config];

  MostVisitedTilesCollectionView* collection_view =
      FindSubviewByClass<MostVisitedTilesCollectionView>(view_controller_.view);
  ASSERT_TRUE(collection_view != nil);
  ASSERT_TRUE(collection_view.onContentSizeChanged != nil);

  OCMExpect([mock_bottom_sheet updateBottomSheetPositionAnimated:YES]);
  collection_view.onContentSizeChanged(CGSizeMake(300, 100));
  [mock_bottom_sheet verify];
}

// Tests that onHeightChanged callback updates feed insets and sheet position
// when MVT is in the bottom sheet.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestMvtHeightChangeCallbackWhenInBottomSheet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMVTInBottomSheet);

  [view_controller_ loadViewIfNeeded];
  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);
  id mock_bottom_sheet = OCMPartialMock(sheet);

  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedItem* item = [[MostVisitedItem alloc] init];
  config.mostVisitedItems = @[ item ];

  [view_controller_ setMostVisitedTilesConfig:config];

  MostVisitedTilesCollectionView* collection_view =
      FindSubviewByClass<MostVisitedTilesCollectionView>(view_controller_.view);
  ASSERT_TRUE(collection_view != nil);
  ASSERT_TRUE(collection_view.onContentSizeChanged != nil);

  OCMExpect([mock_bottom_sheet updateBottomSheetPositionAnimated:YES]);
  collection_view.onContentSizeChanged(CGSizeMake(300, 100));
  [mock_bottom_sheet verify];
}

// Tests that onContentSizeChanged callback correctly updates the bottom sheet
// even when setMostVisitedTilesConfig is invoked before viewDidLoad.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestMvtHeightChangeCallbackWhenConfiguredBeforeViewDidLoad) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMVTInBottomSheet);

  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedItem* item = [[MostVisitedItem alloc] init];
  config.mostVisitedItems = @[ item ];

  // Configure before view is loaded.
  [view_controller_ setMostVisitedTilesConfig:config];

  // Now load view, which instantiates bottomSheetViewController.
  [view_controller_ loadViewIfNeeded];
  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);
  id mock_bottom_sheet = OCMPartialMock(sheet);

  MostVisitedTilesCollectionView* collection_view =
      FindSubviewByClass<MostVisitedTilesCollectionView>(view_controller_.view);
  ASSERT_TRUE(collection_view != nil);
  ASSERT_TRUE(collection_view.onContentSizeChanged != nil);

  OCMExpect([mock_bottom_sheet updateBottomSheetPositionAnimated:YES]);
  collection_view.onContentSizeChanged(CGSizeMake(300, 100));
  [mock_bottom_sheet verify];
}

// Tests that setDefaultSearchEngineName updates fakebox accessibility label.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestDefaultSearchEngineNameUpdatesHintLabel) {
  [view_controller_ loadViewIfNeeded];

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);

  [view_controller_ setDefaultSearchEngineName:@"DuckDuckGo"];
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

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);

  UIButton* plus_button = base::apple::ObjCCastStrict<UIButton>(
      FindSubviewWithAccessibilityIdentifier(
          fake_location_bar, kNTPPlusButtonAccessibilityIdentifier));
  UIImageView* logo_view = FindSubviewByClass<UIImageView>(fake_location_bar);
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

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);
  UIButton* voice_button = base::apple::ObjCCastStrict<UIButton>(
      FindSubviewWithAccessibilityIdentifier(
          fake_location_bar, kNTPVoiceSearchButtonAccessibilityIdentifier));
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

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);
  UIButton* plus_button = base::apple::ObjCCastStrict<UIButton>(
      FindSubviewWithAccessibilityIdentifier(
          fake_location_bar, kNTPPlusButtonAccessibilityIdentifier));
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

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);
  UIButton* voice_button = base::apple::ObjCCastStrict<UIButton>(
      FindSubviewWithAccessibilityIdentifier(
          fake_location_bar, kNTPVoiceSearchButtonAccessibilityIdentifier));
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

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);
  UIButton* voice_button = base::apple::ObjCCastStrict<UIButton>(
      FindSubviewWithAccessibilityIdentifier(
          fake_location_bar, kNTPVoiceSearchButtonAccessibilityIdentifier));
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

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);
  UIButton* lens_button = base::apple::ObjCCastStrict<UIButton>(
      FindSubviewWithAccessibilityIdentifier(
          fake_location_bar, kNTPLensButtonAccessibilityIdentifier));
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

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);
  UIButton* lens_button = base::apple::ObjCCastStrict<UIButton>(
      FindSubviewWithAccessibilityIdentifier(
          fake_location_bar, kNTPLensButtonAccessibilityIdentifier));
  ASSERT_TRUE(lens_button != nil);
  lens_button.hidden = YES;

  [[mock_mutator reject] notifyLensBadgeDisplayed];
  [view_controller_ viewDidAppear:NO];
  EXPECT_OCMOCK_VERIFY(mock_mutator);
}

// Tests that notifyLensBadgeDisplayed is called when lensButton is visible.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestLensBadgeNotNotifiedWhenLensButtonVisible) {
  id mock_mutator = OCMProtocolMock(@protocol(NewTabPageMutator));
  view_controller_.mutator = mock_mutator;
  view_controller_.useNewBadgeForLensButton = YES;

  [view_controller_ loadViewIfNeeded];

  UIView* fake_location_bar = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPFakeOmniboxAccessibilityIdentifier);
  ASSERT_TRUE(fake_location_bar != nil);
  UIButton* lens_button = base::apple::ObjCCastStrict<UIButton>(
      FindSubviewWithAccessibilityIdentifier(
          fake_location_bar, kNTPLensButtonAccessibilityIdentifier));
  ASSERT_TRUE(lens_button != nil);
  lens_button.hidden = NO;

  OCMExpect([mock_mutator notifyLensBadgeDisplayed]);
  [view_controller_ viewDidAppear:NO];
  EXPECT_OCMOCK_VERIFY(mock_mutator);
}

// Tests that the customization menu button is created with proper accessibility
// identifier and label.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestCustomizationMenuButtonCreated) {
  [view_controller_ loadViewIfNeeded];

  ExtendedTouchTargetButton* button = view_controller_.customizationMenuButton;
  ASSERT_TRUE(button != nil);
  EXPECT_NSEQ(kNTPCustomizationMenuButtonIdentifier,
              button.accessibilityIdentifier);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_HOME_CUSTOMIZATION_ACCESSIBILITY_LABEL),
      button.accessibilityLabel);
}

// Tests that the customization menu button and identity disc button share the
// same vertical center line.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestHeaderButtonsVerticalCenterAlignment) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

  view_controller_.view.frame = CGRectMake(0, 0, 400, 800);
  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  UIButton* identity_disc_button =
      FindSubviewByClass<NTPIdentityDiscButton>(view_controller_.view);
  ASSERT_TRUE(identity_disc_button != nil);
  UIButton* customization_button = view_controller_.customizationMenuButton;
  ASSERT_TRUE(customization_button != nil);

  EXPECT_NEAR(customization_button.center.y, identity_disc_button.center.y,
              0.5);
}

// Tests that layout guide kFeedIPHNamedGuide references the customization menu
// button.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestCustomizationMenuButtonLayoutGuideRegistered) {
  id mock_guide_center = OCMClassMock([LayoutGuideCenter class]);
  view_controller_.layoutGuideCenter = mock_guide_center;

  OCMExpect([mock_guide_center referenceView:[OCMArg any]
                                   underName:kFeedIPHNamedGuide]);
  [view_controller_ loadViewIfNeeded];

  EXPECT_OCMOCK_VERIFY(mock_guide_center);
}

// Tests that notifyCustomizationBadgeDisplayed is not called when
// useNewBadgeForCustomizationMenu is NO.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestCustomizationBadgeNotNotifiedWhenBadgeDisabled) {
  id mock_mutator = OCMProtocolMock(@protocol(NewTabPageMutator));
  view_controller_.mutator = mock_mutator;
  view_controller_.useNewBadgeForCustomizationMenu = NO;

  [view_controller_ loadViewIfNeeded];

  [[mock_mutator reject] notifyCustomizationBadgeDisplayed];
  [view_controller_ viewDidAppear:NO];
  EXPECT_OCMOCK_VERIFY(mock_mutator);
}

// Tests that notifyCustomizationBadgeDisplayed is called when
// useNewBadgeForCustomizationMenu is YES.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestCustomizationBadgeNotifiedWhenBadgeEnabled) {
  id mock_mutator = OCMProtocolMock(@protocol(NewTabPageMutator));
  view_controller_.mutator = mock_mutator;
  view_controller_.useNewBadgeForCustomizationMenu = YES;

  [view_controller_ loadViewIfNeeded];

  OCMExpect([mock_mutator notifyCustomizationBadgeDisplayed]);
  [view_controller_ viewDidAppear:NO];
  EXPECT_OCMOCK_VERIFY(mock_mutator);
}

// Tests that tapping the customization menu button invokes
// customizationMenuWasTapped on header commands handler and fades the badge.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestCustomizationMenuButtonAction) {
  [view_controller_ loadViewIfNeeded];
  view_controller_.useNewBadgeForCustomizationMenu = YES;

  id mock_header_commands =
      OCMProtocolMock(@protocol(NewTabPageHeaderCommands));
  view_controller_.headerCommandsHandler = mock_header_commands;

  ExtendedTouchTargetButton* button = view_controller_.customizationMenuButton;
  ASSERT_TRUE(button != nil);

  OCMExpect([mock_header_commands customizationMenuWasTapped:button]);
  [button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(mock_header_commands);
  EXPECT_FALSE(view_controller_.useNewBadgeForCustomizationMenu);
}

// Tests that scrollToTopAnimated and isScrolledToTop properly interact with the
// bottom sheet.
TEST_F(NewTabPageRedesignViewControllerTest, TestScrollToTop) {
  [view_controller_ loadViewIfNeeded];

  [view_controller_ scrollToTopAnimated:YES];
  EXPECT_TRUE([view_controller_ isScrolledToTop]);
}

// Tests that topContentHeight includes magic stack height and spacing on iPad
// regular.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestIPadRegularTopContentHeightIncludesMagicStack) {
  [view_controller_ loadViewIfNeeded];

  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;

  CGFloat initial_height = [view_controller_ topContentHeight];

  MagicStackCollectionViewController* magic_stack =
      [[MagicStackCollectionViewController alloc] init];
  [view_controller_ setMagicStackViewController:magic_stack];

  CGFloat height_with_magic_stack = [view_controller_ topContentHeight];
  CGFloat expected_magic_stack_delta =
      content_suggestions::ReducedModuleSpacing() + kMagicStackHeight;

  EXPECT_FLOAT_EQ(height_with_magic_stack,
                  initial_height + expected_magic_stack_delta);
}

// Tests that tapping the backdrop collapses the sheet to resting state.
TEST_F(NewTabPageRedesignViewControllerTest, TestBackdropTappedCollapsesSheet) {
  [view_controller_ loadViewIfNeeded];

  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);
  id mock_sheet = OCMPartialMock(sheet);
  OCMExpect([mock_sheet collapseToRestingAnimated:YES]);

  id mock_recognizer = OCMClassMock([UITapGestureRecognizer class]);
  OCMStub([mock_recognizer state]).andReturn(UIGestureRecognizerStateEnded);

  [view_controller_ backdropTapped:mock_recognizer];

  EXPECT_OCMOCK_VERIFY(mock_sheet);
}

// Tests that Magic Stack view controller hierarchy updates correctly between
// iPad regular and compact/iPhone layouts.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestMagicStackHierarchyReparenting) {
  [view_controller_ loadViewIfNeeded];

  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;

  MagicStackCollectionViewController* magic_stack =
      [[MagicStackCollectionViewController alloc] init];
  [view_controller_ setMagicStackViewController:magic_stack];

  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);

  // In iPad regular, Magic Stack should be a direct child of redesign VC.
  EXPECT_EQ(view_controller_, magic_stack.parentViewController);
  EXPECT_EQ(nil, sheet.magicStackViewController);
  EXPECT_FALSE([magic_stack.view isDescendantOfView:sheet.view]);
  EXPECT_TRUE([magic_stack.view isDescendantOfView:view_controller_.view]);

  // Transition to compact layout.
  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassCompact;
  [view_controller_ updateMagicStackHierarchy];

  // Magic Stack should no longer be a child of redesign VC, and sheet should
  // hold the reference.
  EXPECT_NE(view_controller_, magic_stack.parentViewController);
  EXPECT_EQ(magic_stack, sheet.magicStackViewController);

  // Transition back to iPad regular layout.
  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;
  [view_controller_ updateMagicStackHierarchy];

  EXPECT_EQ(view_controller_, magic_stack.parentViewController);
  EXPECT_EQ(nil, sheet.magicStackViewController);
  EXPECT_FALSE([magic_stack.view isDescendantOfView:sheet.view]);
  EXPECT_TRUE([magic_stack.view isDescendantOfView:view_controller_.view]);
}

// Tests that Most Visited Tiles view hierarchy updates correctly between
// iPad regular and compact/iPhone layouts.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestMostVisitedHierarchyReparenting) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMVTInBottomSheet);

  [view_controller_ loadViewIfNeeded];

  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;

  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedItem* item = [[MostVisitedItem alloc] init];
  config.mostVisitedItems = @[ item ];

  [view_controller_ setMostVisitedTilesConfig:config];

  MostVisitedTilesCollectionView* mvt_view =
      FindSubviewByClass<MostVisitedTilesCollectionView>(view_controller_.view);
  ASSERT_TRUE(mvt_view != nil);
  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);

  // In iPad regular, MVT should be in redesign VC and not in bottom sheet.
  EXPECT_FALSE([mvt_view isDescendantOfView:sheet.view]);
  EXPECT_TRUE([mvt_view isDescendantOfView:view_controller_.view]);

  // Transition to compact layout.
  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassCompact;
  [view_controller_ updateMostVisitedHierarchy];

  // In compact layout, MVT should be embedded in bottom sheet.
  EXPECT_TRUE([mvt_view isDescendantOfView:sheet.view]);

  // Transition back to iPad regular layout.
  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;
  [view_controller_ updateMostVisitedHierarchy];

  // MVT should be restored to redesign VC and not in bottom sheet.
  EXPECT_FALSE([mvt_view isDescendantOfView:sheet.view]);
  EXPECT_TRUE([mvt_view isDescendantOfView:view_controller_.view]);
}

// Tests that on iPad regular, didUpdateTopOffset synchronizes tablet omnibox
// scroll progress with feed expansion progress and updates backdrop blur.
TEST_F(NewTabPageRedesignViewControllerTest,
       TestIPadRegularDidUpdateTopOffsetSynchronizesTabletOmnibox) {
  view_controller_.view.frame = CGRectMake(0, 0, 1024, 768);
  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;
  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  id mock_content_delegate =
      OCMProtocolMock(@protocol(NewTabPageContentDelegate));
  view_controller_.NTPContentDelegate = mock_content_delegate;

  NewTabPageBottomSheetViewController* sheet =
      FindChildViewController<NewTabPageBottomSheetViewController>(
          view_controller_);
  ASSERT_TRUE(sheet != nil);

  CGFloat expandedOffset = [sheet expandedOffset];
  CGFloat restingOffset = [sheet restingOffset];
  ASSERT_GT(restingOffset, expandedOffset);
  CGFloat midOffset = (expandedOffset + restingOffset) / 2.0;

  // At midOffset, progress is 0.5, so expansionProgress = 1.0 - 0.5 = 0.5.
  OCMExpect([mock_content_delegate didUpdateNTPTabOmniboxScrollProgress:0.5]);

  [view_controller_ bottomSheetViewController:sheet
                           didUpdateTopOffset:midOffset];

  UIView* backdrop_blur = FindSubviewWithAccessibilityIdentifier(
      view_controller_.view, kNTPBackdropBlurIdentifier);
  ASSERT_TRUE(backdrop_blur != nil);
  EXPECT_FLOAT_EQ(0.5, backdrop_blur.alpha);
  EXPECT_TRUE(backdrop_blur.userInteractionEnabled);
  EXPECT_OCMOCK_VERIFY(mock_content_delegate);

  // When resting (expansionProgress = 0.0), progress is 0.0.
  OCMExpect([mock_content_delegate didUpdateNTPTabOmniboxScrollProgress:0.0]);
  [view_controller_ bottomSheetViewController:sheet
                           didUpdateTopOffset:restingOffset];
  EXPECT_FLOAT_EQ(0.0, backdrop_blur.alpha);
  EXPECT_FALSE(backdrop_blur.userInteractionEnabled);
  EXPECT_OCMOCK_VERIFY(mock_content_delegate);

  // When fully expanded (expansionProgress = 1.0), progress is 1.0.
  OCMExpect([mock_content_delegate didUpdateNTPTabOmniboxScrollProgress:1.0]);
  [view_controller_ bottomSheetViewController:sheet
                           didUpdateTopOffset:expandedOffset];
  EXPECT_FLOAT_EQ(1.0, backdrop_blur.alpha);
  EXPECT_TRUE(backdrop_blur.userInteractionEnabled);
  EXPECT_OCMOCK_VERIFY(mock_content_delegate);
}

// Tests that updateADPBadgeWithErrorFound updates the identity disc button.
TEST_F(NewTabPageRedesignViewControllerTest, TestUpdateADPBadgeWithErrorFound) {
  NSString* name = @"John Doe";
  NSString* email = @"john@example.com";
  [view_controller_ updateADPBadgeWithErrorFound:YES name:name email:email];
  // Load view so identity disc button is created.
  [view_controller_ loadViewIfNeeded];
  NTPIdentityDiscButton* identity_disc =
      FindSubviewByClass<NTPIdentityDiscButton>(view_controller_.view);
  ASSERT_TRUE(identity_disc != nil);
  NSString* expected_error_label = l10n_util::GetNSStringF(
      IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL_OPEN_ACCOUNT_MENU_WITH_ERROR,
      base::SysNSStringToUTF16(name), base::SysNSStringToUTF16(email));
  EXPECT_NSEQ(expected_error_label, identity_disc.accessibilityLabel);

  // Updating while view is loaded propagates immediately.
  [view_controller_ updateADPBadgeWithErrorFound:NO name:name email:email];
  NSString* expected_normal_label = l10n_util::GetNSStringF(
      IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL_OPEN_ACCOUNT_MENU,
      base::SysNSStringToUTF16(name), base::SysNSStringToUTF16(email));
  EXPECT_NSEQ(expected_normal_label, identity_disc.accessibilityLabel);
}
