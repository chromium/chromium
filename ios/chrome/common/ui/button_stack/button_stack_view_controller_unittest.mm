// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Test delegate to check if the correct methods are called.
@interface ButtonStackActionTestDelegate : NSObject <ButtonStackActionDelegate>
@property(nonatomic, assign) BOOL primaryActionTapped;
@property(nonatomic, assign) BOOL secondaryActionTapped;
@property(nonatomic, assign) BOOL tertiaryActionTapped;
@end

@implementation ButtonStackActionTestDelegate
- (void)didTapPrimaryActionButton {
  self.primaryActionTapped = YES;
}
- (void)didTapSecondaryActionButton {
  self.secondaryActionTapped = YES;
}
- (void)didTapTertiaryActionButton {
  self.tertiaryActionTapped = YES;
}
@end

// Test fixture for ButtonStackViewController.
class ButtonStackViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    configuration_ = [[ButtonStackConfiguration alloc] init];
    configuration_.primaryActionString = @"Primary";
    configuration_.secondaryActionString = @"Secondary";
    configuration_.tertiaryActionString = @"Tertiary";

    view_controller_ = [[ButtonStackViewController alloc]
        initWithConfiguration:configuration_];
    delegate_ = [[ButtonStackActionTestDelegate alloc] init];
    view_controller_.actionDelegate = delegate_;

    // Load the view.
    [view_controller_ view];
  }

  ButtonStackConfiguration* configuration_;
  ButtonStackViewController* view_controller_;
  ButtonStackActionTestDelegate* delegate_;
};

// Tests that tapping the primary button calls the delegate.
TEST_F(ButtonStackViewControllerTest, TestPrimaryAction) {
  EXPECT_FALSE(delegate_.primaryActionTapped);
  [view_controller_.primaryActionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(delegate_.primaryActionTapped);
}

// Tests that tapping the secondary button calls the delegate.
TEST_F(ButtonStackViewControllerTest, TestSecondaryAction) {
  EXPECT_FALSE(delegate_.secondaryActionTapped);
  [view_controller_.secondaryActionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(delegate_.secondaryActionTapped);
}

// Tests that tapping the tertiary button calls the delegate.
TEST_F(ButtonStackViewControllerTest, TestTertiaryAction) {
  EXPECT_FALSE(delegate_.tertiaryActionTapped);
  [view_controller_.tertiaryActionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_TRUE(delegate_.tertiaryActionTapped);
}

// Tests the loading state.
TEST_F(ButtonStackViewControllerTest, TestLoadingState) {
  [view_controller_ setLoading:YES];
  EXPECT_FALSE(view_controller_.primaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.secondaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_EQ(PrimaryButtonImageSpinner,
            view_controller_.primaryActionButton.primaryButtonImage);
  EXPECT_NSEQ(@"", view_controller_.primaryActionButton.configuration.title);

  [view_controller_ setLoading:NO];
  EXPECT_TRUE(view_controller_.primaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.secondaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_EQ(PrimaryButtonImageNone,
            view_controller_.primaryActionButton.primaryButtonImage);
  EXPECT_NSEQ(configuration_.primaryActionString,
              view_controller_.primaryActionButton.configuration.title);
}

// Tests the confirmed state.
TEST_F(ButtonStackViewControllerTest, TestConfirmedState) {
  [view_controller_ setConfirmed:YES];
  EXPECT_FALSE(view_controller_.primaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.secondaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_EQ(PrimaryButtonImageCheckmark,
            view_controller_.primaryActionButton.primaryButtonImage);
  EXPECT_NSEQ(@"", view_controller_.primaryActionButton.configuration.title);

  [view_controller_ setConfirmed:NO];
  EXPECT_TRUE(view_controller_.primaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.secondaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_EQ(PrimaryButtonImageNone,
            view_controller_.primaryActionButton.primaryButtonImage);
  EXPECT_NSEQ(configuration_.primaryActionString,
              view_controller_.primaryActionButton.configuration.title);
}

// Tests that setting loading to YES when confirmed is YES sets confirmed to NO.
TEST_F(ButtonStackViewControllerTest, TestLoadingWhenConfirmed) {
  [view_controller_ setConfirmed:YES];
  EXPECT_EQ(PrimaryButtonImageCheckmark,
            view_controller_.primaryActionButton.primaryButtonImage);

  [view_controller_ setLoading:YES];
  EXPECT_EQ(PrimaryButtonImageSpinner,
            view_controller_.primaryActionButton.primaryButtonImage);
}

// Tests that setting confirmed to YES when loading is YES sets loading to NO.
TEST_F(ButtonStackViewControllerTest, TestConfirmedWhenLoading) {
  [view_controller_ setLoading:YES];
  EXPECT_EQ(PrimaryButtonImageSpinner,
            view_controller_.primaryActionButton.primaryButtonImage);

  [view_controller_ setConfirmed:YES];
  EXPECT_EQ(PrimaryButtonImageCheckmark,
            view_controller_.primaryActionButton.primaryButtonImage);
}

// Tests that buttons are only created if they have a title.
TEST_F(ButtonStackViewControllerTest, TestPartialConfiguration) {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  configuration.primaryActionString = @"Primary Only";
  ButtonStackViewController* view_controller =
      [[ButtonStackViewController alloc] initWithConfiguration:configuration];
  [view_controller view];
  EXPECT_FALSE(view_controller.primaryActionButton.hidden);
  EXPECT_TRUE(view_controller.secondaryActionButton.hidden);
  EXPECT_TRUE(view_controller.tertiaryActionButton.hidden);
}

// Tests that updating the configuration correctly updates the buttons.
TEST_F(ButtonStackViewControllerTest, TestUpdateConfiguration) {
  // All buttons should be visible initially.
  EXPECT_FALSE(view_controller_.primaryActionButton.hidden);
  EXPECT_FALSE(view_controller_.secondaryActionButton.hidden);
  EXPECT_FALSE(view_controller_.tertiaryActionButton.hidden);

  // Update to a configuration with only a secondary action.
  ButtonStackConfiguration* new_configuration =
      [[ButtonStackConfiguration alloc] init];
  new_configuration.secondaryActionString = @"New Secondary";
  [view_controller_ updateConfiguration:new_configuration];

  // Verify the button states.
  EXPECT_TRUE(view_controller_.primaryActionButton.hidden);
  EXPECT_FALSE(view_controller_.secondaryActionButton.hidden);
  EXPECT_TRUE(view_controller_.tertiaryActionButton.hidden);
  EXPECT_NSEQ(@"New Secondary",
              view_controller_.secondaryActionButton.configuration.title);
}

// Tests that updating the configuration correctly updates the button styles.
TEST_F(ButtonStackViewControllerTest, TestUpdateConfigurationStyle) {
  // Update to a destructive primary action.
  ButtonStackConfiguration* new_configuration =
      [[ButtonStackConfiguration alloc] init];
  new_configuration.primaryActionString = @"Destructive";
  new_configuration.primaryButtonStyle = ChromeButtonStylePrimaryDestructive;
  [view_controller_ updateConfiguration:new_configuration];

  // Verify the button style was updated.
  EXPECT_EQ(ChromeButtonStylePrimaryDestructive,
            view_controller_.primaryActionButton.style);
}

// Tests that reloading the configuration correctly updates the buttons.
TEST_F(ButtonStackViewControllerTest, TestReloadConfiguration) {
  // All buttons should be visible initially.
  EXPECT_NSEQ(@"Primary",
              view_controller_.primaryActionButton.configuration.title);

  // Update the configuration directly.
  view_controller_.configuration.primaryActionString = @"New Primary";
  [view_controller_ reloadConfiguration];

  // Verify the button title has been updated.
  EXPECT_NSEQ(@"New Primary",
              view_controller_.primaryActionButton.configuration.title);
}
