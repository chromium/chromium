// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_blur_view.h"

#import "base/test/ios/wait_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;

class AppBarBlurViewTest : public PlatformTest {
 public:
  AppBarBlurViewTest() {
    window_ = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 200, 200)];
    blur_view_ =
        [[AppBarBlurView alloc] initWithEffectStyle:UIBlurEffectStyleDark
                                    maxBlurFraction:0.5];
    [window_ addSubview:blur_view_];
  }

 protected:
  UIWindow* window_;
  AppBarBlurView* blur_view_;
};

// Tests that AppBarBlurView initializes with a default blurAmount of 0.0.
TEST_F(AppBarBlurViewTest, TestInitialization) {
  EXPECT_EQ(blur_view_.blurAmount, 0.0);
}

// Tests that setting blurAmount synchronously updates the property and clamps
// to the [0.0, 1.0] range.
TEST_F(AppBarBlurViewTest, TestSetBlurAmount) {
  blur_view_.blurAmount = 0.5;
  EXPECT_EQ(blur_view_.blurAmount, 0.5);

  blur_view_.blurAmount = 1.0;
  EXPECT_EQ(blur_view_.blurAmount, 1.0);

  // Clamping above 1.0.
  blur_view_.blurAmount = 1.5;
  EXPECT_EQ(blur_view_.blurAmount, 1.0);

  // Clamping below 0.0.
  blur_view_.blurAmount = -0.5;
  EXPECT_EQ(blur_view_.blurAmount, 0.0);
}

// Tests that setting blurAmount inside a UIView animation block animates the
// blurAmount to the target value.
TEST_F(AppBarBlurViewTest, TestAnimatedBlurAmount) {
  blur_view_.blurAmount = 0.0;

  [UIView animateWithDuration:0.05
                   animations:^{
                     blur_view_.blurAmount = 1.0;
                   }];

  bool completed = WaitUntilConditionOrTimeout(base::Seconds(1.0), ^{
    return blur_view_.blurAmount == 1.0;
  });
  EXPECT_TRUE(completed);
}

// Tests that effect is nil when blurAmount is 0.0 and non-nil during partial
// blur.
TEST_F(AppBarBlurViewTest, TestEffectClearedAtZero) {
  EXPECT_EQ(blur_view_.blurAmount, 0.0);
  EXPECT_NSEQ(blur_view_.effect, nil);

  blur_view_.blurAmount = 0.5;
  EXPECT_NE(blur_view_.effect, nil);

  blur_view_.blurAmount = 0.0;
  EXPECT_NSEQ(blur_view_.effect, nil);
}

// Tests that effect is cleared when CADisplayLink animation completes to 1.0.
TEST_F(AppBarBlurViewTest, TestEffectClearedAfterAnimationToFull) {
  blur_view_.blurAmount = 0.0;

  [UIView animateWithDuration:0.05
                   animations:^{
                     blur_view_.blurAmount = 1.0;
                   }];

  bool completed = WaitUntilConditionOrTimeout(base::Seconds(1.0), ^{
    return blur_view_.blurAmount == 1.0 && blur_view_.effect == nil;
  });
  EXPECT_TRUE(completed);
}
