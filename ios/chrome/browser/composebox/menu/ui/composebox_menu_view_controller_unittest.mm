// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/test/scoped_feature_list.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item_type.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_option.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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

}  // namespace
