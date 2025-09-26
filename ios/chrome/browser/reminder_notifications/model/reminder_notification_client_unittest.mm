// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_client.h"

#import <UserNotifications/UserNotifications.h>

#import "base/apple/foundation_util.h"
#import "base/json/values_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_mediator.h"
#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_builder.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Tests the `ReminderNotificationClient` and associated objects.
class ReminderNotificationClientTest : public PlatformTest {
 protected:
  ReminderNotificationClientTest() {
    feature_list_.InitAndEnableFeature(kSeparateProfilesForManagedAccounts);

    TestProfileIOS::Builder builder;
    builder.SetPrefService(CreatePrefService());
    profile_ = std::move(builder).Build();

    mock_scene_state_ = OCMClassMock([SceneState class]);
    OCMStub([mock_scene_state_ activationLevel])
        .andReturn(SceneActivationLevelForegroundActive);
    browser_ = std::make_unique<TestBrowser>(profile_.get(), mock_scene_state_);
    BrowserListFactory::GetForProfile(profile_.get())
        ->AddBrowser(browser_.get());

    client_ = std::make_unique<ReminderNotificationClient>(profile_.get());
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterProfilePrefs(registry.get());
    return prefs;
  }

  // Creates a mock command handler for a given protocol and starts dispatching.
  id MockHandler(Protocol* protocol) {
    id mock_handler = OCMProtocolMock(protocol);
    [browser_->GetCommandDispatcher() startDispatchingToTarget:mock_handler
                                                   forProtocol:protocol];
    return mock_handler;
  }

  // Sets up a mock notification center, so notification requests can be
  // tested.
  void SetupMockNotificationCenter() {
    mock_notification_center_ = OCMClassMock([UNUserNotificationCenter class]);
    // Swizzle in the mock notification center.
    UNUserNotificationCenter* (^swizzle_block)() =
        ^UNUserNotificationCenter*() {
          return mock_notification_center_;
        };
    notification_center_swizzler_ = std::make_unique<ScopedBlockSwizzler>(
        [UNUserNotificationCenter class], @selector(currentNotificationCenter),
        swizzle_block);
  }

  // Stubs getPendingNotificationRequestsWithCompletionHandler.
  void StubGetPendingRequests(NSArray<UNNotificationRequest*>* requests) {
    auto completionCaller =
        ^BOOL(void (^completion)(NSArray<UNNotificationRequest*>* requests)) {
          completion(requests);
          return YES;
        };
    OCMStub([mock_notification_center_
        getPendingNotificationRequestsWithCompletionHandler:
            [OCMArg checkWithBlock:completionCaller]]);
  }

  // Sets up an expectation for addNotificationRequest, checking the URL.
  void ExpectAddNotificationRequest(const GURL& expected_url) {
    id request_arg_matcher =
        [OCMArg checkWithBlock:^BOOL(UNNotificationRequest* req) {
          EXPECT_TRUE([req.identifier
              hasPrefix:kReminderNotificationsIdentifierPrefix]);
          EXPECT_NSEQ(req.content.userInfo[@"url"],
                      [net::NSURLWithGURL(expected_url) absoluteString]);
          return YES;
        }];
    ExpectAddNotificationRequestWithMatcher(request_arg_matcher);
  }

  // Sets up an expectation for addNotificationRequest with a custom matcher.
  void ExpectAddNotificationRequestWithMatcher(id request_arg_matcher) {
    auto completionCaller = ^BOOL(void (^completion)(NSError* error)) {
      completion(nil);  // Simulate success
      return YES;
    };
    OCMExpect([mock_notification_center_
        addNotificationRequest:request_arg_matcher
         withCompletionHandler:[OCMArg checkWithBlock:completionCaller]]);
  }

  // Helper to set reminder prefs.
  void SetReminderPrefs(const base::Value::Dict& reminders) {
    profile_->GetPrefs()->SetDict(prefs::kReminderNotifications,
                                  reminders.Clone());
  }

  // Helper to set the reminder permission.
  void SetReminderPermission(bool enabled) {
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                prefs::kFeaturePushNotificationPermissions);
    update->Set(kReminderNotificationKey, enabled);
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  IOSChromeScopedTestingLocalState local_state_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<ReminderNotificationClient> client_;
  id mock_notification_center_;
  std::unique_ptr<ScopedBlockSwizzler> notification_center_swizzler_;
  id mock_scene_state_;
  std::unique_ptr<TestBrowser> browser_;
};

#pragma mark - Test cases

// Test the ReminderNotificationBuilder that the client uses to build
// local notification content.
TEST_F(ReminderNotificationClientTest, Builder) {
  GURL url("http://example.org");
  base::Time target_time = base::Time::Now() + base::Hours(1);

  ReminderNotificationBuilder* builder =
      [[ReminderNotificationBuilder alloc] initWithURL:url time:target_time];
  [builder setPageTitle:@"Example Page Title"];

  ScheduledNotificationRequest request = [builder buildRequest];

  EXPECT_TRUE(
      [request.identifier hasPrefix:kReminderNotificationsIdentifierPrefix]);
  EXPECT_NE(request.content, nil);
  EXPECT_NSEQ(request.content.userInfo[@"url"],
              [net::NSURLWithGURL(url) absoluteString]);
  EXPECT_NEAR(request.time_interval.InSecondsF(), base::Hours(1).InSecondsF(),
              1.0);
}

// Test scheduling a single notification from prefs.
TEST_F(ReminderNotificationClientTest, OneReminderInPrefs) {
  SetupMockNotificationCenter();
  SetReminderPermission(true);
  GURL url("http://example.com/page1");
  base::Time reminder_time = base::Time::Now() + base::Minutes(10);
  base::Value::Dict reminder_details;
  reminder_details.Set(kReminderNotificationsTimeKey,
                       base::TimeToValue(reminder_time));
  base::Value::Dict reminders;
  reminders.Set(url.spec(), std::move(reminder_details));

  StubGetPendingRequests(nil);
  ExpectAddNotificationRequest(url);
  SetReminderPrefs(reminders);
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

// Test scheduling multiple notifications from prefs.
TEST_F(ReminderNotificationClientTest, DISABLED_MultipleRemindersInPrefs) {
  SetupMockNotificationCenter();
  SetReminderPermission(true);
  GURL url1("http://example.com/page1");
  GURL url2("http://example.org/anotherpage");
  base::Time reminder_time1 = base::Time::Now() + base::Minutes(10);
  base::Time reminder_time2 = base::Time::Now() + base::Minutes(20);

  base::Value::Dict details1;
  details1.Set(kReminderNotificationsTimeKey,
               base::TimeToValue(reminder_time1));
  base::Value::Dict details2;
  details2.Set(kReminderNotificationsTimeKey,
               base::TimeToValue(reminder_time2));

  base::Value::Dict reminders;
  reminders.Set(url1.spec(), std::move(details1));
  reminders.Set(url2.spec(), std::move(details2));

  StubGetPendingRequests(nil);
  ExpectAddNotificationRequest(url1);
  ExpectAddNotificationRequest(url2);
  SetReminderPrefs(reminders);
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

// Test that no notification is scheduled when not permitted.
TEST_F(ReminderNotificationClientTest, NoScheduleWhenNotPermitted) {
  SetupMockNotificationCenter();
  SetReminderPermission(false);
  GURL url("http://example.com/page1");
  base::Time reminder_time = base::Time::Now() + base::Minutes(10);
  base::Value::Dict reminder_details;
  reminder_details.Set(kReminderNotificationsTimeKey,
                       base::TimeToValue(reminder_time));
  base::Value::Dict reminders;
  reminders.Set(url.spec(), std::move(reminder_details));

  StubGetPendingRequests(nil);
  OCMReject([mock_notification_center_ addNotificationRequest:[OCMArg any]
                                        withCompletionHandler:[OCMArg any]]);
  SetReminderPrefs(reminders);
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

// Tests that handling a notification interaction with a valid URL eventually
// dispatches a command to open that URL in a new tab.
TEST_F(ReminderNotificationClientTest, HandleInteractionOpensUrlInNewTab) {
  GURL url_to_open("https://www.chromium.org/reminders");

  id mock_application_handler = MockHandler(@protocol(ApplicationCommands));

  OCMExpect([mock_application_handler
      openURLInNewTab:[OCMArg checkWithBlock:^BOOL(OpenNewTabCommand* command) {
        EXPECT_EQ(command.URL, url_to_open);
        EXPECT_EQ(command.inIncognito, NO);
        EXPECT_EQ(command.inBackground, NO);
        return YES;
      }]]);

  id mock_response = OCMClassMock([UNNotificationResponse class]);
  id mock_notification = OCMClassMock([UNNotification class]);
  id mock_request = OCMClassMock([UNNotificationRequest class]);
  id mock_content = OCMClassMock([UNNotificationContent class]);

  OCMStub([mock_response notification]).andReturn(mock_notification);
  OCMStub([mock_notification request]).andReturn(mock_request);
  OCMStub([mock_request content]).andReturn(mock_content);
  OCMStub([mock_request identifier])
      .andReturn(kReminderNotificationsIdentifierPrefix);
  OCMStub([mock_content userInfo]).andReturn(@{
    @"url" : base::SysUTF8ToNSString(url_to_open.spec())
  });

  ASSERT_TRUE(client_->HandleNotificationInteraction(mock_response));

  client_->OnSceneActiveForegroundBrowserReady();

  task_environment_.RunUntilIdle();

  EXPECT_OCMOCK_VERIFY(mock_application_handler);
}

// Test scheduling a single notification from Prefs and verifies Prefs removal.
TEST_F(ReminderNotificationClientTest, OneReminderInPrefsHasPrefRemoved) {
  SetupMockNotificationCenter();
  SetReminderPermission(true);
  GURL url("http://example.com/page1");
  base::Time reminder_time = base::Time::Now() + base::Minutes(10);
  base::Value::Dict reminder_details;
  reminder_details.Set(kReminderNotificationsTimeKey,
                       base::TimeToValue(reminder_time));
  base::Value::Dict reminders;
  reminders.Set(url.spec(), std::move(reminder_details));

  StubGetPendingRequests(nil);
  ExpectAddNotificationRequest(url);

  // Set the prefs, which will trigger the scheduling logic.
  SetReminderPrefs(reminders);

  // Verify the pref is initially set.
  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kReminderNotifications)
                  .contains(url.spec()));

  // Run pending tasks to allow the notification scheduling and its
  // completion handler (which removes the pref) to execute.
  task_environment_.RunUntilIdle();

  // Verify the mock and that the pref has been removed.
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  const base::Value::Dict& final_reminders =
      profile_->GetPrefs()->GetDict(prefs::kReminderNotifications);
  EXPECT_FALSE(final_reminders.contains(url.spec()));
  EXPECT_TRUE(final_reminders.empty());
}

// Test scheduling multiple notifications from Prefs and verifies Prefs removal.
TEST_F(ReminderNotificationClientTest,
       DISABLED_MultipleRemindersInPrefsHavePrefsRemoved) {
  SetupMockNotificationCenter();
  SetReminderPermission(true);
  GURL url1("http://example.com/page1");
  GURL url2("http://example.org/anotherpage");
  base::Time reminder_time1 = base::Time::Now() + base::Minutes(10);
  base::Time reminder_time2 = base::Time::Now() + base::Minutes(20);

  base::Value::Dict details1;
  details1.Set(kReminderNotificationsTimeKey,
               base::TimeToValue(reminder_time1));
  base::Value::Dict details2;
  details2.Set(kReminderNotificationsTimeKey,
               base::TimeToValue(reminder_time2));

  base::Value::Dict reminders;
  reminders.Set(url1.spec(), std::move(details1));
  reminders.Set(url2.spec(), std::move(details2));

  StubGetPendingRequests(nil);
  ExpectAddNotificationRequest(url1);
  ExpectAddNotificationRequest(url2);

  // Set the prefs, triggering scheduling.
  SetReminderPrefs(reminders);

  // Verify prefs are initially set.
  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kReminderNotifications)
                  .contains(url1.spec()));
  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kReminderNotifications)
                  .contains(url2.spec()));

  // Run pending tasks.
  task_environment_.RunUntilIdle();

  // Verify mocks and pref removal.
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  const base::Value::Dict& final_reminders =
      profile_->GetPrefs()->GetDict(prefs::kReminderNotifications);
  EXPECT_FALSE(final_reminders.contains(url1.spec()));
  EXPECT_FALSE(final_reminders.contains(url2.spec()));
  EXPECT_TRUE(final_reminders.empty());
}

// Tests that the client doesn't reschedule a reminder that is already pending
// in the notification center.
TEST_F(ReminderNotificationClientTest, DoesNotReschedulePendingReminder) {
  SetupMockNotificationCenter();
  SetReminderPermission(true);
  GURL url("http://example.com/page1");
  base::Time reminder_time = base::Time::Now() + base::Minutes(10);

  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.userInfo = @{@"url" : [net::NSURLWithGURL(url) absoluteString]};
  UNNotificationRequest* pending_request =
      [UNNotificationRequest requestWithIdentifier:@"reminder_existing_123"
                                           content:content
                                           trigger:nil];
  StubGetPendingRequests(@[ pending_request ]);

  base::Value::Dict reminder_details;
  reminder_details.Set(kReminderNotificationsTimeKey,
                       base::TimeToValue(reminder_time));
  base::Value::Dict reminders;
  reminders.Set(url.spec(), std::move(reminder_details));
  SetReminderPrefs(reminders);

  OCMReject([mock_notification_center_ addNotificationRequest:[OCMArg any]
                                        withCompletionHandler:[OCMArg any]]);
  task_environment_.RunUntilIdle();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(prefs::kReminderNotifications)
                  .contains(url.spec()));
}
