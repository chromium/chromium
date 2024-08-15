// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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

// Tests that the NSArray of UITraits passed into `TraitCollectionSetForTraits`
// is not the returned value when the `kTraitCollectionDidChangeRefactor`
// feature flag is disabled.
TEST_F(UIKitUIUtilTest, UITraitArrayIsReturnedWhenKillswitchIsEnabled) {
  if (@available(iOS 17, *)) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        kEnableTraitCollectionRegistration);

    NSArray<UITrait>* traits = @[ UITraitForceTouchCapability.self ];
    EXPECT_NE([TraitCollectionSetForTraits(traits) count], [traits count]);
  }
}

// Tests that the NSArray of UITraits passed into `TraitCollectionSetForTraits`
// is the returned value when the `kTraitCollectionDidChangeRefactor` feature
// flag is enabled.
TEST_F(UIKitUIUtilTest, UITraitArrayIsReturnedWhenKillswitchIsDisabled) {
  if (@available(iOS 17, *)) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        kEnableTraitCollectionRegistration);

    NSArray<UITrait>* traits = @[ UITraitForceTouchCapability.self ];
    EXPECT_EQ([TraitCollectionSetForTraits(traits) count], [traits count]);
  }
}

}  // namespace
