// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_configuration.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using SwitchContentConfigurationTest = PlatformTest;

// Tests that the content size is correct for SwitchContentConfiguration.
TEST_F(SwitchContentConfigurationTest, ContentSize) {
  SwitchContentConfiguration* config =
      [[SwitchContentConfiguration alloc] init];

  CGSize content_size = [config contentSize];

  UIView* content_view = [config makeContentView];
  [content_view setNeedsLayout];
  [content_view layoutIfNeeded];

  CGSize compressed_size =
      [content_view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];

  if (@available(iOS 26, *)) {
    // Those are used as golden value for iOS 26. If the value changes because
    // the design of the switch change, update them.
    EXPECT_EQ(content_size.width, 61);
    EXPECT_EQ(content_size.height, 28);
  }
  EXPECT_EQ(content_size.width, compressed_size.width);
  EXPECT_EQ(content_size.height, compressed_size.height);
}

}  // namespace
