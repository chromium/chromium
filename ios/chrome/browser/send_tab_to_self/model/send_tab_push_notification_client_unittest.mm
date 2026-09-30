// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_push_notification_client.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

NSString* const kTestUrl = @"https://www.example.com";
constexpr char kAutoOpenOutcomeHistogram[] =
    "Sharing.SendTabToSelf.AutoOpenOutcome2";

}  // namespace

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
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  send_tab_to_self::StubSendTabToSelfSyncService>();
            }));

    ProfileIOS* profile =
        profile_manager_.AddProfileWithBuilder(std::move(builder));
    BrowserList* list = BrowserListFactory::GetForProfile(profile);
    scene_state_ = [[SceneState alloc] init];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    browser_ = std::make_unique<TestBrowser>(profile, scene_state_);
    list->AddBrowser(browser_.get());
    client_ = IsMultiProfilePushNotificationHandlingEnabled()
                  ? std::make_unique<SendTabPushNotificationClient>(profile)
                  : std::make_unique<SendTabPushNotificationClient>();
    ScopedDictPrefUpdate update(GetApplicationContext()->GetLocalState(),
                                prefs::kAppLevelPushNotificationPermissions);
    update->Set(kSendTabNotificationKey, true);
    application_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:application_handler_
                     forProtocol:@protocol(SceneCommands)];
    model_ = static_cast<send_tab_to_self::FakeSendTabToSelfModel*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(profile)
            ->GetSendTabToSelfModel());
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mock_response_);
    EXPECT_OCMOCK_VERIFY(mock_notification_);
    EXPECT_OCMOCK_VERIFY((id)application_handler_);
    PlatformTest::TearDown();
  }

  // Returns a mock UNNotificationResponse.
  id MockRequestResponse(bool is_send_tab_notification,
                         const std::string& guid = "",
                         NSString* url_string = kTestUrl) {
    mock_response_ = OCMClassMock([UNNotificationResponse class]);
    OCMStub([mock_response_ notification])
        .andReturn(
            MockNotification(is_send_tab_notification, guid, url_string));
    return mock_response_;
  }

  // Returns a mock UNNotification.
  id MockNotification(bool is_send_tab_notification,
                      const std::string& guid = "",
                      NSString* url_string = kTestUrl) {
    UNNotificationRequest* request =
        CreateRequest(is_send_tab_notification, guid, url_string);
    mock_notification_ = OCMClassMock([UNNotification class]);
    OCMStub([mock_notification_ request]).andReturn(request);
    return mock_notification_;
  }

  id CreateRequest(bool is_send_tab_notification,
                   const std::string& guid = "",
                   NSString* url_string = kTestUrl) {
    NSMutableDictionary<NSString*, id>* payload =
        [[NSMutableDictionary alloc] init];
    if (url_string) {
      [payload setObject:url_string forKey:@"url"];
    }
    if (is_send_tab_notification) {
      [payload setObject:@"6" forKey:@"push_notification_client_id"];
      if (!guid.empty()) {
        [payload setObject:base::SysUTF8ToNSString(guid) forKey:@"SendTabGuid"];
      }
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
  SceneState* scene_state_ = nil;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<SendTabPushNotificationClient> client_;
  raw_ptr<send_tab_to_self::FakeSendTabToSelfModel> model_ = nullptr;
  id<SceneCommands> application_handler_ = nil;
  id mock_response_ = nil;
  id mock_notification_ = nil;
};

// Tests that interacting with a valid send tab notification loads the URL in a
// new tab and marks the entry opened and activated in the model.
TEST_F(SendTabPushNotificationClientTest, TestNotificationInteraction) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  // Add an entry to the fake model.
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL(base::SysNSStringToUTF8(kTestUrl)), "title", "device",
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  ASSERT_NE(nullptr, entry);
  std::string guid = entry->GetGUID();

  // Set up expectation BEFORE the action.
  OCMExpect([application_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        EXPECT_EQ(GURL(base::SysNSStringToUTF8(kTestUrl)), command.URL);
        EXPECT_NSEQ(base::SysUTF8ToNSString(guid),
                    command.sendTabToSelfEntryGUID);
        // Verify that the entry was marked opened in the model BEFORE opening
        // the new tab to prevent reentrancy issues from creating duplicate
        // tabs in SendTabToSelfBrowserAgent.
        EXPECT_EQ(guid, model_->last_opened_guid());
        EXPECT_EQ(guid, model_->last_activated_guid());
        return YES;
      }]]);

  // Trigger the interaction.
  bool handle_interaction = client_->HandleNotificationInteraction(
      MockRequestResponse(/*is_send_tab_notification=*/true, guid));
  EXPECT_TRUE(handle_interaction);

  // Assert that the entry was marked opened and activated!
  EXPECT_EQ(guid, model_->last_opened_guid());
  EXPECT_EQ(guid, model_->last_activated_guid());
  EXPECT_EQ(model_->last_activated_entry_point(),
            send_tab_to_self::ShareActivatedEntryPoint::kMobileNotification);

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      send_tab_to_self::AutoOpenOutcome::kTabOpenedViaNotification, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "IOS.Notifications.SendTab.Interaction"),
            1);
}

// Tests that opening a tab via notification interaction does not cause
// SendTabToSelfBrowserAgent's WebStateListDidChange observer to reentrantly
// treat the entry as unopened and spawn a duplicate background tab or duplicate
// metrics (crbug.com/563304530).
TEST_F(SendTabPushNotificationClientTest,
       TestNotificationInteraction_DoesNotTriggerReentrantDuplicateTab) {
  base::test::ScopedFeatureList feature_list(
      send_tab_to_self::kSendTabToSelfAutoOpen);
  base::HistogramTester histogram_tester;

  model_->SetLocalCacheGuid("device");
  const send_tab_to_self::SendTabToSelfEntry* entry = model_->AddEntryRemotely(
      GURL(base::SysNSStringToUTF8(kTestUrl)), "title", "device",
      send_tab_to_self::PageContext(), send_tab_to_self::NavigationHistory());
  ASSERT_NE(nullptr, entry);
  std::string guid = entry->GetGUID();

  // Attach SendTabToSelfBrowserAgent to observe WebStateList changes.
  SendTabToSelfBrowserAgent::CreateForBrowser(browser_.get());
  ASSERT_NE(nullptr, SendTabToSelfBrowserAgent::FromBrowser(browser_.get()));

  // When openURLInNewTab is dispatched, simulate creating and inserting the
  // new active WebState into the browser's WebStateList. This fires
  // WebStateListDidChange synchronously and tests reentrancy.
  OCMExpect([application_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        EXPECT_EQ(GURL(base::SysNSStringToUTF8(kTestUrl)), command.URL);
        EXPECT_NSEQ(base::SysUTF8ToNSString(guid),
                    command.sendTabToSelfEntryGUID);
        // Verify that the entry was marked opened in the model BEFORE opening
        // the new tab to prevent reentrancy issues from creating duplicate
        // tabs in SendTabToSelfBrowserAgent.
        EXPECT_EQ(guid, model_->last_opened_guid());
        EXPECT_EQ(guid, model_->last_activated_guid());

        auto fake_web_state = std::make_unique<web::FakeWebState>();
        fake_web_state->SetCurrentURL(command.URL);
        fake_web_state->WasShown();
        browser_->GetWebStateList()->InsertWebState(
            std::move(fake_web_state),
            WebStateList::InsertionParams::Automatic().Activate(true));
        return YES;
      }]]);

  bool handle_interaction = client_->HandleNotificationInteraction(
      MockRequestResponse(/*is_send_tab_notification=*/true, guid));
  EXPECT_TRUE(handle_interaction);

  // Exactly one tab should exist in the WebStateList (the one opened by the
  // notification), and NO duplicate background tabs should have been created.
  EXPECT_EQ(1, browser_->GetWebStateList()->count());

  // Verify that only the single notification outcome was recorded, and NO
  // reentrant kTabsOpenedInBackgroundUponActivation was recorded.
  histogram_tester.ExpectUniqueSample(
      kAutoOpenOutcomeHistogram,
      send_tab_to_self::AutoOpenOutcome::kTabOpenedViaNotification, 1);
  histogram_tester.ExpectBucketCount(
      kAutoOpenOutcomeHistogram,
      send_tab_to_self::AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation,
      0);
}

// Tests that interacting with a send tab notification when the entry has not
// arrived in the model yet buffers the GUID on the model and loads the URL.
TEST_F(SendTabPushNotificationClientTest,
       TestNotificationInteraction_EntryNotInModelYet) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  std::string guid = "guid_not_in_model_yet";

  // Do NOT add the entry to `model_` prior to interaction to simulate the race
  // condition where notification arrives/is tapped before sync completes.

  // Set up expectation BEFORE the action.
  OCMExpect([application_handler_
      openURLInNewTab:[OCMArg checkWithBlock:^(OpenNewTabCommand* command) {
        EXPECT_EQ(GURL(base::SysNSStringToUTF8(kTestUrl)), command.URL);
        EXPECT_NSEQ(base::SysUTF8ToNSString(guid),
                    command.sendTabToSelfEntryGUID);
        return YES;
      }]]);

  // Trigger the interaction.
  bool handle_interaction = client_->HandleNotificationInteraction(
      MockRequestResponse(/*is_send_tab_notification=*/true, guid));
  EXPECT_TRUE(handle_interaction);

  // Assert that the entry was marked opened and activated on the model despite
  // not being in the model yet.
  EXPECT_EQ(guid, model_->last_opened_guid());
  EXPECT_EQ(guid, model_->last_activated_guid());
  EXPECT_EQ(model_->last_activated_entry_point(),
            send_tab_to_self::ShareActivatedEntryPoint::kMobileNotification);

  histogram_tester.ExpectUniqueSample(
      kAutoOpenOutcomeHistogram,
      send_tab_to_self::AutoOpenOutcome::kTabOpenedViaNotification, 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "IOS.Notifications.SendTab.Interaction"),
            1);
}

// Tests that non-send-tab notifications are rejected by the client.
TEST_F(SendTabPushNotificationClientTest,
       TestNotificationInteraction_NotSendTabNotification) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  bool handle_interaction = client_->HandleNotificationInteraction(
      MockRequestResponse(/*is_send_tab_notification=*/false));

  // Check destination URL is not loaded.
  OCMReject([application_handler_ openURLInNewTab:[OCMArg any]]);
  EXPECT_FALSE(handle_interaction);

  histogram_tester.ExpectTotalCount(kAutoOpenOutcomeHistogram, 0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "IOS.Notifications.SendTab.Interaction"),
            0);
}

// Tests that notification interactions with invalid URLs are rejected.
TEST_F(SendTabPushNotificationClientTest,
       TestNotificationInteraction_InvalidUrl) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  OCMReject([application_handler_ openURLInNewTab:[OCMArg any]]);
  bool handle_interaction =
      client_->HandleNotificationInteraction(MockRequestResponse(
          /*is_send_tab_notification=*/true, "guid123", @"not-a-valid-url"));
  EXPECT_FALSE(handle_interaction);

  histogram_tester.ExpectTotalCount(kAutoOpenOutcomeHistogram, 0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "IOS.Notifications.SendTab.Interaction"),
            0);
}

// Tests that notification interactions with empty URLs are rejected.
TEST_F(SendTabPushNotificationClientTest,
       TestNotificationInteraction_EmptyUrl) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  OCMReject([application_handler_ openURLInNewTab:[OCMArg any]]);
  bool handle_interaction =
      client_->HandleNotificationInteraction(MockRequestResponse(
          /*is_send_tab_notification=*/true, "guid123", @""));
  EXPECT_FALSE(handle_interaction);

  histogram_tester.ExpectTotalCount(kAutoOpenOutcomeHistogram, 0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "IOS.Notifications.SendTab.Interaction"),
            0);
}
