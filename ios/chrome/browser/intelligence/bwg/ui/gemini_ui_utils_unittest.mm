// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
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

}  // namespace
