// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

NSArray<RecipientInfoForIOSDisplay*>* CreateRecipients(int amount) {
  NSMutableArray<RecipientInfoForIOSDisplay*>* recipients =
      [NSMutableArray array];
  for (int i = 0; i < amount; i++) {
    password_manager::RecipientInfo recipient;
    [recipients addObject:([[RecipientInfoForIOSDisplay alloc]
                              initWithRecipientInfo:recipient])];
  }
  return recipients;
}

}  // namespace

class FamilyPickerCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    browser_ =
        std::make_unique<TestBrowser>(TestProfileIOS::Builder().Build().get());
    view_controller_ = [[FamilyPickerViewController alloc]
        initWithStyle:UITableViewStylePlain];

    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    mock_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_commands_handler_
                     forProtocol:@protocol(SettingsCommands)];

    coordinator_ = [[FamilyPickerCoordinator alloc]
        initWithBaseNavigationController:[[FakeUINavigationController alloc]
                                             init]
                                 browser:browser_.get()
                              recipients:nil];
    ASSERT_TRUE([coordinator_
        conformsToProtocol:
            @protocol(FamilyPickerViewControllerPresentationDelegate)]);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> browser_;
  FamilyPickerCoordinator* coordinator_;
  FamilyPickerViewController* view_controller_;

  id mock_application_commands_handler_;
  id mock_settings_commands_handler_;
};

TEST_F(FamilyPickerCoordinatorTest, LogsMetricsOnOneRecipientSelected) {
  base::HistogramTester histogram_tester;

  [(id<FamilyPickerViewControllerPresentationDelegate>)coordinator_
          familyPickerClosed:view_controller_
      withSelectedRecipients:CreateRecipients(1)];

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kFamilyPickerShareWithOneMember, 1);
}

TEST_F(FamilyPickerCoordinatorTest, LogsMetricsOnMultipleRecipientsSelected) {
  base::HistogramTester histogram_tester;

  [(id<FamilyPickerViewControllerPresentationDelegate>)coordinator_
          familyPickerClosed:view_controller_
      withSelectedRecipients:CreateRecipients(3)];

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kFamilyPickerShareWithMultipleMembers, 1);
}

TEST_F(FamilyPickerCoordinatorTest, OpensHelpCenterOnLearnMoreTap) {
  base::HistogramTester histogram_tester;

  OCMExpect([mock_application_commands_handler_
      closeSettingsUIAndOpenURL:[OCMArg checkWithBlock:^BOOL(
                                            OpenNewTabCommand* command) {
        return command.URL ==
               GURL("https://support.google.com/chrome/?p=password_sharing");
      }]]);
  [(id<FamilyPickerViewControllerPresentationDelegate>)
          coordinator_ learnMoreLinkWasTapped];
  EXPECT_OCMOCK_VERIFY(mock_application_commands_handler_);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::
          kFamilyPickerIneligibleRecipientLearnMoreClicked,
      1);
}
