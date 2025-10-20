// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/button_stack/button_stack_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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
  EXPECT_TRUE(view_controller_.primaryActionButton.configuration
                  .showsActivityIndicator);
  EXPECT_NSEQ(@"", view_controller_.primaryActionButton.configuration.title);

  [view_controller_ setLoading:NO];
  EXPECT_TRUE(view_controller_.primaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.secondaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.primaryActionButton.configuration
                   .showsActivityIndicator);
  EXPECT_NSEQ(configuration_.primaryActionString,
              view_controller_.primaryActionButton.configuration.title);
}

// Tests the confirmed state.
TEST_F(ButtonStackViewControllerTest, TestConfirmedState) {
  [view_controller_ setConfirmed:YES];
  EXPECT_FALSE(view_controller_.primaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.secondaryActionButton.enabled);
  EXPECT_FALSE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_NE(nil, view_controller_.primaryActionButton.configuration.image);
  EXPECT_NSEQ(@"", view_controller_.primaryActionButton.configuration.title);

  [view_controller_ setConfirmed:NO];
  EXPECT_TRUE(view_controller_.primaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.secondaryActionButton.enabled);
  EXPECT_TRUE(view_controller_.tertiaryActionButton.enabled);
  EXPECT_EQ(configuration_.primaryActionImage,
            view_controller_.primaryActionButton.configuration.image);
  EXPECT_NSEQ(configuration_.primaryActionString,
              view_controller_.primaryActionButton.configuration.title);
}

// Tests that setting loading to YES when confirmed is YES sets confirmed to NO.
TEST_F(ButtonStackViewControllerTest, TestLoadingWhenConfirmed) {
  [view_controller_ setConfirmed:YES];
  EXPECT_NE(nil, view_controller_.primaryActionButton.configuration.image);

  [view_controller_ setLoading:YES];
  EXPECT_TRUE(view_controller_.primaryActionButton.configuration
                  .showsActivityIndicator);
  // The image should be cleared when loading.
  EXPECT_EQ(nil, view_controller_.primaryActionButton.configuration.image);
}

// Tests that setting confirmed to YES when loading is YES sets loading to NO.
TEST_F(ButtonStackViewControllerTest, TestConfirmedWhenLoading) {
  [view_controller_ setLoading:YES];
  EXPECT_TRUE(view_controller_.primaryActionButton.configuration
                  .showsActivityIndicator);

  [view_controller_ setConfirmed:YES];
  EXPECT_FALSE(view_controller_.primaryActionButton.configuration
                   .showsActivityIndicator);
  EXPECT_NE(nil, view_controller_.primaryActionButton.configuration.image);
}

// Tests that images are correctly set on the buttons.
TEST_F(ButtonStackViewControllerTest, TestButtonImages) {
  const CGFloat kTestSymbolPointSize = 15.0;
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kTestSymbolPointSize];
  UIImage* primaryImage = [UIImage systemImageNamed:@"pencil"
                                  withConfiguration:config];
  UIImage* secondaryImage = [UIImage systemImageNamed:@"trash"
                                    withConfiguration:config];
  UIImage* tertiaryImage = [UIImage systemImageNamed:@"folder"
                                   withConfiguration:config];

  configuration_.primaryActionImage = primaryImage;
  configuration_.secondaryActionImage = secondaryImage;
  configuration_.tertiaryActionImage = tertiaryImage;

  view_controller_ =
      [[ButtonStackViewController alloc] initWithConfiguration:configuration_];
  [view_controller_ view];

  EXPECT_EQ(primaryImage,
            view_controller_.primaryActionButton.configuration.image);
  EXPECT_EQ(secondaryImage,
            view_controller_.secondaryActionButton.configuration.image);
  EXPECT_EQ(tertiaryImage,
            view_controller_.tertiaryActionButton.configuration.image);
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
  new_configuration.primaryButtonStyle =
      ButtonStackButtonStylePrimaryDestructive;
  [view_controller_ updateConfiguration:new_configuration];

  // Verify the button style was updated.
  UIColor* destructiveColor = [UIColor colorNamed:kRedColor];
  UIButton* button = view_controller_.primaryActionButton;

  BOOL background_as_tint;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if (@available(iOS 26, *)) {
    background_as_tint = true;
  } else {
#endif
    background_as_tint = false;
#if defined(__IPHONE_26_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  }
#endif

  if (background_as_tint) {
    EXPECT_TRUE([destructiveColor isEqual:button.tintColor]);
  } else {
    EXPECT_TRUE([destructiveColor
        isEqual:button.configuration.background.backgroundColor]);
  }
}
