// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_push_notification_client.h"

#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for SendTabPushNotificationClient.
class SendTabPushNotificationClientTest : public PlatformTest {
 public:
  SendTabPushNotificationClientTest() = default;
  ~SendTabPushNotificationClientTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        SendTabToSelfSyncServiceFactory::GetDefaultFactory());

    ProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(std::move(builder));
    BrowserList* list = BrowserListFactory::GetForProfile(profile);
    mock_scene_state_ = OCMClassMock([SceneState class]);
    OCMStub([mock_scene_state_ activationLevel])
        .andReturn(SceneActivationLevelForegroundActive);
    browser_ = std::make_unique<TestBrowser>(profile, mock_scene_state_);
    list->AddBrowser(browser_.get());
    client_ = std::make_unique<SendTabPushNotificationClient>();
    ScopedDictPrefUpdate update(GetApplicationContext()->GetLocalState(),
                                prefs::kAppLevelPushNotificationPermissions);
    update->Set(kSendTabNotificationKey, true);
    application_handler_ = OCMProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_handler_
                     forProtocol:@protocol(ApplicationCommands)];
  }

  // Returns a mock UNNotificationResponse.
  id MockRequestResponse(bool is_send_tab_notification) {
    id mock_response = OCMClassMock([UNNotificationResponse class]);
    OCMStub([mock_response notification])
        .andReturn(MockNotification(is_send_tab_notification));
    return mock_response;
  }

  // Returns a mock UNNotification.
  id MockNotification(bool is_send_tab_notification) {
    UNNotificationRequest* request = CreateRequest(is_send_tab_notification);
    id mock_notification = OCMClassMock([UNNotification class]);
    OCMStub([mock_notification request]).andReturn(request);
    return mock_notification;
  }

  id CreateRequest(bool is_send_tab_notification) {
    NSMutableDictionary<NSString*, id>* payload =
        [[NSMutableDictionary alloc] init];
    [payload setObject:@"https://www.example.com" forKey:@"url"];
    if (is_send_tab_notification) {
      [payload setObject:@"6" forKey:@"push_notification_client_id"];
    }

    UNMutableNotificationContent* content =
        [[UNMutableNotificationContent alloc] init];
    content.title = @"Mock Title";
    content.body = @"Mock Body";
    content.userInfo = payload;

    return [UNNotificationRequest requestWithIdentifier:@""
                                                content:content
                                                trigger:nil];
  }

 protected:
  web::WebTaskEnvironment web_task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  id mock_scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<SendTabPushNotificationClient> client_;
  id<ApplicationCommands> application_handler_;
  id mock_notification_center_;
  id mock_application_handler_;
};

TEST_F(SendTabPushNotificationClientTest, TestNotificationInteraction) {
  bool handle_interaction = client_->HandleNotificationInteraction(
      MockRequestResponse(/*is_send_tab_notification=*/true));

  // Check destination URL loaded.
  OCMExpect([application_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        return command.URL == "https://www.example.com/";
      }]]);
  EXPECT_TRUE(handle_interaction);
}

TEST_F(SendTabPushNotificationClientTest,
       TestNotificationInteraction_NotSendTabNotification) {
  bool handle_interaction = client_->HandleNotificationInteraction(
      MockRequestResponse(/*is_send_tab_notification=*/false));

  // Check destination URL is not loaded.
  OCMReject([application_handler_ openURLInNewTab:[OCMArg any]]);
  EXPECT_FALSE(handle_interaction);
}
