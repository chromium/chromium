// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/info_button_content_configuration.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using InfoButtonContentConfigurationTest = PlatformTest;

// Tests that the content size is correct for InfoButtonContentConfiguration.
TEST_F(InfoButtonContentConfigurationTest, ContentSize) {
  InfoButtonContentConfiguration* config =
      [[InfoButtonContentConfiguration alloc] init];

  CGSize content_size = [config contentSize];

  UIView* content_view = [config makeContentView];
  [content_view setNeedsLayout];
  [content_view layoutIfNeeded];

  CGSize compressed_size =
      [content_view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];

  // kButtonSize is internal to the implementation (27).
  EXPECT_EQ(content_size.width, 27);
  EXPECT_EQ(content_size.height, 27);
  EXPECT_EQ(content_size.width, compressed_size.width);
  EXPECT_EQ(content_size.height, compressed_size.height);
}

}  // namespace
