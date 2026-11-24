// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/ui/intelligence_header_view.h"

#import <QuartzCore/QuartzCore.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/ui/gradient_activity_indicator_view.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_configuration.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Test double conforming to `IntelligenceHeaderViewDelegate` that records
// triggered item identifiers.
@interface FakeIntelligenceHeaderViewDelegate
    : NSObject <IntelligenceHeaderViewDelegate>
// Identifier most recently received via
// `-headerView:didTriggerItemWithIdentifier:`.
@property(nonatomic, copy) NSString* triggeredIdentifier;
@end

@implementation FakeIntelligenceHeaderViewDelegate

- (void)headerView:(IntelligenceHeaderView*)headerView
    didTriggerItemWithIdentifier:(NSString*)identifier {
  _triggeredIdentifier = [identifier copy];
}

@end

namespace {

using chrome_test_util::FindViewByClass;
using chrome_test_util::FindViewById;

constexpr CGFloat kIconPointSize = 14.0;
constexpr CGFloat kTestHeaderWidth = 360.0;
constexpr CGFloat kTestWindowDimension = 300.0;

NSString* const kTestTitle = @"Assistant";
NSString* const kTestSubtitle = @"Sharing current page";
NSString* const kCloseTitle = @"Close";
NSString* const kCloseButtonId = @"IntelligenceHeaderCloseButtonId";
NSString* const kMenuTitle = @"More options";
NSString* const kMenuButtonId = @"IntelligenceHeaderMenuButtonId";
NSString* const kHistoryTitle = @"History";
NSString* const kHistoryButtonId = @"IntelligenceHeaderHistoryButtonId";
NSString* const kResumeTitle = @"Resume";
NSString* const kCallToActionTitle = @"View";
NSString* const kUpdatedCallToActionTitle = @"Review";
NSString* const kCallToActionButtonId =
    @"IntelligenceHeaderCallToActionButtonId";

// Creates a no-op `UIAction`.
UIAction* CreateNoOpAction() {
  return [UIAction actionWithHandler:^(UIAction*){
  }];
}

// Creates an icon `IntelligenceHeaderItem` using `accessibility_id` as its
// identifier.
IntelligenceHeaderItem* CreateIconItem(Symbol symbol,
                                       NSString* title,
                                       NSString* accessibility_id) {
  return [IntelligenceHeaderItem
           itemWithIdentifier:accessibility_id
                         icon:SymbolWithPointSize(symbol, kIconPointSize)
                        title:title
      accessibilityIdentifier:accessibility_id];
}

class IntelligenceHeaderViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    header_view_ = [[IntelligenceHeaderView alloc] initWithFrame:CGRectZero];
    delegate_ = [[FakeIntelligenceHeaderViewDelegate alloc] init];
    header_view_.delegate = delegate_;
  }

  IntelligenceHeaderView* header_view_;
  FakeIntelligenceHeaderViewDelegate* delegate_;
};

// Tests default property values upon initialization.
TEST_F(IntelligenceHeaderViewTest, DefaultState) {
  EXPECT_EQ(header_view_.leadingIcon, nil);
  EXPECT_EQ(header_view_.title, nil);
  EXPECT_EQ(header_view_.subtitle, nil);
  EXPECT_EQ(header_view_.activity, IntelligenceHeaderActivity::kIdle);
  EXPECT_EQ(header_view_.density, IntelligenceHeaderDensity::kRegular);
  EXPECT_EQ(header_view_.primaryItem, nil);
  EXPECT_EQ(header_view_.secondaryItems, nil);

  UIImageView* icon_view = FindViewByClass<UIImageView>(header_view_);
  ASSERT_NE(icon_view, nil);
  EXPECT_NE(icon_view.image, nil);
}

// Tests that `leadingIcon` overrides the default logo and setting `nil`
// restores it.
TEST_F(IntelligenceHeaderViewTest,
       CustomLeadingIconOverridesAndFallsBackToDefault) {
  UIImageView* icon_view = FindViewByClass<UIImageView>(header_view_);
  ASSERT_NE(icon_view, nil);
  ASSERT_NE(icon_view.image, nil);

  UIImage* custom_icon = SymbolWithPointSize(SymbolPlus, kIconPointSize);
  header_view_.leadingIcon = custom_icon;
  EXPECT_NSEQ(header_view_.leadingIcon, custom_icon);
  EXPECT_NSEQ(icon_view.image, custom_icon);

  header_view_.leadingIcon = nil;
  EXPECT_EQ(header_view_.leadingIcon, nil);
  EXPECT_NE(icon_view.image, nil);
  EXPECT_NSNE(icon_view.image, custom_icon);
}

// Tests that `activity` toggles the `GradientActivityIndicatorView` visibility.
TEST_F(IntelligenceHeaderViewTest, ActivityTogglesActivityIndicator) {
  GradientActivityIndicatorView* indicator =
      FindViewByClass<GradientActivityIndicatorView>(header_view_);
  ASSERT_NE(indicator, nil);
  EXPECT_TRUE(indicator.hidden);

  header_view_.activity = IntelligenceHeaderActivity::kActive;
  EXPECT_EQ(header_view_.activity, IntelligenceHeaderActivity::kActive);
  EXPECT_FALSE(indicator.hidden);

  header_view_.activity = IntelligenceHeaderActivity::kIdle;
  EXPECT_EQ(header_view_.activity, IntelligenceHeaderActivity::kIdle);
  EXPECT_TRUE(indicator.hidden);
}

// Tests that `GradientActivityIndicatorView` keeps its Core Animation layers
// animating when detached from and reattached to a `UIWindow`.
TEST_F(IntelligenceHeaderViewTest,
       ActivityIndicatorAnimationsPersistAcrossWindowDetach) {
  UIWindow* window =
      [[UIWindow alloc] initWithFrame:CGRectMake(0.0, 0.0, kTestWindowDimension,
                                                 kTestWindowDimension)];
  [window addSubview:header_view_];
  header_view_.activity = IntelligenceHeaderActivity::kActive;

  GradientActivityIndicatorView* indicator =
      FindViewByClass<GradientActivityIndicatorView>(header_view_);
  ASSERT_NE(indicator, nil);
  CALayer* gradient_layer = indicator.layer.sublayers.firstObject;
  ASSERT_NE(gradient_layer, nil);
  ASSERT_NE(gradient_layer.mask, nil);
  EXPECT_GT(gradient_layer.animationKeys.count, 0u);
  EXPECT_GT(gradient_layer.mask.animationKeys.count, 0u);

  // Detach and reattach `header_view_` while `activity` remains `kActive`.
  [header_view_ removeFromSuperview];
  [window addSubview:header_view_];

  EXPECT_GT(gradient_layer.animationKeys.count, 0u);
  EXPECT_GT(gradient_layer.mask.animationKeys.count, 0u);
}

// Tests that `reset` clears content and accessories while preserving `density`.
TEST_F(IntelligenceHeaderViewTest, ResetClearsStateAndPreservesDensity) {
  UIImage* custom_icon = SymbolWithPointSize(SymbolPlus, kIconPointSize);
  header_view_.leadingIcon = custom_icon;
  header_view_.title = kTestTitle;
  header_view_.subtitle = kTestSubtitle;
  header_view_.activity = IntelligenceHeaderActivity::kActive;
  header_view_.density = IntelligenceHeaderDensity::kCondensed;
  header_view_.primaryItem =
      CreateIconItem(SymbolXMark, kCloseTitle, kCloseButtonId);
  header_view_.secondaryItems =
      @[ CreateIconItem(SymbolEllipsis, kMenuTitle, kMenuButtonId) ];

  ASSERT_NE(FindViewById(header_view_, kCloseButtonId), nil);
  ASSERT_NE(FindViewById(header_view_, kMenuButtonId), nil);

  [header_view_ reset];

  EXPECT_EQ(header_view_.leadingIcon, nil);
  EXPECT_EQ(header_view_.title, nil);
  EXPECT_EQ(header_view_.subtitle, nil);
  EXPECT_EQ(header_view_.activity, IntelligenceHeaderActivity::kIdle);
  EXPECT_EQ(header_view_.density, IntelligenceHeaderDensity::kCondensed);
  EXPECT_EQ(header_view_.primaryItem, nil);
  EXPECT_EQ(header_view_.secondaryItems, nil);
  EXPECT_EQ(FindViewById(header_view_, kCloseButtonId), nil);
  EXPECT_EQ(FindViewById(header_view_, kMenuButtonId), nil);
  UIImageView* icon_view = FindViewByClass<UIImageView>(header_view_);
  ASSERT_NE(icon_view, nil);
  EXPECT_NE(icon_view.image, nil);
  EXPECT_NSNE(icon_view.image, custom_icon);
}

// Tests that tapping an identifier-backed `primaryItem` notifies `delegate` and
// that a menu-backed item configures `showsMenuAsPrimaryAction`.
TEST_F(IntelligenceHeaderViewTest, PrimaryAndMenuItemsConfigureButtons) {
  header_view_.primaryItem =
      CreateIconItem(SymbolXMark, kCloseTitle, kCloseButtonId);

  UIMenu* menu = [UIMenu menuWithChildren:@[ CreateNoOpAction() ]];
  header_view_.secondaryItems = @[ [IntelligenceHeaderItem
           itemWithIdentifier:kMenuButtonId
                         icon:SymbolWithPointSize(SymbolEllipsis,
                                                  kIconPointSize)
                        title:kMenuTitle
      accessibilityIdentifier:kMenuButtonId
                         menu:menu] ];

  UIButton* close_button = FindViewById<UIButton>(header_view_, kCloseButtonId);
  ASSERT_NE(close_button, nil);
  EXPECT_NSEQ(close_button.accessibilityLabel, kCloseTitle);
  [close_button
      sendActionsForControlEvents:UIControlEventPrimaryActionTriggered];
  EXPECT_NSEQ(delegate_.triggeredIdentifier, kCloseButtonId);

  UIButton* menu_button = FindViewById<UIButton>(header_view_, kMenuButtonId);
  ASSERT_NE(menu_button, nil);
  EXPECT_TRUE(menu_button.showsMenuAsPrimaryAction);
  EXPECT_NSEQ(menu_button.menu, menu);
}

// Tests that two icon secondary items are grouped in a shared capsule stack
// view, whereas mixing an icon item and a call-to-action item keeps them
// ungrouped.
TEST_F(IntelligenceHeaderViewTest, SecondaryItemsGrouping) {
  IntelligenceHeaderItem* history_item =
      CreateIconItem(SymbolClock, kHistoryTitle, kHistoryButtonId);
  IntelligenceHeaderItem* menu_item =
      CreateIconItem(SymbolEllipsis, kMenuTitle, kMenuButtonId);
  header_view_.secondaryItems = @[ history_item, menu_item ];

  UIButton* history_button =
      FindViewById<UIButton>(header_view_, kHistoryButtonId);
  UIButton* menu_button = FindViewById<UIButton>(header_view_, kMenuButtonId);
  ASSERT_NE(history_button, nil);
  ASSERT_NE(menu_button, nil);
  // Both buttons share the grouped capsule `UIStackView` as their superview.
  EXPECT_EQ(history_button.superview, menu_button.superview);

  IntelligenceHeaderItem* cta_item =
      [IntelligenceHeaderItem callToActionWithIdentifier:kCallToActionButtonId
                                                   title:kCallToActionTitle
                                 accessibilityIdentifier:kCallToActionButtonId];
  header_view_.secondaryItems = @[ cta_item, menu_item ];

  UIButton* cta_button =
      FindViewById<UIButton>(header_view_, kCallToActionButtonId);
  menu_button = FindViewById<UIButton>(header_view_, kMenuButtonId);
  ASSERT_NE(cta_button, nil);
  ASSERT_NE(menu_button, nil);
  // Standalone buttons are added directly to `_accessoryStackView` without a
  // nested capsule wrapper.
  EXPECT_EQ(cta_button.superview, menu_button.superview);
  EXPECT_EQ(cta_button.superview.layer.cornerRadius, 0.0);
}

// Tests that updating an item's title or icon with the same identifier updates
// the existing `UIButton` in place without recreating it.
TEST_F(IntelligenceHeaderViewTest, SameIdentifierUpdatesButtonContentInPlace) {
  header_view_.primaryItem =
      [IntelligenceHeaderItem callToActionWithIdentifier:kCallToActionButtonId
                                                   title:kCallToActionTitle
                                 accessibilityIdentifier:kCallToActionButtonId];
  header_view_.secondaryItems =
      @[ CreateIconItem(SymbolClock, kHistoryTitle, kHistoryButtonId) ];

  UIButton* cta_button =
      FindViewById<UIButton>(header_view_, kCallToActionButtonId);
  UIButton* icon_button =
      FindViewById<UIButton>(header_view_, kHistoryButtonId);
  ASSERT_NE(cta_button, nil);
  ASSERT_NE(icon_button, nil);

  UIImage* updated_icon = SymbolWithPointSize(SymbolPlus, kIconPointSize);
  header_view_.primaryItem = [IntelligenceHeaderItem
      callToActionWithIdentifier:kCallToActionButtonId
                           title:kUpdatedCallToActionTitle
         accessibilityIdentifier:kCallToActionButtonId];
  header_view_.secondaryItems =
      @[ [IntelligenceHeaderItem itemWithIdentifier:kHistoryButtonId
                                               icon:updated_icon
                                              title:kResumeTitle
                            accessibilityIdentifier:kHistoryButtonId] ];

  EXPECT_EQ(FindViewById<UIButton>(header_view_, kCallToActionButtonId),
            cta_button);
  EXPECT_NSEQ(cta_button.configuration.title, kUpdatedCallToActionTitle);
  EXPECT_NSEQ(cta_button.accessibilityLabel, kUpdatedCallToActionTitle);

  EXPECT_EQ(FindViewById<UIButton>(header_view_, kHistoryButtonId),
            icon_button);
  EXPECT_NSEQ(icon_button.configuration.image, updated_icon);
  EXPECT_NSEQ(icon_button.accessibilityLabel, kResumeTitle);
}

// Tests that matching secondary items (including a refreshed `UIMenu`) update
// their buttons in place, while changing secondary item count/grouping rebuilds
// only the secondary buttons and leaves `primaryItem`'s button untouched.
TEST_F(IntelligenceHeaderViewTest,
       SecondaryMenuUpdatesInPlaceAndRebuildsPreservePrimaryButton) {
  header_view_.primaryItem =
      CreateIconItem(SymbolXMark, kCloseTitle, kCloseButtonId);
  UIMenu* menu_v1 = [UIMenu menuWithChildren:@[ CreateNoOpAction() ]];
  IntelligenceHeaderItem* history_item =
      CreateIconItem(SymbolClock, kHistoryTitle, kHistoryButtonId);
  IntelligenceHeaderItem* menu_item_v1 = [IntelligenceHeaderItem
           itemWithIdentifier:kMenuButtonId
                         icon:SymbolWithPointSize(SymbolEllipsis,
                                                  kIconPointSize)
                        title:kMenuTitle
      accessibilityIdentifier:kMenuButtonId
                         menu:menu_v1];
  header_view_.secondaryItems = @[ history_item, menu_item_v1 ];

  UIButton* close_button = FindViewById<UIButton>(header_view_, kCloseButtonId);
  UIButton* history_button =
      FindViewById<UIButton>(header_view_, kHistoryButtonId);
  UIButton* menu_button = FindViewById<UIButton>(header_view_, kMenuButtonId);
  ASSERT_NE(close_button, nil);
  ASSERT_NE(history_button, nil);
  ASSERT_NE(menu_button, nil);
  UIView* grouped_capsule = menu_button.superview;
  ASSERT_GT(grouped_capsule.layer.cornerRadius, 0.0);

  // Updating content and passing a fresh `UIMenu` with the same count, styles,
  // and identifiers updates both grouped buttons in place.
  UIMenu* menu_v2 =
      [UIMenu menuWithChildren:@[ CreateNoOpAction(), CreateNoOpAction() ]];
  IntelligenceHeaderItem* updated_history_item = [IntelligenceHeaderItem
           itemWithIdentifier:kHistoryButtonId
                         icon:SymbolWithPointSize(SymbolPlus, kIconPointSize)
                        title:kResumeTitle
      accessibilityIdentifier:kHistoryButtonId];
  IntelligenceHeaderItem* menu_item_v2 = [IntelligenceHeaderItem
           itemWithIdentifier:kMenuButtonId
                         icon:SymbolWithPointSize(SymbolEllipsis,
                                                  kIconPointSize)
                        title:kMenuTitle
      accessibilityIdentifier:kMenuButtonId
                         menu:menu_v2];
  header_view_.secondaryItems = @[ updated_history_item, menu_item_v2 ];

  EXPECT_EQ(FindViewById<UIButton>(header_view_, kHistoryButtonId),
            history_button);
  EXPECT_EQ(FindViewById<UIButton>(header_view_, kMenuButtonId), menu_button);
  EXPECT_EQ(FindViewById<UIButton>(header_view_, kCloseButtonId), close_button);
  EXPECT_NSEQ(history_button.accessibilityLabel, kResumeTitle);
  EXPECT_NSEQ(menu_button.menu, menu_v2);
  NSArray<UIView*>* expected_order = @[ history_button, menu_button ];
  EXPECT_NSEQ(base::apple::ObjCCastStrict<UIStackView>(grouped_capsule)
                  .arrangedSubviews,
              expected_order);

  // Switching from grouped (2 icons) to standalone (1 CTA) rebuilds only the
  // secondary section while keeping `close_button` untouched.
  header_view_.secondaryItems = @[ [IntelligenceHeaderItem
      callToActionWithIdentifier:kCallToActionButtonId
                           title:kCallToActionTitle
         accessibilityIdentifier:kCallToActionButtonId] ];
  UIButton* cta_button =
      FindViewById<UIButton>(header_view_, kCallToActionButtonId);
  ASSERT_NE(cta_button, nil);
  EXPECT_EQ(cta_button.superview.layer.cornerRadius, 0.0);
  EXPECT_EQ(FindViewById<UIButton>(header_view_, kCloseButtonId), close_button);
}

// Tests that condensed density produces a smaller fitting height than regular
// density for the same content.
TEST_F(IntelligenceHeaderViewTest, CondensedDensityReducesFittingHeight) {
  header_view_.title = kTestTitle;
  header_view_.subtitle = kTestSubtitle;

  CGSize target_size =
      CGSizeMake(kTestHeaderWidth, UILayoutFittingCompressedSize.height);
  CGFloat regular_height =
      [header_view_
            systemLayoutSizeFittingSize:target_size
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;

  header_view_.density = IntelligenceHeaderDensity::kCondensed;
  CGFloat condensed_height =
      [header_view_
            systemLayoutSizeFittingSize:target_size
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;

  EXPECT_LT(condensed_height, regular_height);
}

// Tests that `applyConfiguration:` updates all header fields atomically, reuses
// matching accessory buttons in place, and removes accessories when set to
// `nil`.
TEST_F(IntelligenceHeaderViewTest,
       ApplyConfigurationUpdatesFieldsAndReusesButtons) {
  UIImage* custom_icon = SymbolWithPointSize(SymbolPlus, kIconPointSize);
  IntelligenceHeaderItem* primary_v1 =
      [IntelligenceHeaderItem callToActionWithIdentifier:kCallToActionButtonId
                                                   title:kCallToActionTitle
                                 accessibilityIdentifier:kCallToActionButtonId];
  IntelligenceHeaderItem* secondary_v1 =
      CreateIconItem(SymbolClock, kHistoryTitle, kHistoryButtonId);

  IntelligenceHeaderConfiguration* config_v1 =
      [[IntelligenceHeaderConfiguration alloc]
          initWithLeadingIcon:custom_icon
                        title:kTestTitle
                     subtitle:kTestSubtitle
                     activity:IntelligenceHeaderActivity::kActive
                      density:IntelligenceHeaderDensity::kCondensed
                  primaryItem:primary_v1
               secondaryItems:@[ secondary_v1 ]];
  [header_view_ applyConfiguration:config_v1];

  EXPECT_NSEQ(header_view_.leadingIcon, custom_icon);
  EXPECT_NSEQ(header_view_.title, kTestTitle);
  EXPECT_NSEQ(header_view_.subtitle, kTestSubtitle);
  EXPECT_EQ(header_view_.activity, IntelligenceHeaderActivity::kActive);
  EXPECT_EQ(header_view_.density, IntelligenceHeaderDensity::kCondensed);
  EXPECT_EQ(header_view_.primaryItem, primary_v1);
  EXPECT_NSEQ(header_view_.secondaryItems, @[ secondary_v1 ]);

  GradientActivityIndicatorView* indicator =
      FindViewByClass<GradientActivityIndicatorView>(header_view_);
  ASSERT_NE(indicator, nil);
  EXPECT_FALSE(indicator.hidden);

  UIButton* primary_button =
      FindViewById<UIButton>(header_view_, kCallToActionButtonId);
  UIButton* secondary_button =
      FindViewById<UIButton>(header_view_, kHistoryButtonId);
  ASSERT_NE(primary_button, nil);
  ASSERT_NE(secondary_button, nil);

  IntelligenceHeaderItem* primary_v2 = [IntelligenceHeaderItem
      callToActionWithIdentifier:kCallToActionButtonId
                           title:kUpdatedCallToActionTitle
         accessibilityIdentifier:kCallToActionButtonId];
  IntelligenceHeaderItem* secondary_v2 = [IntelligenceHeaderItem
           itemWithIdentifier:kHistoryButtonId
                         icon:SymbolWithPointSize(SymbolPlus, kIconPointSize)
                        title:kResumeTitle
      accessibilityIdentifier:kHistoryButtonId];

  IntelligenceHeaderConfiguration* config_v2 =
      [[IntelligenceHeaderConfiguration alloc]
          initWithLeadingIcon:nil
                        title:kResumeTitle
                     subtitle:nil
                     activity:IntelligenceHeaderActivity::kIdle
                      density:IntelligenceHeaderDensity::kRegular
                  primaryItem:primary_v2
               secondaryItems:@[ secondary_v2 ]];
  [header_view_ applyConfiguration:config_v2];

  EXPECT_EQ(header_view_.leadingIcon, nil);
  EXPECT_NSEQ(header_view_.title, kResumeTitle);
  EXPECT_EQ(header_view_.subtitle, nil);
  EXPECT_EQ(header_view_.activity, IntelligenceHeaderActivity::kIdle);
  EXPECT_EQ(header_view_.density, IntelligenceHeaderDensity::kRegular);
  EXPECT_TRUE(indicator.hidden);
  EXPECT_EQ(FindViewById<UIButton>(header_view_, kCallToActionButtonId),
            primary_button);
  EXPECT_NSEQ(primary_button.configuration.title, kUpdatedCallToActionTitle);
  EXPECT_EQ(FindViewById<UIButton>(header_view_, kHistoryButtonId),
            secondary_button);
  EXPECT_NSEQ(secondary_button.accessibilityLabel, kResumeTitle);

  IntelligenceHeaderConfiguration* config_v3 =
      [[IntelligenceHeaderConfiguration alloc]
          initWithLeadingIcon:nil
                        title:nil
                     subtitle:nil
                     activity:IntelligenceHeaderActivity::kIdle
                      density:IntelligenceHeaderDensity::kRegular
                  primaryItem:nil
               secondaryItems:nil];
  [header_view_ applyConfiguration:config_v3];

  EXPECT_EQ(header_view_.primaryItem, nil);
  EXPECT_EQ(header_view_.secondaryItems, nil);
  EXPECT_EQ(FindViewById<UIButton>(header_view_, kCallToActionButtonId), nil);
  EXPECT_EQ(FindViewById<UIButton>(header_view_, kHistoryButtonId), nil);
}

}  // namespace
