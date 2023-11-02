// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using UIKitUIUtilTest = PlatformTest;

// Verify the assumption about UIViewController that on iPad and iOS 13+ all
// orientations are supported.
TEST_F(UIKitUIUtilTest, UIViewControllerSupportedOrientationsTest) {
  UIViewController* viewController =
      [[UIViewController alloc] initWithNibName:nil bundle:nil];

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    EXPECT_EQ(UIInterfaceOrientationMaskAll,
              [viewController supportedInterfaceOrientations]);
    return;
  }

  // Starting with iOS 13, the default [UIViewController
  // supportedInterfaceOrientations] returns UIInterfaceOrientationMaskAll.
  // However, this is only true if the application was built with the Xcode 11
  // SDK (in order to preserve old behavior).
  UIInterfaceOrientationMask expectedMask = UIInterfaceOrientationMaskAll;
  EXPECT_EQ(expectedMask, [viewController supportedInterfaceOrientations]);
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

TEST_F(UIKitUIUtilTest, ViewHierarchyRootForView) {
  UIView* view1 = [[UIView alloc] init];
  EXPECT_EQ(ViewHierarchyRootForView(view1), view1);

  UIView* view2 = [[UIView alloc] init];
  [view1 addSubview:view2];
  EXPECT_EQ(ViewHierarchyRootForView(view2), view1);

  UIWindow* window = [[UIWindow alloc] init];
  [window addSubview:view1];

  EXPECT_EQ(ViewHierarchyRootForView(view1), window);
  EXPECT_EQ(ViewHierarchyRootForView(view2), window);

  [view1 removeFromSuperview];
  EXPECT_EQ(ViewHierarchyRootForView(view1), view1);
  EXPECT_EQ(ViewHierarchyRootForView(view2), view1);
}

}  // namespace
