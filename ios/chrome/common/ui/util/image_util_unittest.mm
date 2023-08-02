// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/image_util.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using ImageUtilTest = PlatformTest;

#define EXPECT_EQ_RECT(a, b) \
  EXPECT_NSEQ(NSStringFromCGRect(a), NSStringFromCGRect(b))
#define EXPECT_EQ_SIZE(a, b) \
  EXPECT_NSEQ(NSStringFromCGSize(a), NSStringFromCGSize(b))

TEST_F(ImageUtilTest, TestProjectionFill) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Resize with same aspect ratio.
  originalSize = CGSizeMake(100, 100);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, 0, 50, 50);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Resize with different aspect ratio.
  originalSize = CGSizeMake(100, 100);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, 0, 60, 40);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Stretch the other way.
  originalSize = CGSizeMake(100, 100);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, 0, 40, 60);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

TEST_F(ImageUtilTest, TestProjectionFit) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Landscape resize to 50x50, but squeezed into 50x25.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = CGSizeMake(50, 25);
  expectedProjection = CGRectMake(0, 0, 50, 25);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 60x40, but squeezed into 60x30.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = CGSizeMake(60, 30);
  expectedProjection = CGRectMake(0, 0, 60, 30);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 40x60, but squeezed into 40x20.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = CGSizeMake(40, 20);
  expectedProjection = CGRectMake(0, 0, 40, 20);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 50x50, but squeezed into 25x50.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = CGSizeMake(25, 50);
  expectedProjection = CGRectMake(0, 0, 25, 50);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 60x40, but squeezed into 20x40.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = CGSizeMake(20, 40);
  expectedProjection = CGRectMake(0, 0, 20, 40);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 40x60, but squeezed into 30x60.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = CGSizeMake(30, 60);
  expectedProjection = CGRectMake(0, 0, 30, 60);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFit,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

TEST_F(ImageUtilTest, TestProjectionAspectFill) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Landscape resize to 50x50
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(-25, 0, 100, 50);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 60x40, but squeezed into 60x30.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(-10, 0, 80, 40);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 40x60, but squeezed into 40x20.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(-40, 0, 120, 60);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 50x50, but squeezed into 25x50.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(50, 50);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, -25, 50, 100);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 60x40, but clipped to 20x40.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(60, 40);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, -40, 60, 120);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 40x60, but clipped to 30x60.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(40, 60);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, -10, 40, 80);
  CalculateProjection(originalSize, targetSize, ProjectionMode::kAspectFill,
                      revisedSize, projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

TEST_F(ImageUtilTest, TestProjectionAspectFillAlignTop) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Landscape resize to 100x100
  originalSize = CGSizeMake(400, 200);
  targetSize = CGSizeMake(100, 100);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(-50, 0, 200, 100);
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillAlignTop, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 100x100 and aligned to top
  originalSize = CGSizeMake(200, 400);
  targetSize = CGSizeMake(100, 100);
  expectedRevisedSize = targetSize;
  expectedProjection = CGRectMake(0, 0, 100, 200);
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillAlignTop, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

TEST_F(ImageUtilTest, TestProjectionAspectFillNoClipping) {
  CGSize originalSize, targetSize, expectedRevisedSize, revisedSize;
  CGRect expectedProjection, projection;

  // Landscape resize to 50x50
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(50, 50);
  expectedProjection = CGRectMake(0, 0, 100, 50);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 60x40.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(60, 40);
  expectedProjection = CGRectMake(0, 0, 80, 40);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Landscape resize to 40x60.
  originalSize = CGSizeMake(100, 50);
  targetSize = CGSizeMake(40, 60);
  expectedProjection = CGRectMake(0, 0, 120, 60);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 50x50.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(50, 50);
  expectedProjection = CGRectMake(0, 0, 50, 100);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 60x40.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(60, 40);
  expectedProjection = CGRectMake(0, 0, 60, 120);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);

  // Portrait resize to 40x60.
  originalSize = CGSizeMake(50, 100);
  targetSize = CGSizeMake(40, 60);
  expectedProjection = CGRectMake(0, 0, 40, 80);
  expectedRevisedSize = expectedProjection.size;
  CalculateProjection(originalSize, targetSize,
                      ProjectionMode::kAspectFillNoClipping, revisedSize,
                      projection);
  EXPECT_EQ_RECT(expectedProjection, projection);
  EXPECT_EQ_SIZE(expectedRevisedSize, revisedSize);
}

// Returns an image of random color in the same scale as the device main
// screen.
UIImage* testImage(CGSize imageSize) {
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.opaque = NO;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:imageSize format:format];

  return
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* UIContext) {
        CGContextRef context = UIContext.CGContext;
        CGContextSetRGBStrokeColor(context, 0, 0, 0, 1.0);
        CGContextSetRGBFillColor(context, 0, 0, 0, 1.0);
        CGContextFillRect(
            context, CGRectMake(0.0, 0.0, imageSize.width, imageSize.height));
      }];
}

TEST_F(ImageUtilTest, TestResizeImageOpacity) {
  UIImage* actual;
  UIImage* image = testImage(CGSizeMake(100, 100));
  actual =
      ResizeImage(image, CGSizeMake(50, 50), ProjectionMode::kAspectFit, YES);
  EXPECT_TRUE(actual);

  actual =
      ResizeImage(image, CGSizeMake(50, 50), ProjectionMode::kAspectFit, NO);
  EXPECT_TRUE(actual);
}

TEST_F(ImageUtilTest, TestResizeImageInvalidInput) {
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
