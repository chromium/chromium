// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"

#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ColorfulSymbolContentConfigurationTest = PlatformTest;

// Tests that the content size is correct for
// ColorfulSymbolContentConfiguration.
TEST_F(ColorfulSymbolContentConfigurationTest, ContentSize) {
  ColorfulSymbolContentConfiguration* config =
      [[ColorfulSymbolContentConfiguration alloc] init];

  CGSize content_size = [config contentSize];
  UIView* content_view = [config makeContentView];
  [content_view setNeedsLayout];
  [content_view layoutIfNeeded];

  EXPECT_EQ(content_size.width, kTableViewIconImageSize);
  EXPECT_EQ(content_size.height, kTableViewIconImageSize);
  EXPECT_EQ(content_size.width, content_view.bounds.size.width);
  EXPECT_EQ(content_size.height, content_view.bounds.size.height);
}

}  // namespace
