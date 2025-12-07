// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Tests for the custom NewTabPageTrait.
class NewTabPageTraitTest : public PlatformTest {
 public:
  void SetUp() override { viewController_ = [[UIViewController alloc] init]; }

 protected:
  UIViewController* viewController_;
};

// Tests that setting and retrieving the palette works.
TEST_F(NewTabPageTraitTest, TestSettingNewTabPageTrait) {
  NewTabPageColorPalette* palette = [[NewTabPageColorPalette alloc] init];

  [[[CustomUITraitAccessor alloc]
      initWithMutableTraits:viewController_.traitOverrides]
      setObjectForNewTabPageTrait:palette];

  EXPECT_EQ(palette,
            [viewController_.traitCollection objectForNewTabPageTrait]);
}
