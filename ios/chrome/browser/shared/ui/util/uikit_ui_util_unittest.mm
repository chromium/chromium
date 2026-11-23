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

// Height in points used for docked keyboard frames in tests.
constexpr CGFloat kDockedKeyboardHeight = 250.0;

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

// Returns a keyboard frame-change notification from `screen` whose end frame is
// `frame`.
NSNotification* KeyboardNotificationWithFrame(UIScreen* screen, CGRect frame) {
  return
      [NSNotification notificationWithName:UIKeyboardWillChangeFrameNotification
                                    object:screen
                                  userInfo:@{
                                    UIKeyboardFrameEndUserInfoKey :
                                        [NSValue valueWithCGRect:frame],
                                  }];
}

class VisibleKeyboardHeightTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    window_ = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    screen_ = window_.windowScene.screen;
    ASSERT_TRUE(screen_);
    screen_bounds_ = screen_.coordinateSpace.bounds;
    ASSERT_FALSE(CGRectIsEmpty(screen_bounds_));
    window_.frame = screen_bounds_;
  }

  // Returns a full-width frame of `height` anchored to the bottom of
  // `screen_bounds_`.
  CGRect DockedKeyboardFrame(CGFloat height) const {
    return CGRectMake(0, CGRectGetHeight(screen_bounds_) - height,
                      CGRectGetWidth(screen_bounds_), height);
  }

  // Returns the visible keyboard height for `frame` using `screen` (which may
  // be nil or a different screen) as the notification object.
  CGFloat VisibleHeightForFrame(CGRect frame, UIScreen* screen) const {
    return VisibleKeyboardHeightFromNotification(
        KeyboardNotificationWithFrame(screen, frame), window_);
  }

  // Returns the visible keyboard height for `frame` using `screen_` as the
  // notification object.
  CGFloat VisibleHeightForFrame(CGRect frame) const {
    return VisibleHeightForFrame(frame, screen_);
  }

  UIWindow* window_;
  UIScreen* screen_;
  CGRect screen_bounds_;
};

// Tests that `VisibleKeyboardHeightFromNotification` returns the visible height
// of a docked keyboard on screen, partially offscreen during interactive
// dismissal, and 0 once dismissed offscreen.
TEST_F(VisibleKeyboardHeightTest, DockedFullScreen) {
  CGRect docked_frame = DockedKeyboardFrame(kDockedKeyboardHeight);
  EXPECT_EQ(VisibleHeightForFrame(docked_frame), kDockedKeyboardHeight);

  // Partially offscreen docked keyboard (e.g., interactive scroll dismissal).
  constexpr CGFloat kOffscreenOffset = 75.0;
  EXPECT_EQ(
      VisibleHeightForFrame(CGRectOffset(docked_frame, 0, kOffscreenOffset)),
      kDockedKeyboardHeight - kOffscreenOffset);

  // Dismissed keyboard offscreen below the window.
  CGRect offscreen_frame =
      CGRectMake(0, CGRectGetHeight(screen_bounds_),
                 CGRectGetWidth(screen_bounds_), kDockedKeyboardHeight);
  EXPECT_EQ(VisibleHeightForFrame(offscreen_frame), 0.0);
}

// Tests that `VisibleKeyboardHeightFromNotification` returns 0 for floating
// (narrower than the screen) and undocked (raised above the bottom edge)
// keyboards.
TEST_F(VisibleKeyboardHeightTest, FloatingAndUndocked) {
  constexpr CGFloat kHeight = 200.0;
  CGFloat floating_width = CGRectGetWidth(screen_bounds_) / 2.0;

  // Floating keyboard positioned above the bottom edge.
  CGRect floating_mid_screen =
      CGRectMake(40.0, CGRectGetHeight(screen_bounds_) - kHeight - 100.0,
                 floating_width, kHeight);
  EXPECT_EQ(VisibleHeightForFrame(floating_mid_screen), 0.0);

  // Floating keyboard dragged down to touch the bottom edge.
  CGRect floating_at_bottom = CGRectMake(
      40.0, CGRectGetHeight(screen_bounds_) - kHeight, floating_width, kHeight);
  EXPECT_EQ(VisibleHeightForFrame(floating_at_bottom), 0.0);

  // Full-width undocked keyboard raised above the bottom edge.
  CGRect undocked_frame = CGRectMake(0, CGRectGetHeight(screen_bounds_) - 350.0,
                                     CGRectGetWidth(screen_bounds_), kHeight);
  EXPECT_EQ(VisibleHeightForFrame(undocked_frame), 0.0);
}

// Tests that `VisibleKeyboardHeightFromNotification` handles windowed mode
// (partial overlap and window completely above the docked keyboard).
TEST_F(VisibleKeyboardHeightTest, WindowedMode) {
  CGRect docked_frame = DockedKeyboardFrame(kDockedKeyboardHeight);

  // Window raised 100pt above the bottom of the screen partially overlaps the
  // docked keyboard by `kDockedKeyboardHeight - 100.0`.
  constexpr CGFloat kBottomMargin = 100.0;
  window_.frame =
      CGRectMake(20.0, 40.0, CGRectGetWidth(screen_bounds_) - 40.0,
                 CGRectGetHeight(screen_bounds_) - 40.0 - kBottomMargin);
  EXPECT_EQ(VisibleHeightForFrame(docked_frame),
            kDockedKeyboardHeight - kBottomMargin);

  // Window completely above the docked keyboard has 0 overlap.
  window_.frame = CGRectMake(
      20.0, 20.0, CGRectGetWidth(screen_bounds_) - 40.0,
      CGRectGetHeight(screen_bounds_) - kDockedKeyboardHeight - 40.0);
  EXPECT_EQ(VisibleHeightForFrame(docked_frame), 0.0);
}

// Tests that `VisibleKeyboardHeightFromNotification` handles nil inputs,
// empty frames, detached windows, nil notification objects, and cross-screen
// notifications.
TEST_F(VisibleKeyboardHeightTest, ScreenAndNilInputs) {
  CGRect docked_frame = DockedKeyboardFrame(kDockedKeyboardHeight);

  EXPECT_EQ(VisibleKeyboardHeightFromNotification(
                KeyboardNotificationWithFrame(screen_, docked_frame), nil),
            0.0);
  EXPECT_EQ(VisibleKeyboardHeightFromNotification(nil, window_), 0.0);
  EXPECT_EQ(VisibleHeightForFrame(CGRectZero), 0.0);

  // Nil notification object falls back to `window.windowScene.screen`.
  EXPECT_EQ(VisibleHeightForFrame(docked_frame, nil), kDockedKeyboardHeight);

  // Detached window without a screen returns 0.
  UIWindow* detached_window = [[UIWindow alloc] initWithFrame:screen_bounds_];
  ASSERT_FALSE(detached_window.windowScene.screen);
  EXPECT_EQ(VisibleKeyboardHeightFromNotification(
                KeyboardNotificationWithFrame(screen_, docked_frame),
                detached_window),
            0.0);

  // Notification from a different screen returns 0.
  UIScreen* other_screen = OCMClassMock([UIScreen class]);
  EXPECT_EQ(VisibleHeightForFrame(docked_frame, other_screen), 0.0);
}

// Tests that short input-accessory-only bars return 0 on iPad and Regular x
// Regular phone layouts while docked keyboards (including windowed overlap) and
// compact-height phone bars return their overlap height.
TEST_F(VisibleKeyboardHeightTest, LargeLayoutAccessoryBarThreshold) {
  window_.traitOverrides.userInterfaceIdiom = UIUserInterfaceIdiomPad;
  [window_ updateTraitsIfNeeded];

  EXPECT_EQ(VisibleHeightForFrame(DockedKeyboardFrame(199.0)), 0.0);
  EXPECT_EQ(VisibleHeightForFrame(DockedKeyboardFrame(200.0)), 200.0);

  // Stage Manager / windowed mode: a 250pt docked iPad keyboard overlapping an
  // offset window by 120pt should still report 120pt.
  window_.frame = CGRectMake(20.0, 40.0, CGRectGetWidth(screen_bounds_) - 40.0,
                             CGRectGetHeight(screen_bounds_) - 170.0);
  EXPECT_EQ(VisibleHeightForFrame(DockedKeyboardFrame(250.0)), 120.0);

  // Regular x Regular phone layout (e.g. unfolded foldable) applies the same
  // threshold.
  window_.frame = screen_bounds_;
  window_.traitOverrides.userInterfaceIdiom = UIUserInterfaceIdiomPhone;
  window_.traitOverrides.horizontalSizeClass = UIUserInterfaceSizeClassRegular;
  window_.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassRegular;
  [window_ updateTraitsIfNeeded];
  EXPECT_EQ(VisibleHeightForFrame(DockedKeyboardFrame(199.0)), 0.0);
  EXPECT_EQ(VisibleHeightForFrame(DockedKeyboardFrame(200.0)), 200.0);

  // Compact-height phone (e.g. iPhone landscape): short bars are not filtered.
  window_.traitOverrides.verticalSizeClass = UIUserInterfaceSizeClassCompact;
  [window_ updateTraitsIfNeeded];
  EXPECT_EQ(VisibleHeightForFrame(DockedKeyboardFrame(79.0)), 79.0);
}

}  // namespace
