// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_view_controller.h"

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_button_factory.h"
#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_view_controller.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_mutator.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_visibility_delegate.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_item.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_mutator.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Interface to expose functions for testing.
@interface LocationBarBadgeViewController (Testing)
- (void)userTappedBadge;
- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered;
- (void)dismissIPHAnimated:(BOOL)animated;
- (void)didCollapseBadgeContainer:(LocationBarBadgeType)badgeType;
@end

namespace {

NSString* const kLocationBarBadgeOverflowIdentifier =
    @"LocationBarBadgeOverflowIdentifier";
NSString* const kLocationBarBadgeReaderModeIdentifier =
    @"LocationBarBadgeReaderModeIdentifier";
NSString* const kOverflowAccessibilityLabel = @"Overflow";
NSString* const kOverflowLabelText = @"More";
NSString* const kReaderAccessibilityLabel = @"Reader mode";
NSString* const kReaderLabelText = @"Reader";

}  // namespace

// Test fixture for LocationBarBadgeViewController.
class LocationBarBadgeViewControllerTest : public PlatformTest {
 protected:
  LocationBarBadgeViewControllerTest() {
    view_controller_ = [[LocationBarBadgeViewController alloc] init];
    mock_mutator_ = OCMProtocolMock(@protocol(LocationBarBadgeMutator));
    view_controller_.mutator = mock_mutator_;
    mock_contextual_panel_mutator_ =
        OCMProtocolMock(@protocol(ContextualPanelEntrypointMutator));
    view_controller_.contextualPanelEntryPointMutator =
        mock_contextual_panel_mutator_;
  }

  // Returns a Reader mode `LocationBarBadgeItem` with `icon` and
  // `display_state`, using fixed label and accessibility values and no unread
  // indicator.
  LocationBarBadgeItem* CreateBadgeItem(
      UIImage* icon,
      LocationBarBadgeDisplayState display_state) {
    return [[LocationBarBadgeItem alloc]
              initWithBadgeType:LocationBarBadgeType::kReaderMode
                      iconImage:icon
                      labelText:kReaderLabelText
        accessibilityIdentifier:kLocationBarBadgeReaderModeIdentifier
             accessibilityLabel:kReaderAccessibilityLabel
             hasUnreadIndicator:NO
                   displayState:display_state];
  }

  // Configures `view_controller_` with an `IncognitoBadgeViewController`.
  void ConfigureIncognitoBadgeViewController() {
    BadgeButtonFactory* factory = [[BadgeButtonFactory alloc] init];
    incognito_badge_view_controller_ =
        [[IncognitoBadgeViewController alloc] initWithButtonFactory:factory];
    view_controller_.incognitoBadgeViewController =
        incognito_badge_view_controller_;
  }

  LocationBarBadgeViewController* view_controller_;
  IncognitoBadgeViewController* incognito_badge_view_controller_;
  id mock_mutator_;
  id mock_contextual_panel_mutator_;
};

// Tests that `didCompleteTransitionToSmallEntrypoint` is called after the badge
// container has been collapsed for a supported contextual panel badge.
TEST_F(LocationBarBadgeViewControllerTest,
       DidCompleteTransitionToSmallEntrypoint) {
  // Create the conditions in which the Ask Gemini badge is not available, but
  // the Page Action Menu supports other contextual panel badges.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kPageActionMenu);

  [view_controller_ loadViewIfNeeded];
  ContextualPanelItemConfiguration configuration(
      ContextualPanelItemType::SamplePanelItem);
  configuration.entrypoint_symbol = SymbolChromeProduct;
  [view_controller_ setEntrypointConfig:&configuration];

  OCMExpect(
      [mock_contextual_panel_mutator_ didCompleteTransitionToSmallEntrypoint]);

  [view_controller_ didCollapseBadgeContainer:
                        LocationBarBadgeType::kContextualPanelEntryPointSample];

  EXPECT_OCMOCK_VERIFY(mock_contextual_panel_mutator_);
}

// Tests that the badge shows and hides correctly and notifies
// `visibilityDelegate` before mutating `view.hidden`.
TEST_F(LocationBarBadgeViewControllerTest, ShowAndHideBadge) {
  [view_controller_ loadViewIfNeeded];

  id mock_visibility_delegate =
      OCMProtocolMock(@protocol(ContextualPanelEntrypointVisibilityDelegate));
  view_controller_.visibilityDelegate = mock_visibility_delegate;

  LocationBarBadgeConfiguration* config = [[LocationBarBadgeConfiguration alloc]
       initWithBadgeType:LocationBarBadgeType::kReaderMode
      accessibilityLabel:@"Reader Mode"
              badgeImage:[[UIImage alloc] init]];
  [view_controller_ setBadgeConfig:config];

  __block BOOL view_hidden_when_show_delegate_called = NO;
  OCMExpect([mock_visibility_delegate setContextualPanelEntrypointHidden:NO])
      .andDo(^(NSInvocation*) {
        view_hidden_when_show_delegate_called = view_controller_.view.hidden;
      });

  [view_controller_ showBadge];
  EXPECT_TRUE(view_hidden_when_show_delegate_called);
  EXPECT_FALSE(view_controller_.view.hidden);

  __block BOOL view_hidden_when_hide_delegate_called = YES;
  OCMExpect([mock_visibility_delegate setContextualPanelEntrypointHidden:YES])
      .andDo(^(NSInvocation*) {
        view_hidden_when_hide_delegate_called = view_controller_.view.hidden;
      });

  [view_controller_ hideBadge];
  EXPECT_FALSE(view_hidden_when_hide_delegate_called);
  EXPECT_TRUE(view_controller_.view.hidden);
  EXPECT_OCMOCK_VERIFY(mock_visibility_delegate);
}

// Tests that the mutator is called when the badge is tapped.
TEST_F(LocationBarBadgeViewControllerTest, BadgeTapped) {
  [view_controller_ loadViewIfNeeded];
  LocationBarBadgeConfiguration* config = [[LocationBarBadgeConfiguration alloc]
       initWithBadgeType:LocationBarBadgeType::kOverflow
      accessibilityLabel:@"Overflow"
              badgeImage:[[UIImage alloc] init]];
  [view_controller_ setBadgeConfig:config];
  OCMExpect([mock_mutator_ badgeTapped:config]);
  [view_controller_ userTappedBadge];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that the mutator is called when the label is centered.
TEST_F(LocationBarBadgeViewControllerTest,
       SetLocationBarLabelCenteredBetweenContent) {
  [view_controller_ loadViewIfNeeded];
  OCMExpect([mock_mutator_ setLocationBarLabelCenteredBetweenContent:YES]);
  [view_controller_ setLocationBarLabelCenteredBetweenContent:YES];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that the mutator is called when the IPH is dismissed.
TEST_F(LocationBarBadgeViewControllerTest, DismissIPHAnimated) {
  [view_controller_ loadViewIfNeeded];
  OCMExpect([mock_mutator_ dismissIPHAnimated:YES]);
  [mock_mutator_ dismissIPHAnimated:YES];
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Fake visibility delegate used because OCMock cannot box C++ `std::optional`
// parameters passed by value.
@interface LocationBarBadgeViewControllerFakeVisibilityDelegate
    : NSObject <ContextualPanelEntrypointVisibilityDelegate>
@property(nonatomic, assign) std::optional<ContextualPanelItemType> itemType;
@property(nonatomic, assign) std::optional<ContextualPanelItemType>
    itemTypeWhenHidden;
@property(nonatomic, assign) BOOL didHideEntrypoint;
@end

@implementation LocationBarBadgeViewControllerFakeVisibilityDelegate
- (void)setContextualPanelEntrypointHidden:(BOOL)hidden {
  if (hidden) {
    _didHideEntrypoint = YES;
    _itemTypeWhenHidden = _itemType;
  }
}
- (void)setContextualPanelItemType:
    (std::optional<ContextualPanelItemType>)itemType {
  _itemType = itemType;
}
- (void)setContextualPanelCurrentlyAnimating:(BOOL)animating {
  // No-op.
}
- (void)setContextualPanelEntrypointState:
    (ContextualPanelEntrypointState)state {
  // No-op.
}
@end

// Tests that `contextualPanelItemType` is cleared when hiding a badge or
// setting a non-Contextual-Panel badge configuration.
TEST_F(LocationBarBadgeViewControllerTest,
       ClearsContextualPanelItemTypeForNonContextualPanelBadges) {
  [view_controller_ loadViewIfNeeded];
  LocationBarBadgeViewControllerFakeVisibilityDelegate* visibility_delegate =
      [[LocationBarBadgeViewControllerFakeVisibilityDelegate alloc] init];
  view_controller_.visibilityDelegate = visibility_delegate;

  LocationBarBadgeConfiguration* reader_mode_config =
      [[LocationBarBadgeConfiguration alloc]
           initWithBadgeType:LocationBarBadgeType::kReaderMode
          accessibilityLabel:@"Reader Mode"
                  badgeImage:[[UIImage alloc] init]];
  [view_controller_ setBadgeConfig:reader_mode_config];
  [view_controller_
      setContextualPanelItemType:ContextualPanelItemType::ReaderModeItem];
  EXPECT_EQ(visibility_delegate.itemType,
            ContextualPanelItemType::ReaderModeItem);

  [view_controller_ hideBadge];
  EXPECT_TRUE(visibility_delegate.didHideEntrypoint);
  EXPECT_EQ(visibility_delegate.itemType, std::nullopt);
  EXPECT_EQ(visibility_delegate.itemTypeWhenHidden, std::nullopt);

  [view_controller_
      setContextualPanelItemType:ContextualPanelItemType::ReaderModeItem];
  ASSERT_EQ(visibility_delegate.itemType,
            ContextualPanelItemType::ReaderModeItem);

  LocationBarBadgeConfiguration* gemini_config =
      [[LocationBarBadgeConfiguration alloc]
           initWithBadgeType:LocationBarBadgeType::kGeminiContextualCueChip
          accessibilityLabel:@"Ask Gemini"
                  badgeImage:[[UIImage alloc] init]];
  [view_controller_ setBadgeConfig:gemini_config];
  EXPECT_EQ(visibility_delegate.itemType, std::nullopt);
}

// Tests `LocationBarBadgeItem` value equality and `setBadgeItems:` visibility,
// accessibility attributes, display states, and deduplication.
TEST_F(LocationBarBadgeViewControllerTest,
       SetBadgeItemsUpdatesVisibilityAndState) {
  base::test::ScopedFeatureList feature_list(kLocationBarBadgeMigration);
  UIImage* icon = [[UIImage alloc] init];
  LocationBarBadgeItem* expanded_item =
      CreateBadgeItem(icon, LocationBarBadgeDisplayState::kExpanded);
  LocationBarBadgeItem* duplicate_item =
      CreateBadgeItem(icon, LocationBarBadgeDisplayState::kExpanded);
  LocationBarBadgeItem* collapsed_item =
      CreateBadgeItem(icon, LocationBarBadgeDisplayState::kCollapsed);
  EXPECT_NSEQ(expanded_item, duplicate_item);
  EXPECT_EQ(expanded_item.hash, duplicate_item.hash);
  EXPECT_NSNE(expanded_item, collapsed_item);

  OCMExpect([mock_mutator_ setLocationBarLabelCenteredBetweenContent:YES]);
  [view_controller_ setBadgeItems:@[ expanded_item ]];
  EXPECT_FALSE(view_controller_.view.hidden);
  UIButton* button =
      chrome_test_util::FindViewByClass<UIButton>(view_controller_.view);
  EXPECT_NSEQ(button.accessibilityIdentifier,
              kLocationBarBadgeReaderModeIdentifier);
  EXPECT_NSEQ(button.accessibilityLabel, kReaderAccessibilityLabel);
  EXPECT_NSEQ(chrome_test_util::FindLabelWithText(view_controller_.view,
                                                  kReaderLabelText)
                  .textColor,
              [UIColor colorNamed:kTextPrimaryColor]);

  // Passing an equal array is a deduplication no-op.
  id<LocationBarBadgeMutator> strict_mutator =
      OCMStrictProtocolMock(@protocol(LocationBarBadgeMutator));
  view_controller_.mutator = strict_mutator;
  [view_controller_ setBadgeItems:@[ duplicate_item ]];
  view_controller_.mutator = mock_mutator_;

  OCMExpect([mock_mutator_ setLocationBarLabelCenteredBetweenContent:NO]);
  [view_controller_ setBadgeItems:@[ collapsed_item ]];
  EXPECT_FALSE(view_controller_.view.hidden);

  [view_controller_ setBadgeItems:@[]];
  EXPECT_TRUE(view_controller_.view.hidden);
  EXPECT_NSEQ(button.accessibilityIdentifier, nil);
  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that switching item types updates visual elements and button metadata.
TEST_F(LocationBarBadgeViewControllerTest,
       UpdatesButtonMetadataWhenSwitchingBadgeType) {
  base::test::ScopedFeatureList feature_list(kLocationBarBadgeMigration);
  UIImage* icon = [[UIImage alloc] init];
  LocationBarBadgeItem* reader_item =
      CreateBadgeItem(icon, LocationBarBadgeDisplayState::kExpanded);
  LocationBarBadgeItem* overflow_item = [[LocationBarBadgeItem alloc]
            initWithBadgeType:LocationBarBadgeType::kOverflow
                    iconImage:icon
                    labelText:kOverflowLabelText
      accessibilityIdentifier:kLocationBarBadgeOverflowIdentifier
           accessibilityLabel:kOverflowAccessibilityLabel
           hasUnreadIndicator:NO
                 displayState:LocationBarBadgeDisplayState::kCollapsed];

  [view_controller_ setBadgeItems:@[ reader_item ]];
  EXPECT_FALSE(view_controller_.view.hidden);

  [view_controller_ setBadgeItems:@[ overflow_item ]];
  EXPECT_FALSE(view_controller_.view.hidden);
  UIButton* button =
      chrome_test_util::FindViewByClass<UIButton>(view_controller_.view);
  EXPECT_NSEQ(button.accessibilityIdentifier,
              kLocationBarBadgeOverflowIdentifier);
  EXPECT_NSEQ(button.accessibilityLabel, kOverflowAccessibilityLabel);
}

// Tests that switching from an expanded item to another badge type reports the
// collapse of the outgoing badge type, not the incoming one.
TEST_F(LocationBarBadgeViewControllerTest,
       ReportsOutgoingBadgeTypeOnCollapseWhenSwitchingBadgeType) {
  base::test::ScopedFeatureList feature_list(kLocationBarBadgeMigration);
  UIImage* icon = [[UIImage alloc] init];
  LocationBarBadgeItem* reader_item =
      CreateBadgeItem(icon, LocationBarBadgeDisplayState::kExpanded);
  LocationBarBadgeItem* overflow_item = [[LocationBarBadgeItem alloc]
            initWithBadgeType:LocationBarBadgeType::kOverflow
                    iconImage:icon
                    labelText:kOverflowLabelText
      accessibilityIdentifier:kLocationBarBadgeOverflowIdentifier
           accessibilityLabel:kOverflowAccessibilityLabel
           hasUnreadIndicator:NO
                 displayState:LocationBarBadgeDisplayState::kCollapsed];
  __block int collapse_count = 0;
  __block LocationBarBadgeType collapsed_badge_type =
      LocationBarBadgeType::kNone;
  OCMStub([[mock_mutator_ ignoringNonObjectArgs]
              handleBadgeContainerCollapse:LocationBarBadgeType::kNone])
      .andDo(^(NSInvocation* invocation) {
        ++collapse_count;
        [invocation getArgument:&collapsed_badge_type atIndex:2];
      });

  [view_controller_ setBadgeItems:@[ reader_item ]];
  [view_controller_ setBadgeItems:@[ overflow_item ]];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return collapse_count > 0;
      }));
  EXPECT_EQ(collapse_count, 1);
  EXPECT_EQ(collapsed_badge_type, LocationBarBadgeType::kReaderMode);
}

// Tests that a same-type update of a visible badge keeps the unified container
// label color instead of resetting it to the primary text color.
TEST_F(LocationBarBadgeViewControllerTest,
       KeepsUnifiedContainerLabelColorOnSameTypeUpdate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kLocationBarBadgeMigration, kPageActionMenu},
                                {});
  UIImage* icon = [[UIImage alloc] init];
  LocationBarBadgeItem* expanded_item =
      CreateBadgeItem(icon, LocationBarBadgeDisplayState::kExpanded);
  LocationBarBadgeItem* collapsed_item =
      CreateBadgeItem(icon, LocationBarBadgeDisplayState::kCollapsed);
  UIColor* unified_label_color = [UIColor colorNamed:kSolidWhiteColor];

  [view_controller_ setBadgeItems:@[ expanded_item ]];
  UILabel* label = chrome_test_util::FindLabelWithText(view_controller_.view,
                                                       kReaderLabelText);
  EXPECT_NSEQ(label.textColor, unified_label_color);

  [view_controller_ setBadgeItems:@[ collapsed_item ]];
  EXPECT_NSEQ(label.textColor, unified_label_color);
}

// Tests that `setBadgeItems:` does not update the badge when badge items are
// not supported.
TEST_F(LocationBarBadgeViewControllerTest,
       DoesNotUpdateItemsWhenMigrationFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kLocationBarBadgeMigration);
  [view_controller_ loadViewIfNeeded];
  UIImage* icon = [[UIImage alloc] init];
  LocationBarBadgeItem* item =
      CreateBadgeItem(icon, LocationBarBadgeDisplayState::kExpanded);

  [view_controller_ setBadgeItems:@[ item ]];
  EXPECT_TRUE(view_controller_.view.hidden);
  UIButton* button =
      chrome_test_util::FindViewByClass<UIButton>(view_controller_.view);
  EXPECT_NSEQ(button.accessibilityIdentifier, nil);
}

// Tests that `separatorView` is configured and hidden by default.
TEST_F(LocationBarBadgeViewControllerTest,
       SeparatorViewConfiguredAndHiddenByDefault) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kLocationBarBadgeMigration);

  [view_controller_ view];
  UIView* separator_view = chrome_test_util::FindViewById(
      view_controller_.view, kLocationBarBadgeSeparatorAccessibilityIdentifier);
  ASSERT_NE(nil, separator_view);
  EXPECT_TRUE(separator_view.hidden);
}

// Tests that `setBadgeItems:` displays the separator when items are present and
// migration is enabled.
TEST_F(LocationBarBadgeViewControllerTest,
       SetBadgeItemsShowsSeparatorWhenItemsProvidedAndMigrationEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kLocationBarBadgeMigration);

  [view_controller_ view];
  LocationBarBadgeItem* badge_item = CreateBadgeItem(
      [[UIImage alloc] init], LocationBarBadgeDisplayState::kCollapsed);

  [view_controller_ setBadgeItems:@[ badge_item ]];

  UIView* separator_view = chrome_test_util::FindViewById(
      view_controller_.view, kLocationBarBadgeSeparatorAccessibilityIdentifier);
  ASSERT_NE(nil, separator_view);
  EXPECT_FALSE(view_controller_.view.hidden);
  EXPECT_FALSE(separator_view.hidden);

  // Clearing items should hide the separator along with the view.
  [view_controller_ setBadgeItems:@[]];
  EXPECT_TRUE(view_controller_.view.hidden);
  EXPECT_TRUE(separator_view.hidden);
}

// Tests that `updateForFullscreenProgress:` hides the separator at progress 0
// and restores visibility and full alpha at progress 1, even if items were
// cleared while partially collapsed.
TEST_F(LocationBarBadgeViewControllerTest,
       UpdateForFullscreenProgressUpdatesSeparatorVisibilityAndAlpha) {
  base::test::ScopedFeatureList feature_list(kLocationBarBadgeMigration);
  [view_controller_ view];
  LocationBarBadgeItem* badge_item = CreateBadgeItem(
      [[UIImage alloc] init], LocationBarBadgeDisplayState::kCollapsed);
  [view_controller_ setBadgeItems:@[ badge_item ]];

  UIView* separator_view = chrome_test_util::FindViewById(
      view_controller_.view, kLocationBarBadgeSeparatorAccessibilityIdentifier);
  ASSERT_NE(nil, separator_view);

  [view_controller_ updateForFullscreenProgress:0.0];
  EXPECT_TRUE(separator_view.hidden);

  // Clear items while collapsed, then restore fullscreen to 1.0 before
  // re-adding a badge item.
  [view_controller_ setBadgeItems:@[]];
  [view_controller_ updateForFullscreenProgress:1.0];
  [view_controller_ setBadgeItems:@[ badge_item ]];
  EXPECT_FALSE(separator_view.hidden);
  EXPECT_EQ(1.0, separator_view.alpha);
}

// Tests that `separatorView` is not created when migration is disabled.
TEST_F(LocationBarBadgeViewControllerTest,
       SeparatorViewNotCreatedWhenMigrationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kLocationBarBadgeMigration);

  [view_controller_ view];
  EXPECT_EQ(nil, chrome_test_util::FindViewById(
                     view_controller_.view,
                     kLocationBarBadgeSeparatorAccessibilityIdentifier));
}

// Tests that `setBadgeItems:` displays the badge stack view and separator when
// migration is enabled, independent of legacy `incognitoBadgeViewController`
// visibility state.
TEST_F(LocationBarBadgeViewControllerTest,
       SetBadgeItemsShowsStackViewWhenLegacyIncognitoBadgeHidden) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kLocationBarBadgeMigration);

  [view_controller_ view];
  UIStackView* badge_stack_view = chrome_test_util::FindViewById<UIStackView>(
      view_controller_.view, kLocationBarBadgeStackViewAccessibilityIdentifier);
  ASSERT_NE(nil, badge_stack_view);

  ConfigureIncognitoBadgeViewController();
  // Showing the legacy incognito badge under migration does not mark the
  // unified location bar badge visible before badge items arrive, and the
  // legacy incognito view remains hidden.
  [view_controller_ setIncognitoBadgeViewHidden:NO];
  EXPECT_TRUE(view_controller_.view.hidden);
  EXPECT_TRUE(incognito_badge_view_controller_.view.hidden);

  LocationBarBadgeItem* badge_item = CreateBadgeItem(
      [[UIImage alloc] init], LocationBarBadgeDisplayState::kCollapsed);
  [view_controller_ setBadgeItems:@[ badge_item ]];

  EXPECT_FALSE(view_controller_.view.hidden);
  EXPECT_FALSE(badge_stack_view.hidden);
  UIView* separator_view = chrome_test_util::FindViewById(
      view_controller_.view, kLocationBarBadgeSeparatorAccessibilityIdentifier);
  ASSERT_NE(nil, separator_view);
  EXPECT_FALSE(separator_view.hidden);

  // Hiding the legacy incognito badge under migration does not hide active
  // badge items.
  [view_controller_ setIncognitoBadgeViewHidden:YES];
  EXPECT_FALSE(view_controller_.view.hidden);
  EXPECT_FALSE(badge_stack_view.hidden);

  // Clearing items should hide the badge, stack view, and separator.
  [view_controller_ setBadgeItems:@[]];
  EXPECT_TRUE(view_controller_.view.hidden);
  EXPECT_TRUE(badge_stack_view.hidden);
  EXPECT_TRUE(separator_view.hidden);
}

// Tests that the legacy incognito badge view drives badge and stack view
// visibility when migration is disabled.
TEST_F(LocationBarBadgeViewControllerTest,
       LegacyIncognitoBadgeDrivesStackViewVisibilityWhenMigrationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kLocationBarBadgeMigration);

  [view_controller_ view];
  UIStackView* badge_stack_view = chrome_test_util::FindViewById<UIStackView>(
      view_controller_.view, kLocationBarBadgeStackViewAccessibilityIdentifier);
  ASSERT_NE(nil, badge_stack_view);

  ConfigureIncognitoBadgeViewController();
  EXPECT_TRUE(view_controller_.view.hidden);
  EXPECT_TRUE(badge_stack_view.hidden);

  [view_controller_ setIncognitoBadgeViewHidden:NO];
  EXPECT_FALSE(view_controller_.view.hidden);
  EXPECT_FALSE(badge_stack_view.hidden);

  [view_controller_ setIncognitoBadgeViewHidden:YES];
  EXPECT_TRUE(view_controller_.view.hidden);
  EXPECT_TRUE(badge_stack_view.hidden);
  EXPECT_EQ(nil, chrome_test_util::FindViewById(
                     view_controller_.view,
                     kLocationBarBadgeSeparatorAccessibilityIdentifier));
}
