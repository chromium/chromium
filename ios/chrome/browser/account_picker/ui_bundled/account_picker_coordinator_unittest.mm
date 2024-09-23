// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_configuration.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_coordinator.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_coordinator_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_layout_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_navigation_controller.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Unit tests for the AccountPickerCoordinator.
class AccountPickerCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    base_view_controller_ = [[FakeUIViewController alloc] init];
  }

  void TearDown() final {}

  // Create a AccountPickerCoordinator to test.
  AccountPickerCoordinator* CreateAccountPickerCoordinator(
      AccountPickerConfiguration* configuration) {
    return [[AccountPickerCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                     configuration:configuration];
  }

  AccountPickerConfiguration* CreateAccountPickerConfiguration() {
    AccountPickerConfiguration* configuration =
        [[AccountPickerConfiguration alloc] init];
    configuration.titleText = @"Account picker";
    configuration.bodyText = @"This is the body text of the account picker.";
    configuration.submitButtonTitle = @"Submit";
    configuration.askEveryTimeSwitchLabelText = @"Ask every time";
    return configuration;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
};

// Tests that the AccountPickerCoordinator creates/destroys an
// AccountPickerConfirmationScreenCoordinator with the expected configuration
// when it starts/stops. Also tests that an
// AccountPickerScreenNavigationController is created and presented/dismissed
// and destroyed when the coordinator starts/stops.
TEST_F(AccountPickerCoordinatorTest, PushesAndPopsConfirmationScreen) {
  AccountPickerConfiguration* configuration =
      CreateAccountPickerConfiguration();
  AccountPickerCoordinator* coordinator =
      CreateAccountPickerCoordinator(configuration);

  id mock_confirmation_screen_coordinator =
      OCMClassMock([AccountPickerConfirmationScreenCoordinator class]);
  OCMExpect([mock_confirmation_screen_coordinator alloc])
      .andReturn(mock_confirmation_screen_coordinator);
  OCMExpect([mock_confirmation_screen_coordinator
                initWithBaseViewController:OCMOCK_ANY
                                   browser:browser_.get()
                             configuration:configuration])
      .andReturn(mock_confirmation_screen_coordinator);
  ASSERT_TRUE([coordinator
      conformsToProtocol:
          @protocol(AccountPickerConfirmationScreenCoordinatorDelegate)]);
  OCMExpect([mock_confirmation_screen_coordinator
      setDelegate:static_cast<
                      id<AccountPickerConfirmationScreenCoordinatorDelegate>>(
                      coordinator)]);
  ASSERT_TRUE(
      [coordinator conformsToProtocol:@protocol(AccountPickerLayoutDelegate)]);
  OCMExpect([mock_confirmation_screen_coordinator
      setLayoutDelegate:static_cast<id<AccountPickerLayoutDelegate>>(
                            coordinator)]);
  OCMExpect([base::apple::ObjCCast<AccountPickerConfirmationScreenCoordinator>(
      mock_confirmation_screen_coordinator) start]);
  id mock_confirmation_screen_view_controller =
      OCMProtocolMock(@protocol(AccountPickerScreenViewController));
  OCMStub([mock_confirmation_screen_coordinator viewController])
      .andReturn(mock_confirmation_screen_view_controller);

  // Expect that the AccountPickerScreenNavigationController is created and
  // presented.
  id mock_navigation_controller =
      OCMClassMock([AccountPickerScreenNavigationController class]);
  OCMExpect([mock_navigation_controller alloc])
      .andReturn(mock_navigation_controller);
  OCMExpect(
      [mock_navigation_controller
          initWithRootViewController:mock_confirmation_screen_view_controller])
      .andReturn(mock_navigation_controller);
  ASSERT_TRUE([coordinator
      conformsToProtocol:@protocol(UINavigationControllerDelegate)]);
  OCMExpect([mock_navigation_controller
      setDelegate:static_cast<id<UINavigationControllerDelegate>>(
                      coordinator)]);
  OCMExpect([mock_navigation_controller
      setModalPresentationStyle:UIModalPresentationCustom]);
  ASSERT_TRUE([coordinator
      conformsToProtocol:@protocol(UIViewControllerTransitioningDelegate)]);
  OCMExpect([mock_navigation_controller
      setTransitioningDelegate:static_cast<
                                   id<UIViewControllerTransitioningDelegate>>(
                                   coordinator)]);

  // Verify that the confirmation screen has been created and started and that
  // the account picker navigation controller has been created and presented.
  [coordinator start];
  EXPECT_OCMOCK_VERIFY(mock_confirmation_screen_coordinator);
  EXPECT_EQ(base_view_controller_.presentedViewController,
            mock_navigation_controller);

  OCMStub([mock_navigation_controller presentingViewController])
      .andReturn(base_view_controller_);

  // Expect the AccountPickerConfirmationScreenCoordinator is stopped when the
  // account picker is stopped.
  OCMExpect([base::apple::ObjCCast<AccountPickerConfirmationScreenCoordinator>(
      mock_confirmation_screen_coordinator) stop]);
  [coordinator stop];

  // Check that the account picker navigation controller is dismissed and verify
  // that the confirmation screen coordinator has been stopped.
  EXPECT_EQ(base_view_controller_.presentedViewController, nil);
  EXPECT_OCMOCK_VERIFY(mock_confirmation_screen_coordinator);
}
