// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ImageContentConfigurationTest = PlatformTest;

// Tests that the content size is correct for ImageContentConfiguration when no
// size is specified.
TEST_F(ImageContentConfigurationTest, ContentSizeNoSize) {
  ImageContentConfiguration* config = [[ImageContentConfiguration alloc] init];

  UIGraphicsBeginImageContext(CGSizeMake(10, 20));
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  config.image = image;

  UIView* content_view = [config makeContentView];
  [content_view setNeedsLayout];
  [content_view layoutIfNeeded];

  CGSize content_size = [config contentSize];
  EXPECT_EQ(content_size.width, 10);
  EXPECT_EQ(content_size.height, 20);
  EXPECT_EQ(content_size.width, content_view.intrinsicContentSize.width);
  EXPECT_EQ(content_size.height, content_view.intrinsicContentSize.height);
}

// Tests that the content size is correct for ImageContentConfiguration when
// asking for specific size.
TEST_F(ImageContentConfigurationTest, ContentSizeForcedSize) {
  ImageContentConfiguration* config = [[ImageContentConfiguration alloc] init];

  UIGraphicsBeginImageContext(CGSizeMake(10, 20));
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  config.image = image;

  // Test with explicit image size.
  config.imageSize = CGSizeMake(50, 60);

  UIView* content_view = [config makeContentView];

  CGSize compressed_size =
      [content_view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];

  CGSize content_size = [config contentSize];
  EXPECT_EQ(content_size.width, 50);
  EXPECT_EQ(content_size.height, 60);
  EXPECT_EQ(content_size.width, compressed_size.width);
  EXPECT_EQ(content_size.height, compressed_size.height);
}

}  // namespace
