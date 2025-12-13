// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/activity_indicator_content_configuration.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ActivityIndicatorContentConfigurationTest = PlatformTest;

// Tests that the content size is correct for
// ActivityIndicatorContentConfiguration.
TEST_F(ActivityIndicatorContentConfigurationTest, ContentSize) {
  ActivityIndicatorContentConfiguration* config =
      [[ActivityIndicatorContentConfiguration alloc] init];

  CGSize content_size = [config contentSize];

  UIView* content_view = [config makeContentView];
  [content_view setNeedsLayout];
  [content_view layoutIfNeeded];

  CGSize compressed_size =
      [content_view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];

  EXPECT_EQ(content_size.width, 30);
  EXPECT_EQ(content_size.height, 30);
  EXPECT_EQ(content_size.width, compressed_size.width);
  EXPECT_EQ(content_size.height, compressed_size.height);
}
