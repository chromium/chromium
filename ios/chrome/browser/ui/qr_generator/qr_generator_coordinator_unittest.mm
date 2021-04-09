// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_generator/qr_generator_coordinator.h"

#import "base/mac/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/qr_generator/qr_generator_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "net/base/mac/url_conversions.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class QRGeneratorCoordinatorTest : public PlatformTest {
 protected:
  QRGeneratorCoordinatorTest()
      : test_url_("https://www.google.com/"),
        browser_(std::make_unique<TestBrowser>()),
        scene_state_([[SceneState alloc] initWithAppState:nil]) {
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
  }

  void SetUp() override {
    mock_qr_generation_commands_handler_ =
        OCMStrictProtocolMock(@protocol(QRGenerationCommands));

    test_title_ = @"Does not matter";

    coordinator_ = [[QRGeneratorCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                             title:test_title_
                               URL:test_url_
                           handler:(id<QRGenerationCommands>)
                                       mock_qr_generation_commands_handler_];
  }

  NSString* test_title_;
  GURL test_url_;

  base::test::TaskEnvironment task_environment_;
  id mock_qr_generation_commands_handler_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  SceneState* scene_state_;

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
      base::mac::ObjCCastStrict<QRGeneratorViewController>(
          base_view_controller_.presentedViewController);

  // Verify some properties on the VC.
  EXPECT_TRUE(viewController.helpButtonAvailable);
  EXPECT_EQ(test_title_, viewController.titleString);
  EXPECT_TRUE([net::NSURLWithGURL(test_url_) isEqual:viewController.pageURL]);

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
      base::mac::ObjCCastStrict<QRGeneratorViewController>(
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
      base::mac::ObjCCastStrict<QRGeneratorViewController>(
          base_view_controller_.presentedViewController);

  __block PopoverLabelViewController* popoverViewController;
  id vcPartialMock = OCMPartialMock(viewController);
  [[vcPartialMock expect]
      presentViewController:[OCMArg checkWithBlock:^BOOL(
                                        UIViewController* givenVC) {
        if ([givenVC isKindOfClass:[PopoverLabelViewController class]]) {
          popoverViewController =
              base::mac::ObjCCastStrict<PopoverLabelViewController>(givenVC);
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
