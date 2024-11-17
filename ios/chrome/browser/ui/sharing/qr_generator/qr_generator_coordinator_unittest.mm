// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/qr_generator/qr_generator_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/sharing/qr_generator/qr_generator_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "net/base/apple/url_conversions.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/gurl.h"

class QRGeneratorCoordinatorTest : public PlatformTest {
 protected:
  QRGeneratorCoordinatorTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
  }

  void SetUp() override {
    mock_qr_generation_commands_handler_ =
        OCMStrictProtocolMock(@protocol(QRGenerationCommands));

    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:OCMStrictProtocolMock(
                                     @protocol(BookmarksCommands))
                     forProtocol:@protocol(BookmarksCommands)];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:OCMStrictProtocolMock(@protocol(HelpCommands))
                     forProtocol:@protocol(HelpCommands)];

    coordinator_ = [[QRGeneratorCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                             title:@"Does not matter"
                               URL:GURL("https://www.google.com/")
                           handler:(id<QRGenerationCommands>)
                                       mock_qr_generation_commands_handler_];
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id mock_qr_generation_commands_handler_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  QRGeneratorCoordinator* coordinator_;
};

// Tests that a Done button gets added to the navigation bar, and its action
// dispatches the right command.
TEST_F(QRGeneratorCoordinatorTest, Done_DispatchesCommand) {
  // Set-up mocked handler.
  [[mock_qr_generation_commands_handler_ expect] hideQRCode];

  // Check and start coordinator.
  ASSERT_EQ(base_view_controller_, coordinator_.baseViewController);
  ASSERT_FALSE(base_view_controller_.presentedViewController);

  [coordinator_ start];

  ASSERT_TRUE(base_view_controller_.presentedViewController);
  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[QRGeneratorViewController class]]);

  QRGeneratorViewController* viewController =
      base::apple::ObjCCastStrict<QRGeneratorViewController>(
          base_view_controller_.presentedViewController);

  // Mimick click on done button.
  [viewController.actionHandler confirmationAlertDismissAction];

  // Callback should've gotten invoked.
  [mock_qr_generation_commands_handler_ verify];

  // Set-up mocks for "stop" function.
  id baseViewControllerMock = OCMPartialMock(base_view_controller_);
  [[baseViewControllerMock expect] dismissViewControllerAnimated:YES
                                                      completion:nil];

  [coordinator_ stop];

  // View controller dismissal must have gone through the root view controller.
  [baseViewControllerMock verify];
}

// Tests that tje primary action, share, intializes the activity service
// coordinator properly.
TEST_F(QRGeneratorCoordinatorTest, ShareAction) {
  [coordinator_ start];

  QRGeneratorViewController* viewController =
      base::apple::ObjCCastStrict<QRGeneratorViewController>(
          base_view_controller_.presentedViewController);

  id vcPartialMock = OCMPartialMock(viewController);
  [[vcPartialMock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* givenVC) {
        return [givenVC isKindOfClass:[UIActivityViewController class]];
      }]
                   animated:YES
                 completion:nil];

  // Mimic tap on share button.
  [viewController.actionHandler confirmationAlertPrimaryAction];

  [vcPartialMock verify];
}

// Tests that a popover is properly created and shown when the user taps on
// the learn more button.
TEST_F(QRGeneratorCoordinatorTest, LearnMore) {
  [coordinator_ start];

  QRGeneratorViewController* viewController =
      base::apple::ObjCCastStrict<QRGeneratorViewController>(
          base_view_controller_.presentedViewController);

  __block PopoverLabelViewController* popoverViewController;
  id vcPartialMock = OCMPartialMock(viewController);
  [[vcPartialMock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* givenVC) {
        if ([givenVC isKindOfClass:[PopoverLabelViewController class]]) {
          popoverViewController =
              base::apple::ObjCCastStrict<PopoverLabelViewController>(givenVC);
          return YES;
        }
        return NO;
      }]
                   animated:YES
                 completion:nil];

  // Mimic tap on help button.
  [viewController.actionHandler confirmationAlertLearnMoreAction];

  [vcPartialMock verify];
  EXPECT_TRUE(popoverViewController);

  EXPECT_EQ(viewController.helpButton,
            popoverViewController.popoverPresentationController.barButtonItem);
  EXPECT_EQ(UIPopoverArrowDirectionUp,
            popoverViewController.popoverPresentationController
                .permittedArrowDirections);
}
