// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/image/image_util.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Creates a test UIImage with the given size and returns its PNG data.
NSData* CreateTestImageData(CGSize size) {
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = 1.0;
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];
  UIImage* image =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [[UIColor blackColor] setFill];
        [context fillRect:CGRectMake(0, 0, size.width, size.height)];
      }];
  return UIImagePNGRepresentation(image);
}

// Creates a test JPEG image with the given size and EXIF orientation.
NSData* CreateTestImageDataWithOrientation(
    CGSize size,
    CGImagePropertyOrientation orientation) {
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = 1.0;
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];
  UIImage* image =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [[UIColor blackColor] setFill];
        [context fillRect:CGRectMake(0, 0, size.width, size.height)];
      }];

  NSMutableData* data = [NSMutableData data];
  CGImageDestinationRef destination = CGImageDestinationCreateWithData(
      (__bridge CFMutableDataRef)data,
      (__bridge CFStringRef)UTTypeJPEG.identifier, 1, nil);
  if (!destination) {
    return nil;
  }

  NSDictionary* properties =
      @{(__bridge id)kCGImagePropertyOrientation : @(orientation)};

  CGImageDestinationAddImage(destination, image.CGImage,
                             (__bridge CFDictionaryRef)properties);
  if (!CGImageDestinationFinalize(destination)) {
    CFRelease(destination);
    return nil;
  }
  CFRelease(destination);
  return data;
}

using ImageUtilTest = PlatformTest;

// Tests ImageSizeFromData with valid, nil, and invalid inputs.
TEST_F(ImageUtilTest, ImageSizeFromData) {
  // Valid image data.
  NSData* data = CreateTestImageData(CGSizeMake(200, 100));
  ASSERT_TRUE(data);
  CGSize size = ImageSizeFromData(data);
  EXPECT_EQ(200, size.width);
  EXPECT_EQ(100, size.height);

  // Nil data.
  size = ImageSizeFromData(nil);
  EXPECT_EQ(0, size.width);
  EXPECT_EQ(0, size.height);

  // Invalid data.
  size = ImageSizeFromData(
      [@"not an image" dataUsingEncoding:NSUTF8StringEncoding]);
  EXPECT_EQ(0, size.width);
  EXPECT_EQ(0, size.height);
}

// Tests that ImageSizeFromData respects EXIF orientation and swaps dimensions
// if needed.
TEST_F(ImageUtilTest, ImageSizeFromDataWithOrientation) {
  // Create a landscape image (200x100) but set orientation to Right (6),
  // which means it should be displayed as portrait (100x200).
  NSData* data = CreateTestImageDataWithOrientation(
      CGSizeMake(200, 100), kCGImagePropertyOrientationRight);
  ASSERT_TRUE(data);
  CGSize size = ImageSizeFromData(data);
  EXPECT_EQ(100, size.width);
  EXPECT_EQ(200, size.height);
}

// Tests DownsampledImageFromData with normal, scaled, and larger-target cases.
TEST_F(ImageUtilTest, DownsampledImageFromData) {
  NSData* data = CreateTestImageData(CGSizeMake(1000, 500));
  ASSERT_TRUE(data);

  // Normal downsampling at 1x scale.
  UIImage* image =
      DownsampledImageFromData(data, CGSizeMake(100, 50), /*scale=*/1.0);
  ASSERT_TRUE(image);
  EXPECT_NEAR(100, image.size.width, 1);
  EXPECT_NEAR(50, image.size.height, 1);
  EXPECT_DOUBLE_EQ(1.0, image.scale);

  // With 2x scale — image.size is in points (pixels / scale).
  image = DownsampledImageFromData(data, CGSizeMake(100, 50), /*scale=*/2.0);
  ASSERT_TRUE(image);
  EXPECT_NEAR(100, image.size.width, 1);
  EXPECT_NEAR(50, image.size.height, 1);
  EXPECT_DOUBLE_EQ(2.0, image.scale);

  // Target larger than original — should return original size.
  NSData* smallData = CreateTestImageData(CGSizeMake(50, 50));
  ASSERT_TRUE(smallData);
  image = DownsampledImageFromData(smallData, CGSizeMake(1000, 1000),
                                   /*scale=*/1.0);
  ASSERT_TRUE(image);
  EXPECT_NEAR(50, image.size.width, 1);
  EXPECT_NEAR(50, image.size.height, 1);
}

// Tests DownsampledImageFromData returns nil for invalid inputs.
TEST_F(ImageUtilTest, DownsampledImageFromDataInvalidInput) {
  EXPECT_FALSE(
      DownsampledImageFromData(nil, CGSizeMake(100, 50), /*scale=*/1.0));
  EXPECT_FALSE(DownsampledImageFromData(
      [@"not an image" dataUsingEncoding:NSUTF8StringEncoding],
      CGSizeMake(100, 50), /*scale=*/1.0));
}

// Tests DownsampledImageFromURL with a valid file.
TEST_F(ImageUtilTest, DownsampledImageFromURL) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  NSData* data = CreateTestImageData(CGSizeMake(1000, 500));
  ASSERT_TRUE(data);

  base::FilePath image_path = temp_dir.GetPath().Append("test.png");
  ASSERT_TRUE(base::WriteFile(image_path, base::apple::NSDataToSpan(data)));

  NSURL* imageURL =
      [NSURL fileURLWithPath:base::SysUTF8ToNSString(image_path.value())];
  UIImage* image =
      DownsampledImageFromURL(imageURL, CGSizeMake(100, 50), /*scale=*/1.0);
  ASSERT_TRUE(image);
  EXPECT_NEAR(100, image.size.width, 1);
  EXPECT_NEAR(50, image.size.height, 1);
  EXPECT_DOUBLE_EQ(1.0, image.scale);
}

// Tests DownsampledImageFromURL returns nil for invalid inputs.
TEST_F(ImageUtilTest, DownsampledImageFromURLInvalidInput) {
  EXPECT_FALSE(
      DownsampledImageFromURL(nil, CGSizeMake(100, 50), /*scale=*/1.0));
  EXPECT_FALSE(DownsampledImageFromURL(
      [NSURL fileURLWithPath:@"/tmp/nonexistent_image.png"],
      CGSizeMake(100, 50), /*scale=*/1.0));
}

}  // namespace
