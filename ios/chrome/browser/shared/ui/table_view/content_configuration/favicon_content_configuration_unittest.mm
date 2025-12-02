// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using FaviconContentConfigurationTest = PlatformTest;

// Tests that the content size is correct for FaviconContentConfiguration.
TEST_F(FaviconContentConfigurationTest, ContentSize) {
  FaviconContentConfiguration* config =
      [[FaviconContentConfiguration alloc] init];

  CGSize content_size = [config contentSize];

  UIView* content_view = [config makeContentView];
  [content_view setNeedsLayout];
  [content_view layoutIfNeeded];

  // kFaviconContainerWidth is internal to the implementation (30).
  EXPECT_EQ(content_size.width, 30);
  EXPECT_EQ(content_size.height, 30);
  EXPECT_EQ(content_size.width, content_view.bounds.size.width);
  EXPECT_EQ(content_size.height, content_view.bounds.size.height);
}

}  // namespace
