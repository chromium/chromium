// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

void ExpectInterpolatedColor(UIColor* firstColor,
                             UIColor* secondColor,
                             CGFloat percentage,
                             CGFloat expectedValue) {
  UIColor* interpolatedColor =
      InterpolateFromColorToColor(firstColor, secondColor, percentage);
  CGFloat r, g, b, a;
  [interpolatedColor getRed:&r green:&g blue:&b alpha:&a];
  EXPECT_FLOAT_EQ(expectedValue, r);
  EXPECT_FLOAT_EQ(expectedValue, g);
  EXPECT_FLOAT_EQ(expectedValue, b);
  EXPECT_FLOAT_EQ(1.0, a);
}

using UIKitUIUtilTest = PlatformTest;

// Verify the assumption about UIViewController that on iPad and iOS 13 all
// orientations are supported, and all orientations but Portrait Upside-Down on
// iOS 12 iPhone and iPod Touch.
TEST_F(UIKitUIUtilTest, UIViewControllerSupportedOrientationsTest) {
  UIViewController* viewController =
      [[UIViewController alloc] initWithNibName:nil bundle:nil];

  if (IsIPadIdiom()) {
    EXPECT_EQ(UIInterfaceOrientationMaskAll,
              [viewController supportedInterfaceOrientations]);
    return;
  }

  // Running on iPhone iOS 12 or earlier.
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    EXPECT_EQ(UIInterfaceOrientationMaskAllButUpsideDown,
              [viewController supportedInterfaceOrientations]);
    return;
  }

  // Running on iOS 13 iPhone.

  // Starting with iOS 13, the default [UIViewController
  // supportedInterfaceOrientations] returns UIInterfaceOrientationMaskAll.
  // However, this is only true if the application was built with the Xcode 11
  // SDK (in order to preserve old behavior).
  UIInterfaceOrientationMask expectedMask = UIInterfaceOrientationMaskAll;
  EXPECT_EQ(expectedMask, [viewController supportedInterfaceOrientations]);
}

TEST_F(UIKitUIUtilTest, TestGetUiFont) {
  EXPECT_TRUE(GetUIFont(FONT_HELVETICA, false, 15.0));
  EXPECT_TRUE(GetUIFont(FONT_HELVETICA_NEUE, true, 15.0));
}

// Verifies that greyImage never returns retina-scale images.
TEST_F(UIKitUIUtilTest, TestGreyImage) {
  // Create an image using the device's scale factor.
  const CGSize kSize = CGSizeMake(100, 100);
  UIGraphicsBeginImageContextWithOptions(kSize, NO, 0.0);
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  // Verify the grey image's size and scale.
  UIImage* greyImage = GreyImage(image);
  EXPECT_EQ(kSize.width, greyImage.size.width);
  EXPECT_EQ(kSize.height, greyImage.size.height);
  EXPECT_EQ(1.0, greyImage.scale);
}

// Returns an image of random color in the same scale as the device main
// screen.
UIImage* testImage(CGSize imageSize) {
  UIGraphicsBeginImageContextWithOptions(imageSize, NO, 0);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSetRGBStrokeColor(context, 0, 0, 0, 1.0);
  CGContextSetRGBFillColor(context, 0, 0, 0, 1.0);
  CGContextFillRect(context,
                    CGRectMake(0.0, 0.0, imageSize.width, imageSize.height));
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return image;
}

TEST_F(UIKitUIUtilTest, TestResizeImageOpacity) {
  UIImage* actual;
  UIImage* image = testImage(CGSizeMake(100, 100));
  actual =
      ResizeImage(image, CGSizeMake(50, 50), ProjectionMode::kAspectFit, YES);
  EXPECT_TRUE(actual);
  EXPECT_FALSE(ImageHasAlphaChannel(actual));

  actual =
      ResizeImage(image, CGSizeMake(50, 50), ProjectionMode::kAspectFit, NO);
  EXPECT_TRUE(actual);
  EXPECT_TRUE(ImageHasAlphaChannel(actual));
}

TEST_F(UIKitUIUtilTest, TestResizeImageInvalidInput) {
  UIImage* actual;
  UIImage* image = testImage(CGSizeMake(100, 50));
  actual = ResizeImage(image, CGSizeZero, ProjectionMode::kAspectFit);
  EXPECT_FALSE(actual);

  actual = ResizeImage(image, CGSizeMake(0.1, 0.1), ProjectionMode::kAspectFit);
  EXPECT_FALSE(actual);

  actual =
      ResizeImage(image, CGSizeMake(-100, -100), ProjectionMode::kAspectFit);
  EXPECT_FALSE(actual);

  actual = ResizeImage(nil, CGSizeMake(100, 100), ProjectionMode::kAspectFit);
  EXPECT_FALSE(actual);
}

TEST_F(UIKitUIUtilTest, TintImageKeepsImageProperties) {
  UIImage* image = testImage(CGSizeMake(100, 75));
  UIImage* tintedImage = TintImage(image, [UIColor blueColor]);
  EXPECT_EQ(image.size.width, tintedImage.size.width);
  EXPECT_EQ(image.size.height, tintedImage.size.height);
  EXPECT_EQ(image.scale, tintedImage.scale);
  EXPECT_EQ(image.capInsets.top, tintedImage.capInsets.top);
  EXPECT_EQ(image.capInsets.left, tintedImage.capInsets.left);
  EXPECT_EQ(image.capInsets.bottom, tintedImage.capInsets.bottom);
  EXPECT_EQ(image.capInsets.right, tintedImage.capInsets.right);
  EXPECT_EQ(image.flipsForRightToLeftLayoutDirection,
            tintedImage.flipsForRightToLeftLayoutDirection);
}

TEST_F(UIKitUIUtilTest, TestInterpolateFromColorToColor) {
  CGFloat colorOne = 50.0f / 255.0f;
  CGFloat colorTwo = 100.0f / 255.0f;
  CGFloat expectedOne = 50.0f / 255.0f;
  CGFloat expectedTwo = 55.0f / 255.0f;
  CGFloat expectedThree = 75.0f / 255.0f;
  CGFloat expectedFour = 100.0f / 255.0f;

  UIColor* firstColor =
      [UIColor colorWithRed:colorOne green:colorOne blue:colorOne alpha:1.0];
  UIColor* secondColor =
      [UIColor colorWithRed:colorTwo green:colorTwo blue:colorTwo alpha:1.0];
  ExpectInterpolatedColor(firstColor, secondColor, 0.0f, expectedOne);
  ExpectInterpolatedColor(firstColor, secondColor, 0.1f, expectedTwo);
  ExpectInterpolatedColor(firstColor, secondColor, 0.5f, expectedThree);
  ExpectInterpolatedColor(firstColor, secondColor, 1.0f, expectedFour);
}

// Tests that InterpolateFromColorToColor() works for monochrome colors.
TEST_F(UIKitUIUtilTest, TestInterpolateFromColorToColorMonochrome) {
  CGFloat kRGBComponent = 0.2;
  UIColor* rgb = [UIColor colorWithRed:kRGBComponent
                                 green:kRGBComponent
                                  blue:kRGBComponent
                                 alpha:1.0];
  ASSERT_EQ(kCGColorSpaceModelRGB,
            CGColorSpaceGetModel(CGColorGetColorSpace(rgb.CGColor)));

  UIColor* white = [UIColor whiteColor];
  ASSERT_EQ(kCGColorSpaceModelMonochrome,
            CGColorSpaceGetModel(CGColorGetColorSpace(white.CGColor)));

  UIColor* black = [UIColor blackColor];
  ASSERT_EQ(kCGColorSpaceModelMonochrome,
            CGColorSpaceGetModel(CGColorGetColorSpace(black.CGColor)));

  // Interpolate between monochrome and rgb.
  ExpectInterpolatedColor(black, rgb, 0.5, 0.1);
  // Interpolate between two monochrome colors.
  ExpectInterpolatedColor(black, white, 0.3, 0.3);
}

}  // namespace
