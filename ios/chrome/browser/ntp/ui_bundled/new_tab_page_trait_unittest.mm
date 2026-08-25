// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_card_background_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

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

// Tests NTPCardBackgroundColor when NewTabPageRedesign is enabled.
TEST_F(NewTabPageTraitTest, TestNTPCardBackgroundColorRedesign) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

  EXPECT_NSEQ([UIColor colorNamed:kNTPCardBackgroundColor],
              NTPCardBackgroundColor(nil));
}

// Tests NTPCardBackgroundColor returns secondaryCellColor when a palette is
// provided.
TEST_F(NewTabPageTraitTest, TestNTPCardBackgroundColorWithPalette) {
  NewTabPageColorPalette* palette = [[NewTabPageColorPalette alloc] init];
  palette.secondaryCellColor = [UIColor colorNamed:kRed500Color];

  EXPECT_NSEQ([UIColor colorNamed:kRed500Color],
              NTPCardBackgroundColor(palette));
}

// Tests that NTPCardBackgroundView defaults to card background color.
TEST_F(NewTabPageTraitTest, TestNTPCardBackgroundViewDefault) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

  NTPCardBackgroundView* card_bg = [[NTPCardBackgroundView alloc] init];
  UIView* color_view = [card_bg valueForKey:@"_backgroundColorView"];
  EXPECT_NE(nil, color_view);
  EXPECT_NSEQ([UIColor colorNamed:kNTPCardBackgroundColor],
              color_view.backgroundColor);
  EXPECT_EQ(nil, [card_bg valueForKey:@"_backgroundBlurView"]);
}
