// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_view_controller_presentation_delegate.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class SharingStatusCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    browser_ = std::make_unique<TestBrowser>(
        TestChromeBrowserState::Builder().Build().get());

    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    mock_application_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationSettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_settings_commands_handler_
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    coordinator_ = [[SharingStatusCoordinator alloc]
        initWithBaseViewController:[[FakeUIViewController alloc] init]
                           browser:browser_.get()
                        recipients:nil
                           website:nil
                               URL:GURL("https://example.com")
                 changePasswordURL:GURL("https://change-password.com")];
    ASSERT_TRUE([coordinator_
        conformsToProtocol:
            @protocol(SharingStatusViewControllerPresentationDelegate)]);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> browser_;
  SharingStatusCoordinator* coordinator_;

  id mock_application_commands_handler_;
  id mock_application_settings_commands_handler_;
};

TEST_F(SharingStatusCoordinatorTest, OpensHelpCenterOnLearnMoreTap) {
  base::HistogramTester histogram_tester;

  OCMExpect([mock_application_commands_handler_
      closeSettingsUIAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                            OpenNewTabCommand* command) {
        return command.URL ==
               GURL("https://support.google.com/chrome/?p=password_sharing");
      }]]);
  [(id<SharingStatusViewControllerPresentationDelegate>)
          coordinator_ learnMoreLinkWasTapped];
  EXPECT_OCMOCK_VERIFY(mock_application_commands_handler_);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kSharingConfirmationLearnMoreClicked, 1);
}

TEST_F(SharingStatusCoordinatorTest, RedirectsToSiteOnChangePasswordURLTap) {
  base::HistogramTester histogram_tester;

  OCMExpect([mock_application_commands_handler_
      closeSettingsUIAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                            OpenNewTabCommand* command) {
        return command.URL == GURL("https://change-password.com");
      }]]);
  [(id<SharingStatusViewControllerPresentationDelegate>)
          coordinator_ changePasswordLinkWasTapped];
  EXPECT_OCMOCK_VERIFY(mock_application_commands_handler_);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kSharingConfirmationChangePasswordClicked, 1);
}

TEST_F(SharingStatusCoordinatorTest, LogsMetricsOnDoneButtonClicked) {
  base::HistogramTester histogram_tester;

  [(id<SharingStatusViewControllerPresentationDelegate>)coordinator_
      sharingStatusWasDismissed:nil];

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kSharingConfirmationDoneClicked, 1);
}
