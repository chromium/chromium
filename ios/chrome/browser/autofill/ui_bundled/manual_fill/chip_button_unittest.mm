// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/chip_button.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

namespace {

using ChipButtonTest = PlatformTest;

// Returns the font defined in the button's configuration.
UIFont* GetTitleFont(UIButtonConfiguration* button_configuration) {
  return [button_configuration.attributedTitle attribute:NSFontAttributeName
                                                 atIndex:0
                                          effectiveRange:nullptr];
}

// Tests that the chip button has the expected configuration after its title is
// set.
TEST_F(ChipButtonTest, SetTitle) {
  UIFont* font = [[UIFontMetrics defaultMetrics]
      scaledFontForFont:[UIFont systemFontOfSize:14 weight:UIFontWeightMedium]];
  NSString* title = @"Title";

  {
    base::test::ScopedFeatureList feature_list(kIOSKeyboardAccessoryUpgrade);

    ChipButton* button = [[ChipButton alloc] initWithFrame:CGRectZero];

    [button setTitle:title forState:UIControlStateNormal];

    UIButtonConfiguration* button_configuration = button.configuration;
    EXPECT_TRUE([button_configuration.baseForegroundColor
        isEqual:[UIColor colorNamed:kTextPrimaryColor]]);

    // UI updates related to the kIOSKeyboardAccessoryUpgrade feature are only
    // applied to iPhone for now.
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      EXPECT_NSEQ(button.currentTitle, title);
      EXPECT_NE(button.titleLabel.font, font);
    } else {
      EXPECT_NSEQ(button_configuration.attributedTitle.string, title);
      EXPECT_EQ(GetTitleFont(button_configuration), font);
    }
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kIOSKeyboardAccessoryUpgrade);

    ChipButton* button = [[ChipButton alloc] initWithFrame:CGRectZero];

    [button setTitle:title forState:UIControlStateNormal];

    UIButtonConfiguration* button_configuration = button.configuration;
    EXPECT_TRUE([button_configuration.baseForegroundColor
        isEqual:[UIColor colorNamed:kTextPrimaryColor]]);
    EXPECT_NSEQ(button.currentTitle, title);
    EXPECT_NE(button.titleLabel.font, font);
  }
}

// Tests that the font color of the chip button's title is not changed after a
// call to `setEnabled`.
TEST_F(ChipButtonTest, SetEnabledDoesntChangeFontColor) {
  ChipButton* button = [[ChipButton alloc] initWithFrame:CGRectZero];
  EXPECT_TRUE([button.configuration.baseForegroundColor
      isEqual:[UIColor colorNamed:kTextPrimaryColor]]);

  [button setEnabled:NO];

  EXPECT_TRUE([button.configuration.baseForegroundColor
      isEqual:[UIColor colorNamed:kTextPrimaryColor]]);
}

}  // namespace
