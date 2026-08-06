// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

const CGFloat kSmallPointSize = 10.0;
const CGFloat kLargePointSize = 20.0;
const CGFloat kZeroPointSize = 0.0;
const CGFloat kNegativePointSize = -1.0;

class GeminiUIUtilsTest : public PlatformTest {};

// Tests that a valid image is returned for a given point size.
TEST_F(GeminiUIUtilsTest, BrandedGeminiSymbolWithPointSize) {
  UIImage* image =
      [GeminiUIUtils brandedGeminiSymbolWithPointSize:kLargePointSize];
  ASSERT_NE(image, nil);
  EXPECT_GT(image.size.width, 0);
  EXPECT_GT(image.size.height, 0);
}

// Tests that a valid gradient image is created for a given point size.
TEST_F(GeminiUIUtilsTest, CreateGradientGeminiLogo) {
  UIImage* image = [GeminiUIUtils createGradientGeminiLogo:kLargePointSize];
  ASSERT_NE(image, nil);
  EXPECT_GT(image.size.width, 0);
  EXPECT_GT(image.size.height, 0);
}

// Tests that brandedGeminiSymbolWithPointSize scales the image dimensions
// correctly with point size.
TEST_F(GeminiUIUtilsTest, BrandedGeminiSymbolWithDifferentSizes) {
  UIImage* image_1 =
      [GeminiUIUtils brandedGeminiSymbolWithPointSize:kSmallPointSize];
  UIImage* image_2 =
      [GeminiUIUtils brandedGeminiSymbolWithPointSize:kLargePointSize];

  ASSERT_NE(image_1, nil);
  ASSERT_NE(image_2, nil);

  EXPECT_GT(image_2.size.width, image_1.size.width);
  EXPECT_GT(image_2.size.height, image_1.size.height);
}

// Tests that createGradientGeminiLogo scales the gradient image dimensions
// correctly with point size.
TEST_F(GeminiUIUtilsTest, CreateGradientGeminiLogoWithDifferentSizes) {
  UIImage* image_1 = [GeminiUIUtils createGradientGeminiLogo:kSmallPointSize];
  UIImage* image_2 = [GeminiUIUtils createGradientGeminiLogo:kLargePointSize];

  ASSERT_NE(image_1, nil);
  ASSERT_NE(image_2, nil);

  EXPECT_GT(image_2.size.width, image_1.size.width);
  EXPECT_GT(image_2.size.height, image_1.size.height);
}

// Note for the tests below: We check that dimensions are greater than zero
// rather than expecting specific pixel values (like 20x18) because the fallback
// size depends on the asset's intrinsic dimensions, which may change if the
// asset is updated.

// Tests that passing a zero point size to brandedGeminiSymbolWithPointSize
// returns a valid image without crashing.
TEST_F(GeminiUIUtilsTest, BrandedGeminiSymbolWithZeroSize) {
  UIImage* image =
      [GeminiUIUtils brandedGeminiSymbolWithPointSize:kZeroPointSize];
  // Verify it doesn't crash and returns a valid image (possibly empty).
  ASSERT_NE(image, nil);
  EXPECT_GT(image.size.width, 0);
  EXPECT_GT(image.size.height, 0);
}

// Tests that passing a zero point size to createGradientGeminiLogo returns a
// valid image without crashing.
TEST_F(GeminiUIUtilsTest, CreateGradientGeminiLogoWithZeroSize) {
  UIImage* image = [GeminiUIUtils createGradientGeminiLogo:kZeroPointSize];
  ASSERT_NE(image, nil);
  EXPECT_GT(image.size.width, 0);
  EXPECT_GT(image.size.height, 0);
}

// Tests that passing a negative point size to brandedGeminiSymbolWithPointSize
// returns a valid image or handles it gracefully without crashing.
TEST_F(GeminiUIUtilsTest, BrandedGeminiSymbolWithNegativeSize) {
  UIImage* image =
      [GeminiUIUtils brandedGeminiSymbolWithPointSize:kNegativePointSize];
  ASSERT_NE(image, nil);
  EXPECT_GT(image.size.width, 0);
  EXPECT_GT(image.size.height, 0);
}

// Tests that passing a negative point size to createGradientGeminiLogo
// returns a valid image or handles it gracefully without crashing.
TEST_F(GeminiUIUtilsTest, CreateGradientGeminiLogoWithNegativeSize) {
  UIImage* image = [GeminiUIUtils createGradientGeminiLogo:kNegativePointSize];
  ASSERT_NE(image, nil);
  EXPECT_GT(image.size.width, 0);
  EXPECT_GT(image.size.height, 0);
}

// Tests that contentHeightForView returns 0 when given a nil view.
TEST_F(GeminiUIUtilsTest, ContentHeightForNilViewReturnsZero) {
  EXPECT_EQ(0, [GeminiUIUtils contentHeightForView:nil withContainerWidth:100]);
}

// Tests that contentHeightForView measures accurately both unconstrained (when
// width is 0) and constrained to a specific container width.
TEST_F(GeminiUIUtilsTest, ContentHeightForViewMeasurement) {
  UILabel* label = [[UILabel alloc] init];
  label.text = @"A long multi-line label text that will definitely wrap when "
               @"constrained to a small container width.";
  label.numberOfLines = 0;

  CGFloat unconstrained_height = [GeminiUIUtils contentHeightForView:label
                                                  withContainerWidth:0];
  EXPECT_GT(unconstrained_height, 0);

  CGFloat constrained_height = [GeminiUIUtils contentHeightForView:label
                                                withContainerWidth:100];
  EXPECT_GT(constrained_height, unconstrained_height);
}

// Tests that attributedStringByReplacingWord returns an attributed string with
// the expected font, replacing the target word with a valid image attachment.
TEST_F(GeminiUIUtilsTest, AttributedStringByReplacingWord) {
  UIFont* font = [UIFont systemFontOfSize:17];
  NSString* text = @"Test Gemini Title";
  NSArray<UIColor*>* colors = @[
    [UIColor colorWithRed:1.0 green:0.0 blue:0.0 alpha:1.0],
    [UIColor colorWithRed:0.0 green:0.0 blue:1.0 alpha:1.0]
  ];
  NSAttributedString* attrString =
      [GeminiUIUtils attributedStringByReplacingWord:@"Gemini"
                                              inText:text
                                                font:font
                                              colors:colors];
  ASSERT_NE(attrString, nil);

  // Check font attribute on the first character.
  UIFont* resultFont = [attrString attribute:NSFontAttributeName
                                     atIndex:0
                              effectiveRange:nil];
  EXPECT_NSEQ(resultFont, font);

  // "Gemini" (6 chars) is replaced by an inline attachment character (1 char),
  // so the resulting string length is 12 (17 - 6 + 1).
  EXPECT_EQ(attrString.length, 12u);

  // Index 5 is where "Gemini" started after "Test ".
  NSRange attachmentRange = NSMakeRange(NSNotFound, 0);
  id attachment = [attrString attribute:NSAttachmentAttributeName
                                atIndex:5
                         effectiveRange:&attachmentRange];
  ASSERT_NE(attachment, nil);
  EXPECT_EQ(attachmentRange.location, 5u);
  EXPECT_EQ(attachmentRange.length, 1u);
  EXPECT_TRUE([attachment isKindOfClass:[NSTextAttachment class]]);
  NSTextAttachment* textAttachment =
      base::apple::ObjCCast<NSTextAttachment>(attachment);
  EXPECT_NE(textAttachment.image, nil);
  EXPECT_GT(textAttachment.image.size.width, 0);
  EXPECT_GT(textAttachment.image.size.height, 0);

  // Check the exact string contents after replacement (\uFFFC represents the
  // inline attachment character).
  NSString* expectedString =
      [NSString stringWithFormat:@"Test %C Title", (unichar)0xFFFC];
  EXPECT_NSEQ(attrString.string, expectedString);
}

// Tests that attributedStringWithGradientGeminiForTitle replaces "Gemini" on
// branded builds and returns plain attributed text on non-branded builds.
TEST_F(GeminiUIUtilsTest, AttributedStringWithGradientGeminiForTitle) {
  UIFont* font = [UIFont systemFontOfSize:17];
  NSString* title = @"Test Gemini Title";
  NSAttributedString* attrString =
      [GeminiUIUtils attributedStringWithGradientGeminiForTitle:title
                                                           font:font];
  ASSERT_NE(attrString, nil);

  // Check font attribute on the first character.
  UIFont* resultFont = [attrString attribute:NSFontAttributeName
                                     atIndex:0
                              effectiveRange:nil];
  EXPECT_NSEQ(resultFont, font);

#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  // On branded builds, "Gemini" (6 chars) is replaced by an inline attachment
  // character (1 char), so the resulting string length is 12 (17 - 6 + 1).
  EXPECT_EQ(attrString.length, 12u);

  NSRange attachmentRange = NSMakeRange(NSNotFound, 0);
  id attachment = [attrString attribute:NSAttachmentAttributeName
                                atIndex:5
                         effectiveRange:&attachmentRange];
  ASSERT_NE(attachment, nil);
  EXPECT_EQ(attachmentRange.location, 5u);
  EXPECT_EQ(attachmentRange.length, 1u);
  EXPECT_TRUE([attachment isKindOfClass:[NSTextAttachment class]]);
#else
  // On non-branded builds, the title is untouched.
  EXPECT_EQ(attrString.length, 17u);
  EXPECT_NSEQ(attrString.string, title);
#endif
}

// Tests that attributedStringByReplacingWord handles a non-matching targetWord
// gracefully without altering the string.
TEST_F(GeminiUIUtilsTest, AttributedStringByReplacingWordEdgeCases) {
  UIFont* font = [UIFont systemFontOfSize:20.0];
  NSArray<UIColor*>* colors = @[
    [UIColor colorWithRed:1.0 green:0.0 blue:0.0 alpha:1.0],
    [UIColor colorWithRed:0.0 green:0.0 blue:1.0 alpha:1.0]
  ];

  // Non-matching targetWord returns un-modified plain attributed string.
  NSAttributedString* missingWordAttr =
      [GeminiUIUtils attributedStringByReplacingWord:@"MissingWord"
                                              inText:@"Hello Gemini"
                                                font:font
                                              colors:colors];
  ASSERT_NE(missingWordAttr, nil);
  EXPECT_NSEQ(@"Hello Gemini", missingWordAttr.string);
}

// Tests that attributedStringWithGradientGeminiForTitle returns nil when given
// a nil title or font.
TEST_F(GeminiUIUtilsTest, AttributedStringWithGradientGeminiNilCheck) {
  UIFont* font = [UIFont systemFontOfSize:20.0];
  EXPECT_EQ(nil,
            [GeminiUIUtils attributedStringWithGradientGeminiForTitle:nil
                                                                 font:font]);
  EXPECT_EQ(nil,
            [GeminiUIUtils attributedStringWithGradientGeminiForTitle:@"Title"
                                                                 font:nil]);
}

}  // namespace
