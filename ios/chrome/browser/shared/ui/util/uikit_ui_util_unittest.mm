// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

#import <CoreGraphics/CoreGraphics.h>
#import <CoreImage/CoreImage.h>

#import <utility>
#import <vector>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

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
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = 0;
  format.opaque = NO;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:kSize format:format];

  UIImage* image =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context){
      }];

  // Verify the grey image's size and scale.
  UIImage* greyImage = GreyImage(image);
  EXPECT_EQ(kSize.width, greyImage.size.width);
  EXPECT_EQ(kSize.height, greyImage.size.height);
  EXPECT_EQ(1.0, greyImage.scale);
}

// Tests that GreyImage with kGreyImageCIFilter enabled returns a valid
// grayscale image matching the source dimensions, and verifies that pixels in
// black and white regions are rendered as expected.
TEST_F(UIKitUIUtilTest, TestGreyImageWithCIFilter) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGreyImageCIFilter);

  const CGSize kSize = CGSizeMake(100, 100);
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = 2;
  format.opaque = YES;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:kSize format:format];

  // Create an image with a white left half and a black right half.
  UIImage* image = [renderer imageWithActions:^(
                                 UIGraphicsImageRendererContext* context) {
    [[UIColor whiteColor] setFill];
    UIRectFill(CGRectMake(0, 0, kSize.width / 2, kSize.height));
    [[UIColor blackColor] setFill];
    UIRectFill(CGRectMake(kSize.width / 2, 0, kSize.width / 2, kSize.height));
  }];

  // Convert to grey image and verify its properties.
  UIImage* greyImage = GreyImage(image);
  ASSERT_TRUE(greyImage);
  EXPECT_EQ(kSize.width, greyImage.size.width);
  EXPECT_EQ(kSize.height, greyImage.size.height);
  EXPECT_EQ(image.scale, greyImage.scale);
  EXPECT_TRUE(greyImage.CIImage != nil);
  EXPECT_EQ(greyImage.CGImage, nil);

  CIContext* testContext = [CIContext contextWithOptions:nil];
  CGImageRef cgImage = [testContext createCGImage:greyImage.CIImage
                                         fromRect:greyImage.CIImage.extent];
  ASSERT_TRUE(cgImage);

  const size_t width = CGImageGetWidth(cgImage);
  const size_t height = CGImageGetHeight(cgImage);
  const size_t bytesPerPixel = 4;
  const size_t bytesPerRow = bytesPerPixel * width;

  std::vector<uint8_t> pixelData(bytesPerRow * height);
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      pixelData.data(), width, height, 8, bytesPerRow, colorSpace,
      std::to_underlying(kCGImageAlphaPremultipliedLast) |
          std::to_underlying(kCGImageByteOrder32Big));
  CGContextDrawImage(context, CGRectMake(0, 0, width, height), cgImage);
  CGColorSpaceRelease(colorSpace);
  CGContextRelease(context);
  CGImageRelease(cgImage);

  // Inspect a pixel in the white area (left half).
  const size_t whiteX = width / 4;
  const size_t whiteY = height / 2;
  const size_t whiteOffset = (whiteY * bytesPerRow) + (whiteX * bytesPerPixel);
  EXPECT_NEAR(pixelData[whiteOffset], 255, 1);
  EXPECT_EQ(pixelData[whiteOffset], pixelData[whiteOffset + 1]);
  EXPECT_EQ(pixelData[whiteOffset], pixelData[whiteOffset + 2]);
  EXPECT_EQ(pixelData[whiteOffset + 3], 255);

  // Inspect a pixel in the black area (right half).
  const size_t blackX = (3 * width) / 4;
  const size_t blackY = height / 2;
  const size_t blackOffset = (blackY * bytesPerRow) + (blackX * bytesPerPixel);
  EXPECT_NEAR(pixelData[blackOffset], 0, 1);
  EXPECT_EQ(pixelData[blackOffset], pixelData[blackOffset + 1]);
  EXPECT_EQ(pixelData[blackOffset], pixelData[blackOffset + 2]);
  EXPECT_EQ(pixelData[blackOffset + 3], 255);
}

TEST_F(UIKitUIUtilTest, ViewHierarchyRootForView) {
  UIView* view1 = [[UIView alloc] init];
  EXPECT_EQ(ViewHierarchyRootForView(view1), view1);

  UIView* view2 = [[UIView alloc] init];
  [view1 addSubview:view2];
  EXPECT_EQ(ViewHierarchyRootForView(view2), view1);

  UIWindow* window = [[UIWindow alloc]
      initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
  [window addSubview:view1];

  EXPECT_EQ(ViewHierarchyRootForView(view1), window);
  EXPECT_EQ(ViewHierarchyRootForView(view2), window);

  [view1 removeFromSuperview];
  EXPECT_EQ(ViewHierarchyRootForView(view1), view1);
  EXPECT_EQ(ViewHierarchyRootForView(view2), view1);
}

// Tests that the NSArray of UITraits passed into `TraitCollectionSetForTraits`
// is not the returned value when the `kTraitCollectionDidChangeRefactor`
// feature flag is disabled.
TEST_F(UIKitUIUtilTest, UITraitArrayIsReturnedWhenKillswitchIsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEnableTraitCollectionRegistration);

  NSArray<UITrait>* traits = @[ UITraitForceTouchCapability.class ];
  EXPECT_NE([TraitCollectionSetForTraits(traits) count], [traits count]);
}

// Tests that the NSArray of UITraits passed into `TraitCollectionSetForTraits`
// is the returned value when the `kTraitCollectionDidChangeRefactor` feature
// flag is enabled.
TEST_F(UIKitUIUtilTest, UITraitArrayIsReturnedWhenKillswitchIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableTraitCollectionRegistration);

  NSArray<UITrait>* traits = @[ UITraitForceTouchCapability.class ];
  EXPECT_EQ([TraitCollectionSetForTraits(traits) count], [traits count]);
}

// Tests that the memory footprint of an image is calculated correctly.
TEST_F(UIKitUIUtilTest, MemoryFootprintForImage) {
  const CGSize kSize = CGSizeMake(100, 100);
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = 0;
  format.opaque = NO;
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:kSize format:format];
  UIImage* image =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context){
      }];
  size_t image_size_in_kb = CGImageGetBytesPerRow([image CGImage]) *
                            CGImageGetHeight([image CGImage]) / 1024;
  EXPECT_EQ(MemoryFootprintForImage(image), image_size_in_kb);
}

// Tests that CanShowTabStrip returns true only for Regular x Regular size
// class.
TEST_F(UIKitUIUtilTest, TestCanShowTabStrip) {
  UITraitCollection* regularXRegular = [UITraitCollection
      traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
        mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassRegular;
        mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassRegular;
      }];
  EXPECT_TRUE(CanShowTabStrip(regularXRegular));

  UITraitCollection* compactXRegular = [UITraitCollection
      traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
        mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
        mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassRegular;
      }];
  EXPECT_FALSE(CanShowTabStrip(compactXRegular));

  UITraitCollection* regularXCompact = [UITraitCollection
      traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
        mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassRegular;
        mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassCompact;
      }];
  EXPECT_FALSE(CanShowTabStrip(regularXCompact));

  UITraitCollection* compactXCompact = [UITraitCollection
      traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
        mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
        mutableTraits.verticalSizeClass = UIUserInterfaceSizeClassCompact;
      }];
  EXPECT_FALSE(CanShowTabStrip(compactXCompact));
}

}  // namespace
