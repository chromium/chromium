// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_input_item_view.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/public/composebox_input_item_source.h"
#import "ios/chrome/browser/composebox/public/composebox_input_plate_position.h"
#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// The title of the test items.
NSString* const kItemTitle = @"Title";

// The size of the test bitmaps.
constexpr CGSize kBitmapSize = {10, 10};

// The upload index of the test image. Its label shows the 1-based position.
constexpr NSInteger kImageUploadIndex = 1;

// Returns a bitmap, like a favicon or an image preview.
UIImage* CreateBitmap() {
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:kBitmapSize];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    UIRectFill(CGRectMake(0, 0, kBitmapSize.width, kBitmapSize.height));
  }];
}

class ComposeboxInputItemViewTest : public PlatformTest {
 protected:
  ComposeboxInputItemViewTest()
      : view_([[ComposeboxInputItemView alloc] initWithFrame:CGRectZero]),
        theme_([[ComposeboxTheme alloc]
            initWithInputPlatePosition:ComposeboxInputPlatePosition::kBottom
                             incognito:NO
                                 isNTP:NO]) {}

  // Returns an item of `type` titled `kItemTitle`.
  ComposeboxInputItem* CreateItem(ComposeboxInputItemType type) {
    ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
        initWithComposeboxInputItemType:type
                                 source:ComposeboxInputItemSource::kUnknown];
    item.title = kItemTitle;
    return item;
  }

  ComposeboxInputItemView* view_;
  ComposeboxTheme* theme_;
};

// Test that `accessibilityLabelForImageItem:` formats the 0-based `uploadIndex`
// as a 1-based position in the label.
TEST_F(ComposeboxInputItemViewTest, TestAccessibilityLabelForImageItem) {
  ComposeboxInputItem* item =
      CreateItem(ComposeboxInputItemType::kComposeboxInputItemTypeImage);

  item.uploadIndex = 0;
  NSString* first_label =
      [ComposeboxInputItemView accessibilityLabelForImageItem:item];
  EXPECT_TRUE([first_label containsString:@"1"]);
  EXPECT_FALSE([first_label containsString:@"0"]);

  item.uploadIndex = 1;
  NSString* second_label =
      [ComposeboxInputItemView accessibilityLabelForImageItem:item];
  EXPECT_TRUE([second_label containsString:@"2"]);
  EXPECT_FALSE([second_label containsString:@"1"]);
  EXPECT_NSNE(first_label, second_label);
}

// Test that the chip shows the Large Content Viewer.
TEST_F(ComposeboxInputItemViewTest, TestShowsLargeContentViewer) {
  EXPECT_TRUE(view_.showsLargeContentViewer);
  EXPECT_TRUE(HasLargeContentViewerInteraction(view_));
  EXPECT_TRUE(view_.scalesLargeContentImage);
}

// Test that a tab chip shows its title and an SF Symbol in the Large Content
// Viewer instead of its bitmap favicon.
TEST_F(ComposeboxInputItemViewTest, TestTabWithFaviconLargeContent) {
  ComposeboxInputItem* item =
      CreateItem(ComposeboxInputItemType::kComposeboxInputItemTypeTab);
  item.leadingIconImage = CreateBitmap();

  [view_ configureWithItem:item theme:theme_];

  EXPECT_NSEQ(kItemTitle, view_.largeContentTitle);
  ASSERT_TRUE(view_.largeContentImage);
  EXPECT_TRUE(view_.largeContentImage.symbolImage);
}

// Test that a tab chip without a favicon shows its title and an SF Symbol in
// the Large Content Viewer.
TEST_F(ComposeboxInputItemViewTest, TestTabWithoutFaviconLargeContent) {
  [view_ configureWithItem:
             CreateItem(ComposeboxInputItemType::kComposeboxInputItemTypeTab)
                     theme:theme_];

  EXPECT_NSEQ(kItemTitle, view_.largeContentTitle);
  ASSERT_TRUE(view_.largeContentImage);
  EXPECT_TRUE(view_.largeContentImage.symbolImage);
}

// Test that a Drive chip shows its title and an SF Symbol in the Large Content
// Viewer instead of its bitmap file icon.
TEST_F(ComposeboxInputItemViewTest, TestDriveWithIconLargeContent) {
  ComposeboxInputItem* item =
      CreateItem(ComposeboxInputItemType::kComposeboxInputItemTypeDrive);
  item.leadingIconImage = CreateBitmap();

  [view_ configureWithItem:item theme:theme_];

  EXPECT_NSEQ(kItemTitle, view_.largeContentTitle);
  ASSERT_TRUE(view_.largeContentImage);
  EXPECT_TRUE(view_.largeContentImage.symbolImage);
}

// Test that a file chip shows its title and an SF Symbol in the Large Content
// Viewer.
TEST_F(ComposeboxInputItemViewTest, TestFileLargeContent) {
  [view_ configureWithItem:CreateItem(ComposeboxInputItemType::
                                          kComposeboxInputItemTypeRawFile)
                     theme:theme_];

  EXPECT_NSEQ(kItemTitle, view_.largeContentTitle);
  ASSERT_TRUE(view_.largeContentImage);
  EXPECT_TRUE(view_.largeContentImage.symbolImage);
}

// Test that an image chip shows its indexed accessibility label and an SF
// Symbol in the Large Content Viewer instead of its bitmap preview.
TEST_F(ComposeboxInputItemViewTest, TestImageLargeContent) {
  ComposeboxInputItem* item =
      CreateItem(ComposeboxInputItemType::kComposeboxInputItemTypeImage);
  item.previewImage = CreateBitmap();
  item.uploadIndex = kImageUploadIndex;

  [view_ configureWithItem:item theme:theme_];

  EXPECT_NSEQ([ComposeboxInputItemView accessibilityLabelForImageItem:item],
              view_.largeContentTitle);
  ASSERT_TRUE(view_.largeContentImage);
  EXPECT_TRUE(view_.largeContentImage.symbolImage);
}

// Test that preparing a chip for reuse clears its Large Content Viewer
// content.
TEST_F(ComposeboxInputItemViewTest, TestPrepareForReuseClearsLargeContent) {
  [view_ configureWithItem:CreateItem(ComposeboxInputItemType::
                                          kComposeboxInputItemTypeRawFile)
                     theme:theme_];

  [view_ prepareForReuse];

  EXPECT_FALSE(view_.largeContentTitle);
  EXPECT_FALSE(view_.largeContentImage);
}

}  // namespace
