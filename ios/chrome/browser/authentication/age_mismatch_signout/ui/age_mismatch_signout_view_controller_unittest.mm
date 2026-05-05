// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_signout/ui/age_mismatch_signout_view_controller.h"

#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/ui/age_mismatch_prompt_mode.h"
#import "ios/chrome/browser/shared/ui/elements/home_waiting_view.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

class AgeMismatchSignoutViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[AgeMismatchSignoutViewController alloc]
        initWithMode:AgeMismatchPromptMode::kStandard];
  }

  base::test::TaskEnvironment task_environment_;
  AgeMismatchSignoutViewController* view_controller_;
};

// Tests that blockUI adds a waiting view and disables user interaction.
TEST_F(AgeMismatchSignoutViewControllerTest, TestBlockUI) {
  // Load the view.
  [view_controller_ view];

  EXPECT_TRUE(view_controller_.view.userInteractionEnabled);

  [view_controller_ blockUI];

  EXPECT_FALSE(view_controller_.view.userInteractionEnabled);

  BOOL has_waiting_view = NO;
  for (UIView* subview in view_controller_.view.subviews) {
    if ([subview isKindOfClass:[HomeWaitingView class]]) {
      has_waiting_view = YES;
      break;
    }
  }
  EXPECT_TRUE(has_waiting_view);
}

// Tests that hideStaySignedOutButton property hides the secondary button in
// kSigninFlow mode.
TEST_F(AgeMismatchSignoutViewControllerTest, TestHideStaySignedOutButton) {
  AgeMismatchSignoutViewController* vc =
      [[AgeMismatchSignoutViewController alloc]
          initWithMode:AgeMismatchPromptMode::kSigninFlow];
  [vc setShowStaySignedOutButton:NO];
  [vc view];

  EXPECT_TRUE(vc.configuration.secondaryActionString == nil ||
              [vc.configuration.secondaryActionString length] == 0);
}

}  // namespace
