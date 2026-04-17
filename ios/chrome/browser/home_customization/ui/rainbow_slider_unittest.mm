// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/rainbow_slider+testing.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class RainbowSliderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    slider_ = [[RainbowSlider alloc] initWithFrame:CGRectMake(0, 0, 300, 44)];
    [slider_ layoutIfNeeded];
  }

  // Returns the hue component of the slider's current thumb image tint.
  CGFloat SliderHue() {
    CGFloat hue = 0;
    UIColor* color = [UIColor colorWithHue:slider_.value
                                saturation:1.0
                                brightness:1.0
                                     alpha:1.0];
    [color getHue:&hue saturation:nil brightness:nil alpha:nil];
    return hue;
  }

  RainbowSlider* slider_;
};

// Tests that tapping at the leading edge sets value to 0 and hue to 0.
TEST_F(RainbowSliderTest, TapAtStart) {
  [slider_ setValue:0.5];
  [slider_ updateValueForLocationX:0.0];
  EXPECT_FLOAT_EQ(slider_.value, 0.0);
  EXPECT_NEAR(SliderHue(), 0.0, 0.01);
}

// Tests that tapping at the trailing edge sets value to 1. Hue wraps around
// (1.0 == 0.0 on the color wheel), so UIColor reports 0.
TEST_F(RainbowSliderTest, TapAtEnd) {
  [slider_ updateValueForLocationX:300.0];
  EXPECT_FLOAT_EQ(slider_.value, 1.0);
  EXPECT_NEAR(SliderHue(), 0.0, 0.01);
}

// Tests that tapping at the midpoint sets value and hue to ~0.5.
TEST_F(RainbowSliderTest, TapAtMidpoint) {
  [slider_ updateValueForLocationX:150.0];
  EXPECT_NEAR(slider_.value, 0.5, 0.01);
  EXPECT_NEAR(SliderHue(), 0.5, 0.01);
}

// Tests an invalid negative param could be handled.
TEST_F(RainbowSliderTest, InvalidNegativeInput) {
  [slider_ setValue:0.5];
  [slider_ updateValueForLocationX:-50.0];
  EXPECT_FLOAT_EQ(slider_.value, 0.0);
}

// Tests an invalid large param could be handled.
TEST_F(RainbowSliderTest, InvalidLargeInput) {
  [slider_ updateValueForLocationX:500.0];
  EXPECT_FLOAT_EQ(slider_.value, 1.0);
}

// Tests that the thumb image is updated after a tap.
TEST_F(RainbowSliderTest, ThumbImageUpdatedAfterTap) {
  UIImage* thumbBefore = [slider_ thumbImageForState:UIControlStateNormal];
  [slider_ updateValueForLocationX:200.0];
  UIImage* thumbAfter = [slider_ thumbImageForState:UIControlStateNormal];
  EXPECT_NSNE(thumbBefore, thumbAfter);
}
