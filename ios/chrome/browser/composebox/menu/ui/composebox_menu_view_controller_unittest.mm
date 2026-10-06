// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/unguessable_token.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_shared_tab.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_cell.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item_type.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_list_cell.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_option.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_favicons_accordion_view.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

@interface ComposeboxMenuViewController (Testing)
@property(nonatomic, readonly) UICollectionView* collectionView;
@end

namespace {

// iPhone 16 screen width in points.
const CGFloat kIPhoneScreenWidth = 393.0f;
// iPhone SE screen width in points.
const CGFloat kIPhoneSEScreenWidth = 375.0f;
// Standard test view height.
const CGFloat kTestViewHeight = 600.0f;

using ComposeboxMenuViewControllerTest = PlatformTest;

// Helper to create a dummy UIImage for testing.
UIImage* CreateTestImage() {
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(10, 10)];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    UIRectFill(CGRectMake(0, 0, 10, 10));
  }];
}

// Returns whether `view` has a `UILargeContentViewerInteraction`.
BOOL HasLargeContentViewerInteraction(UIView* view) {
  for (id<UIInteraction> interaction in view.interactions) {
    if ([interaction isKindOfClass:[UILargeContentViewerInteraction class]]) {
      return YES;
    }
  }
  return NO;
}

// Tests that the Shared Tabs cell is configured without a leading image, with
// the comma-joined domain subtitle, and with a trailing favicons accordion view
// alongside the disclosure indicator.
TEST_F(ComposeboxMenuViewControllerTest, TestSharedTabsCellConfiguration) {
  ComposeboxMenuViewController* viewController =
      [[ComposeboxMenuViewController alloc] init];
  viewController.view.frame =
      CGRectMake(0, 0, kIPhoneScreenWidth, kTestViewHeight);

  ComposeboxUIInputState* inputState = [[ComposeboxUIInputState alloc] init];
  inputState.uiConfig = [ComposeboxUIConfig localFallbackUIConfig];
  inputState.allowedAttachments = {
      ComposeboxAttachmentOption::kCurrentTab,
      ComposeboxAttachmentOption::kTab,
  };
  inputState.sharedTabs = @[
    [[ComposeboxMenuSharedTab alloc]
                initWithURL:GURL("https://www.alltrails.com/parks")
                      title:@"AllTrails"
        inputItemIdentifier:base::UnguessableToken::Create()
                    favicon:CreateTestImage()],
    [[ComposeboxMenuSharedTab alloc]
                initWithURL:GURL("https://www.nps.gov/yose")
                      title:@"NPS"
        inputItemIdentifier:base::UnguessableToken::Create()
                    favicon:nil],
  ];

  [viewController setUIInputState:inputState];
  [viewController.view layoutIfNeeded];
  [viewController.collectionView layoutIfNeeded];

  UICollectionView* collectionView = viewController.collectionView;
  ASSERT_NE(collectionView, nil);
  ASSERT_EQ(collectionView.numberOfSections, 2);
  ASSERT_EQ([collectionView numberOfItemsInSection:1], 1);

  NSIndexPath* sharedTabsIndexPath = [NSIndexPath indexPathForItem:0
                                                         inSection:1];
  UICollectionViewCell* rawCell =
      [collectionView cellForItemAtIndexPath:sharedTabsIndexPath];
  ASSERT_TRUE([rawCell isKindOfClass:[ComposeboxMenuListCell class]]);
  ComposeboxMenuListCell* cell = static_cast<ComposeboxMenuListCell*>(rawCell);

  ASSERT_TRUE([cell.contentConfiguration
      isKindOfClass:[UIListContentConfiguration class]]);
  UIListContentConfiguration* contentConfig =
      static_cast<UIListContentConfiguration*>(cell.contentConfiguration);
  EXPECT_EQ(contentConfig.image, nil);
  EXPECT_NSEQ(contentConfig.text,
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MENU_SHARED_TABS));
  EXPECT_NSEQ(contentConfig.secondaryText, @"alltrails.com, nps.gov");

  ASSERT_EQ(cell.accessories.count, 2u);
  EXPECT_TRUE([cell.accessories[0]
      isKindOfClass:[UICellAccessoryDisclosureIndicator class]]);
  ASSERT_TRUE(
      [cell.accessories[1] isKindOfClass:[UICellAccessoryCustomView class]]);
  UICellAccessoryCustomView* customAccessory =
      static_cast<UICellAccessoryCustomView*>(cell.accessories[1]);
  EXPECT_EQ(customAccessory.placement, UICellAccessoryPlacementTrailing);
  EXPECT_EQ(customAccessory.reservedLayoutWidth, 0.0);
  ASSERT_TRUE([customAccessory.customView
      isKindOfClass:[ComposeboxFaviconsAccordionView class]]);
  ComposeboxFaviconsAccordionView* faviconsView =
      static_cast<ComposeboxFaviconsAccordionView*>(customAccessory.customView);
  EXPECT_EQ(faviconsView.arrangedSubviews.count, 2u);
}

// Tests that when 5 attachment items are present, each button is wider than or
// equal to the 60 pt minimum width, and all buttons fit across the screen
// without requiring horizontal scrolling.
TEST_F(ComposeboxMenuViewControllerTest,
       TestFiveAttachmentItemsFitWithoutScrolling) {
  ComposeboxMenuViewController* viewController =
      [[ComposeboxMenuViewController alloc] init];
  viewController.view.frame =
      CGRectMake(0, 0, kIPhoneScreenWidth, kTestViewHeight);

  ComposeboxUIInputState* inputState = [[ComposeboxUIInputState alloc] init];
  inputState.uiConfig = [ComposeboxUIConfig localFallbackUIConfig];
  inputState.allowedAttachments = {
      ComposeboxAttachmentOption::kCurrentTab,
      ComposeboxAttachmentOption::kTab,
      ComposeboxAttachmentOption::kCamera,
      ComposeboxAttachmentOption::kGallery,
      ComposeboxAttachmentOption::kFile,
  };

  [viewController setUIInputState:inputState];
  [viewController.view layoutIfNeeded];

  UICollectionView* collectionView = viewController.collectionView;
  ASSERT_NE(collectionView, nil);
  ASSERT_GE(collectionView.numberOfSections, 1);
  EXPECT_EQ([collectionView numberOfItemsInSection:0], 5);

  NSIndexPath* firstIndexPath = [NSIndexPath indexPathForItem:0 inSection:0];
  UICollectionViewLayoutAttributes* attributes =
      [collectionView layoutAttributesForItemAtIndexPath:firstIndexPath];
  ASSERT_NE(attributes, nil);

  // On 393 pt screen (available width 361 pt), (361 - 24) / 5 = 67.4 pt, which
  // pixel-aligns to ~67.33 pt on @3x screens.
  EXPECT_GE(attributes.frame.size.width, 60.0f);
  EXPECT_NEAR(attributes.frame.size.width, 67.4f, 0.5f);
}

// Tests that when 6 attachment items are present (e.g. with Drive enabled),
// each button is clamped to the 60 pt minimum width instead of shrinking to
// 55 pt, and the total content width exceeds the available container width to
// enable horizontal scrolling.
TEST_F(ComposeboxMenuViewControllerTest,
       TestSixAttachmentItemsEnforceMinimumWidth) {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  ComposeboxMenuViewController* viewController =
      [[ComposeboxMenuViewController alloc] init];
  viewController.view.frame =
      CGRectMake(0, 0, kIPhoneScreenWidth, kTestViewHeight);

  ComposeboxUIInputState* inputState = [[ComposeboxUIInputState alloc] init];
  inputState.uiConfig = [ComposeboxUIConfig localFallbackUIConfig];
  inputState.allowedAttachments = {
      ComposeboxAttachmentOption::kCurrentTab,
      ComposeboxAttachmentOption::kTab,
      ComposeboxAttachmentOption::kCamera,
      ComposeboxAttachmentOption::kGallery,
      ComposeboxAttachmentOption::kFile,
      ComposeboxAttachmentOption::kDrive,
  };

  [viewController setUIInputState:inputState];
  [viewController.view layoutIfNeeded];

  UICollectionView* collectionView = viewController.collectionView;
  ASSERT_NE(collectionView, nil);
  ASSERT_GE(collectionView.numberOfSections, 1);
  EXPECT_EQ([collectionView numberOfItemsInSection:0], 6);

  NSIndexPath* firstIndexPath = [NSIndexPath indexPathForItem:0 inSection:0];
  UICollectionViewLayoutAttributes* firstAttributes =
      [collectionView layoutAttributesForItemAtIndexPath:firstIndexPath];
  ASSERT_NE(firstAttributes, nil);

  // Natural item width would be (361 - 30) / 6 = 55 pt, but must be clamped to
  // 60 pt.
  EXPECT_EQ(firstAttributes.frame.size.width, 60.0f);

  NSIndexPath* lastIndexPath = [NSIndexPath indexPathForItem:5 inSection:0];
  UICollectionViewLayoutAttributes* lastAttributes =
      [collectionView layoutAttributesForItemAtIndexPath:lastIndexPath];
  ASSERT_NE(lastAttributes, nil);
  EXPECT_EQ(lastAttributes.frame.size.width, 60.0f);

  // Total content width for 6 items is 6 * 60 + 5 * 6 = 390 pt, which exceeds
  // the 361 pt available width (393 - 32 padding).
  CGFloat totalContentWidth = CGRectGetMaxX(lastAttributes.frame) -
                              CGRectGetMinX(firstAttributes.frame);
  EXPECT_GT(totalContentWidth, 361.0f);
}

// Tests that on smaller screens like iPhone SE (375 pt width), 6 items still
// maintain the 60 pt minimum width.
TEST_F(ComposeboxMenuViewControllerTest,
       TestSixAttachmentItemsOnSmallScreensEnforceMinimumWidth) {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  ComposeboxMenuViewController* viewController =
      [[ComposeboxMenuViewController alloc] init];
  viewController.view.frame =
      CGRectMake(0, 0, kIPhoneSEScreenWidth, kTestViewHeight);

  ComposeboxUIInputState* inputState = [[ComposeboxUIInputState alloc] init];
  inputState.uiConfig = [ComposeboxUIConfig localFallbackUIConfig];
  inputState.allowedAttachments = {
      ComposeboxAttachmentOption::kCurrentTab,
      ComposeboxAttachmentOption::kTab,
      ComposeboxAttachmentOption::kCamera,
      ComposeboxAttachmentOption::kGallery,
      ComposeboxAttachmentOption::kFile,
      ComposeboxAttachmentOption::kDrive,
  };

  [viewController setUIInputState:inputState];
  [viewController.view layoutIfNeeded];

  UICollectionView* collectionView = viewController.collectionView;
  ASSERT_NE(collectionView, nil);
  ASSERT_GE(collectionView.numberOfSections, 1);
  EXPECT_EQ([collectionView numberOfItemsInSection:0], 6);

  NSIndexPath* firstIndexPath = [NSIndexPath indexPathForItem:0 inSection:0];
  UICollectionViewLayoutAttributes* firstAttributes =
      [collectionView layoutAttributesForItemAtIndexPath:firstIndexPath];
  ASSERT_NE(firstAttributes, nil);

  // On iPhone SE (available width 343 pt), natural width would be
  // (343 - 30) / 6 = 52 pt, but must be clamped to 60 pt.
  EXPECT_EQ(firstAttributes.frame.size.width, 60.0f);
}

// Tests that Drive has the expected accessibility identifier.
TEST_F(ComposeboxMenuViewControllerTest, TestDriveAccessibilityIdentifier) {
  EXPECT_NSEQ(AccessibilityIdentifierForMenuItemType(
                  ComposeboxMenuItemType::kAttachmentDrive),
              kComposeboxAttachDriveActionAccessibilityIdentifier);
}

// Tests that each attachment cell configures `UILargeContentViewer` with its
// title and SF Symbol.
TEST_F(ComposeboxMenuViewControllerTest,
       TestAttachmentCellsLargeContentViewer) {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  ComposeboxMenuViewController* viewController =
      [[ComposeboxMenuViewController alloc] init];
  viewController.view.frame =
      CGRectMake(0, 0, kIPhoneScreenWidth, kTestViewHeight);

  ComposeboxUIInputState* inputState = [[ComposeboxUIInputState alloc] init];
  inputState.uiConfig = [ComposeboxUIConfig localFallbackUIConfig];
  inputState.currentTabFavicon = CreateTestImage();
  inputState.allowedAttachments = {
      ComposeboxAttachmentOption::kCurrentTab,
      ComposeboxAttachmentOption::kTab,
      ComposeboxAttachmentOption::kCamera,
      ComposeboxAttachmentOption::kGallery,
      ComposeboxAttachmentOption::kFile,
      ComposeboxAttachmentOption::kDrive,
  };

  [viewController setUIInputState:inputState];
  [viewController.view layoutIfNeeded];
  [viewController.collectionView layoutIfNeeded];

  UICollectionView* collectionView = viewController.collectionView;
  ASSERT_NE(collectionView, nil);
  ASSERT_GE(collectionView.numberOfSections, 1);
  ASSERT_EQ([collectionView numberOfItemsInSection:0], 6);

  for (NSInteger itemIndex = 0; itemIndex < 6; ++itemIndex) {
    SCOPED_TRACE(testing::Message() << "itemIndex=" << itemIndex);
    NSIndexPath* indexPath = [NSIndexPath indexPathForItem:itemIndex
                                                 inSection:0];
    UICollectionViewCell* rawCell =
        [collectionView cellForItemAtIndexPath:indexPath];
    ComposeboxMenuAttachmentCell* cell =
        base::apple::ObjCCastStrict<ComposeboxMenuAttachmentCell>(rawCell);
    ASSERT_NE(cell, nil);

    // Each attachment card must configure UILargeContentViewer with its title
    // and SF Symbol (never a bitmap favicon).
    EXPECT_TRUE(cell.showsLargeContentViewer);
    EXPECT_TRUE(cell.scalesLargeContentImage);
    EXPECT_TRUE(HasLargeContentViewerInteraction(cell));
    EXPECT_GT(cell.largeContentTitle.length, 0u);
    EXPECT_NSEQ(cell.largeContentTitle, cell.accessibilityLabel);
    ASSERT_NE(cell.largeContentImage, nil);
    EXPECT_TRUE(cell.largeContentImage.isSymbolImage);
    EXPECT_NE(cell.largeContentImage, inputState.currentTabFavicon);
  }
}

// Tests that preparing an attachment cell for reuse clears its Large Content
// Viewer properties.
TEST_F(ComposeboxMenuViewControllerTest,
       TestAttachmentCellPrepareForReuseClearsLargeContent) {
  ComposeboxMenuAttachmentCell* cell =
      [[ComposeboxMenuAttachmentCell alloc] initWithFrame:CGRectZero];
  ComposeboxMenuItem* item = [[ComposeboxMenuItem alloc]
      initWithTitle:@"Gallery"
              image:CreateTestImage()
               type:ComposeboxMenuItemType::kAttachmentGallery
           disabled:NO];
  [cell configureWithItem:item];
  ASSERT_NSEQ(cell.largeContentTitle, @"Gallery");
  ASSERT_NE(cell.largeContentImage, nil);

  [cell prepareForReuse];

  EXPECT_EQ(cell.largeContentTitle, nil);
  EXPECT_EQ(cell.largeContentImage, nil);
}

}  // namespace
