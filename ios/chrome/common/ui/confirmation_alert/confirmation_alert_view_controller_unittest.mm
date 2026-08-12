// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test action handler for ConfirmationAlertViewController.
@interface ConfirmationAlertActionTestHandler
    : NSObject <ConfirmationAlertActionHandler>
@property(nonatomic, assign) BOOL primaryActionTapped;
@property(nonatomic, assign) BOOL secondaryActionTapped;
@property(nonatomic, assign) BOOL tertiaryActionTapped;
@property(nonatomic, assign) BOOL dismissed;
@end

@implementation ConfirmationAlertActionTestHandler
- (void)confirmationAlertPrimaryAction {
  self.primaryActionTapped = YES;
}
- (void)confirmationAlertSecondaryAction {
  self.secondaryActionTapped = YES;
}
- (void)confirmationAlertTertiaryAction {
  self.tertiaryActionTapped = YES;
}
- (void)confirmationAlertDismissed {
  self.dismissed = YES;
}
@end

// Subclass to override isBeingDismissed in unit tests.
@interface TestConfirmationAlertViewController : ConfirmationAlertViewController
@property(nonatomic, assign) BOOL isBeingDismissedOverride;
@end

@implementation TestConfirmationAlertViewController
- (BOOL)isBeingDismissed {
  return self.isBeingDismissedOverride || [super isBeingDismissed];
}
@end

// Test fixture for ConfirmationAlertViewController.
class ConfirmationAlertViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    alert_vc_ = [[TestConfirmationAlertViewController alloc] init];
    handler_ = [[ConfirmationAlertActionTestHandler alloc] init];
    alert_vc_.actionHandler = handler_;
  }

  TestConfirmationAlertViewController* alert_vc_;
  ConfirmationAlertActionTestHandler* handler_;
};

// Tests that ConfirmationAlertViewController forwards dismissal to
// actionHandler.
TEST_F(ConfirmationAlertViewControllerTest, TestConfirmationAlertDismissal) {
  EXPECT_FALSE(handler_.dismissed);
  alert_vc_.isBeingDismissedOverride = YES;
  [alert_vc_ viewDidDisappear:NO];
  EXPECT_TRUE(handler_.dismissed);
}
