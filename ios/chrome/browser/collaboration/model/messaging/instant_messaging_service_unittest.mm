// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service.h"

#import "base/test/mock_callback.h"
#import "base/test/scoped_feature_list.h"
#import "components/collaboration/public/features.h"
#import "components/data_sharing/public/features.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/saved_tab_groups/test_support/mock_versioning_message_controller.h"
#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace collaboration::messaging {

namespace {

using tab_groups::MockTabGroupSyncService;
using ::testing::Return;

// Returns an `InstantMessage`.
InstantMessage CreateInstantMessage(InstantNotificationLevel level) {
  InstantMessage message;
  message.level = level;
  message.type = InstantNotificationType::UNDEFINED;
  message.collaboration_event = CollaborationEvent::TAB_GROUP_REMOVED;
  return message;
}

// Creates a MockTabGroupSyncService with a VersioningMessageController.
std::unique_ptr<KeyedService> CreateMockTabGroupSyncService(
    tab_groups::VersioningMessageController* versioning_message_controller,
    ProfileIOS* profile) {
  auto tab_group_sync_service =
      std::make_unique<::testing::NiceMock<MockTabGroupSyncService>>();
  ON_CALL(*tab_group_sync_service.get(), GetVersioningMessageController())
      .WillByDefault(Return(versioning_message_controller));
  return tab_group_sync_service;
}

class InstantMessagingServiceTest : public PlatformTest {
 public:
  InstantMessagingServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            data_sharing::features::kDataSharingFeature,
            collaboration::features::kCollaborationMessaging,
        },
        /*disable_features=*/{});

    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindOnce(&CreateMockTabGroupSyncService,
                       &versioning_message_controller_));
    // The InstantMessagingService should call the VersioningMessageController
    // at initialization.
    EXPECT_CALL(versioning_message_controller_, ShouldShowMessageUiAsync)
        .Times(1);
    profile_ = std::move(test_profile_builder).Build();
    service_ = InstantMessagingServiceFactory::GetForProfile(profile_.get());
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    application_handler_ = OCMProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());

    ConfigureWebStateList();
    ConfigureInfobarManager();
  }

  // Configures the `web_state_list_`.
  void ConfigureWebStateList() {
    web_state_list_ = browser_->GetWebStateList();
    WebStateListBuilderFromDescription builder(web_state_list_);
    ASSERT_TRUE(
        builder.BuildWebStateListFromDescription("| [0 a*]", profile_.get()));
    active_web_state_ =
        static_cast<web::FakeWebState*>(web_state_list_->GetActiveWebState());
  }

  // Configures the InfoBarManager.
  void ConfigureInfobarManager() {
    active_web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(active_web_state_);
  }

  ~InstantMessagingServiceTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  tab_groups::MockVersioningMessageController versioning_message_controller_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<InstantMessagingService> service_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<web::FakeWebState> active_web_state_;
  id<ApplicationCommands> application_handler_;
};

// Tests the DisplayInstantaneousMessage method for an undefined level message.
TEST_F(InstantMessagingServiceTest, DisplayInstantaneousUndefinedMessage) {
  InstantMessage message =
      CreateInstantMessage(InstantNotificationLevel::UNDEFINED);
  base::MockCallback<
      MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(false));
  service_->DisplayInstantaneousMessage(message, mock_callback.Get());
}

// Tests the DisplayInstantaneousMessage method for a browser level message.
TEST_F(InstantMessagingServiceTest, DisplayInstantaneousBrowserMessage) {
  InstantMessage message =
      CreateInstantMessage(InstantNotificationLevel::BROWSER);
  base::MockCallback<
      MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(true));
  service_->DisplayInstantaneousMessage(message, mock_callback.Get());
}

// Tests the DisplayInstantaneousMessage method for a system level message.
TEST_F(InstantMessagingServiceTest, DisplayInstantaneousSystemMessage) {
  InstantMessage message =
      CreateInstantMessage(InstantNotificationLevel::SYSTEM);
  base::MockCallback<
      MessagingBackendService::InstantMessageDelegate::SuccessCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(false));
  service_->DisplayInstantaneousMessage(message, mock_callback.Get());
}

// Tests the DisplayOutOfDateMessageIfNeeded method when the message is needed.
TEST_F(InstantMessagingServiceTest, DisplayOutOfDateMessage_Needed) {
  EXPECT_CALL(versioning_message_controller_, OnMessageUiShown).Times(1);
  service_->DisplayOutOfDateMessageIfNeeded(true);
}

// Tests the DisplayOutOfDateMessageIfNeeded method when the message is not
// needed.
TEST_F(InstantMessagingServiceTest, DisplayOutOfDateMessage_NotNeeded) {
  EXPECT_CALL(versioning_message_controller_, OnMessageUiShown).Times(0);
  service_->DisplayOutOfDateMessageIfNeeded(false);
}

}  // namespace

}  // namespace collaboration::messaging
