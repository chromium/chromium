// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/crossfade_label.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
constexpr NSString* kTestText = @"Test Text";
}

class CrossfadeLabelTest : public PlatformTest {
 public:
  CrossfadeLabelTest() {
    _view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)];
    _label = [[CrossfadeLabel alloc] init];
    _label.text = kTestText;
    [_view addSubview:_label];
  }

 protected:
  UIView* _view;
  CrossfadeLabel* _label;
};

// Tests the crossfade by calling `crossfadeSetup:`, `crossfadeAnimation`, and
// `crossfadeCleanup`, ensuring the color and opacity are correct at each step.
TEST_F(CrossfadeLabelTest, testCrossfade) {
  UIColor* black = [UIColor colorNamed:kSolidBlackColor];
  UIColor* green = [UIColor colorNamed:kGreenColor];

  EXPECT_EQ(_view.subviews.count, 1ul);

  _label.textColor = black;

  [_label setUpCrossfadeWithTextColor:green attributedText:nil];

  EXPECT_EQ(_label.textColor, black);
  EXPECT_EQ(_label.alpha, 1);
  EXPECT_EQ(_view.subviews.count, 2ul);

  __block int steps_completed = 0;
  [UIView animateWithDuration:0.01
      animations:^{
        [_label crossfade];
        EXPECT_EQ(_label.textColor, black);
        EXPECT_EQ(_label.alpha, 0);
        steps_completed++;
      }
      completion:^(BOOL finished) {
        [_label cleanupAfterCrossfade];
        EXPECT_EQ(_label.textColor, green);
        EXPECT_EQ(_label.alpha, 1);
        EXPECT_EQ(_view.subviews.count, 1ul);
        steps_completed++;
      }];

  // Wait for animation to complete.
  auto wait_condition = ^{
    return steps_completed == 2;
  };
  bool completed =
      WaitUntilConditionOrTimeout(base::Seconds(0.5), wait_condition);
  EXPECT_TRUE(completed);
}
