// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

#import <UserNotifications/UserNotifications.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/threading/thread_restrictions.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/default_browser/model/promo_source.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using startup_metric_utils::FirstRunSentinelCreationResult;

// A simple class that stubs `PrepareToPresentModal:` by immediately calling
// the provided `completion` callback.
@interface PrepareToPresentModalStub : NSObject
@end

@implementation PrepareToPresentModalStub
- (void)prepareToPresentModal:(ProceduralBlock)completion {
  completion();
}
@end

class TipsNotificationClientTest : public PlatformTest {
 protected:
  TipsNotificationClientTest() {
    SetupMockNotificationCenter();
    browser_state_manager_ = std::make_unique<TestChromeBrowserStateManager>(
        TestChromeBrowserState::Builder().Build());
    TestingApplicationContext::GetGlobal()->SetChromeBrowserStateManager(
        browser_state_manager_.get());
    BrowserList* list = BrowserListFactory::GetForBrowserState(
        browser_state_manager_->GetLastUsedBrowserStateForTesting());
    mock_scene_state_ = OCMClassMock([SceneState class]);
    OCMStub([mock_scene_state_ activationLevel])
        .andReturn(SceneActivationLevelForegroundActive);
    browser_ = std::make_unique<TestBrowser>(
        browser_state_manager_->GetLastUsedBrowserStateForTesting(),
        mock_scene_state_);
    list->AddBrowser(browser_.get());
    client_ = std::make_unique<TipsNotificationClient>();
    ScopedDictPrefUpdate update(GetApplicationContext()->GetLocalState(),
                                prefs::kAppLevelPushNotificationPermissions);
    update->Set(kTipsNotificationKey, true);
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

  // Writes the first run sentinel file, to allow notifications to be
  // registered.
  void WriteFirstRunSentinel() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    FirstRun::RemoveSentinel();
    base::File::Error file_error = base::File::FILE_OK;
    FirstRunSentinelCreationResult sentinel_created =
        FirstRun::CreateSentinel(&file_error);
    ASSERT_EQ(sentinel_created, FirstRunSentinelCreationResult::kSuccess)
        << "Error creating FirstRun sentinel: "
        << base::File::ErrorToString(file_error);
    FirstRun::LoadSentinelInfo();
    FirstRun::ClearStateForTesting();
  }

  // Returns an OCMArg that verifies a UNNotificationRequest was passed for the
  // given notification `type`.
  id NotificationRequestArg(TipsNotificationType type) {
    return [OCMArg checkWithBlock:^BOOL(UNNotificationRequest* request) {
      TipsNotificationType requested_type =
          ParseTipsNotificationType(request).value();
      EXPECT_TRUE(IsTipsNotification(request));
      EXPECT_EQ(requested_type, type);
      return YES;
    }];
  }

  // Returns a mock UNNotificationResponse for the given notification `type`.
  id MockRequestResponse(TipsNotificationType type) {
    id mock_response = OCMClassMock([UNNotificationResponse class]);
    OCMStub([mock_response notification]).andReturn(MockNotification(type));
    return mock_response;
  }

  // Returns a mock UNNotification for the given notification `type`.
  id MockNotification(TipsNotificationType type) {
    UNNotificationRequest* request = TipsNotificationRequest(type);
    id mock_notification = OCMClassMock([UNNotification class]);
    OCMStub([mock_notification request]).andReturn(request);
    return mock_notification;
  }

  // Stubs the notification center's completion callback for
  // getPendingNotificationRequestsWithCompletionHandler.
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

  // Stubs the notification center's completion callback for
  // getPendingNotificationRequestsWithCompletionHandler.
  void StubGetDeliveredNotifications(NSArray<UNNotification*>* notifications) {
    auto completionCaller =
        ^BOOL(void (^completion)(NSArray<UNNotification*>* notifications)) {
          completion(notifications);
          return YES;
        };
    OCMStub([mock_notification_center_
        getDeliveredNotificationsWithCompletionHandler:
            [OCMArg checkWithBlock:completionCaller]]);
  }

  // Clears the pref used to store which notification types have already been
  // sent.
  void ClearSentNotifications() {
    GetApplicationContext()->GetLocalState()->SetInteger(
        kTipsNotificationsSentPref, 0);
  }

  // Sets the pref used to store which notification types have been sent.
  void SetSentNotifications(std::vector<TipsNotificationType> types) {
    int bits = 0;
    for (TipsNotificationType type : types) {
      bits |= 1 << int(type);
    }
    GetApplicationContext()->GetLocalState()->SetInteger(
        kTipsNotificationsSentPref, bits);
  }

  // Stubs the `prepareToPresentModal:` method from `ApplicationCommands` so
  // that it immediately calls the completion block.
  void StubPrepareToPresentModal() {
    prepare_to_present_modal_stub_ = [[PrepareToPresentModalStub alloc] init];
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:prepare_to_present_modal_stub_
                     forProtocol:@protocol(ApplicationCommands)];
  }

  // Sets up an OCMock expectation that a notification will be requested.
  void ExpectNotificationRequest(TipsNotificationType type) {
    ExpectNotificationRequest(NotificationRequestArg(type));
  }
  void ExpectNotificationRequest(id request) {
    auto completionCaller = ^BOOL(void (^completion)(NSError* error)) {
      completion(nil);
      return YES;
    };
    OCMExpect([mock_notification_center_
        addNotificationRequest:request
         withCompletionHandler:[OCMArg checkWithBlock:completionCaller]]);
  }

  // Ensures that Chrome is considered as default browser.
  void SetTrueChromeLikelyDefaultBrowser() { LogOpenHTTPURLFromExternalURL(); }

  // Ensures that Chrome is not considered as default browser.
  void SetFalseChromeLikelyDefaultBrowser() { ClearDefaultBrowserPromoData(); }

  // Clears the pref that stores the last action the user took with a Default
  // Browser promo.
  void ClearDefaultBrowserPromoLastAction() {
    GetApplicationContext()->GetLocalState()->ClearPref(
        prefs::kIosDefaultBrowserPromoLastAction);
  }

  base::test::TaskEnvironment task_environment_;
  const base::HistogramTester histogram_tester_;
  std::unique_ptr<TestChromeBrowserStateManager> browser_state_manager_;
  id mock_scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TipsNotificationClient> client_;
  id mock_notification_center_;
  std::unique_ptr<ScopedBlockSwizzler> notification_center_swizzler_;
  PrepareToPresentModalStub* prepare_to_present_modal_stub_;
};

#pragma mark - Test cases

// Tests that HandleNotificationReception does nothing and returns "NoData".
TEST_F(TipsNotificationClientTest, HandleNotificationReception) {
  EXPECT_EQ(client_->HandleNotificationReception(nil),
            UIBackgroundFetchResultNoData);
}

// Tests that RegisterActionalableNotifications returns an empty array.
TEST_F(TipsNotificationClientTest, RegisterActionableNotifications) {
  EXPECT_EQ(client_->RegisterActionableNotifications().count, 0u);
}

// Tests that the client clears any previously requested notifications.
TEST_F(TipsNotificationClientTest, ClearNotification) {
  UNNotificationRequest* request =
      TipsNotificationRequest(TipsNotificationType::kDefaultBrowser);
  StubGetPendingRequests(@[ request ]);
  StubGetDeliveredNotifications(@[]);
  OCMExpect([mock_notification_center_
      removePendingNotificationRequestsWithIdentifiers:[OCMArg any]]);
  ExpectNotificationRequest([OCMArg any]);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Cleared",
                                       TipsNotificationType::kDefaultBrowser,
                                       1);
}

// Tests that the client can register a Default Browser notification.
TEST_F(TipsNotificationClientTest, DefaultBrowserRequest) {
  WriteFirstRunSentinel();
  SetFalseChromeLikelyDefaultBrowser();
  ClearDefaultBrowserPromoLastAction();
  StubGetPendingRequests(nil);
  SetSentNotifications({TipsNotificationType::kWhatsNew,
                        TipsNotificationType::kSignin,
                        TipsNotificationType::kSetUpListContinuation});

  ExpectNotificationRequest(TipsNotificationType::kDefaultBrowser);
  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Sent", TipsNotificationType::kDefaultBrowser, 1);

  // Run again, but this time simulating a delivered notification.
  NSMutableArray<UNNotification*>* delivered_notifications = [NSMutableArray
      arrayWithObject:MockNotification(TipsNotificationType::kDefaultBrowser)];
  StubGetDeliveredNotifications(delivered_notifications);
  base::RunLoop run_loop_2;
  client_->OnSceneActiveForegroundBrowserReady(run_loop_2.QuitClosure());
  run_loop_2.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Triggered",
                                       TipsNotificationType::kDefaultBrowser,
                                       1);

  // Run again, but this time the delivered notification is gone.
  [delivered_notifications removeAllObjects];
  base::RunLoop run_loop_3;
  client_->OnSceneActiveForegroundBrowserReady(run_loop_3.QuitClosure());
  run_loop_3.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Dismissed",
                                       TipsNotificationType::kDefaultBrowser,
                                       1);
}

// Tests that the client handles a Default Browser notification response.
TEST_F(TipsNotificationClientTest, DefaultBrowserHandle) {
  StubPrepareToPresentModal();
  id mock_handler = OCMProtocolMock(@protocol(SettingsCommands));
  OCMExpect([mock_handler
      showDefaultBrowserSettingsFromViewController:nil
                                      sourceForUMA:
                                          DefaultBrowserSettingsPageSource::
                                              kTipsNotification]);
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_handler
                   forProtocol:@protocol(SettingsCommands)];

  id mock_response = MockRequestResponse(TipsNotificationType::kDefaultBrowser);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kDefaultBrowser,
                                       1);
}

// Tests that the client can register a Whats New notification.
TEST_F(TipsNotificationClientTest, WhatsNewRequest) {
  WriteFirstRunSentinel();
  SetTrueChromeLikelyDefaultBrowser();
  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kWhatsNew);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Sent",
                                       TipsNotificationType::kWhatsNew, 1);
}

// Tests that the client handles a Whats New notification response.
TEST_F(TipsNotificationClientTest, WhatsNewHandle) {
  StubPrepareToPresentModal();
  id mock_handler = OCMProtocolMock(@protocol(BrowserCoordinatorCommands));
  OCMExpect([mock_handler showWhatsNew]);
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_handler
                   forProtocol:@protocol(BrowserCoordinatorCommands)];

  id mock_response = MockRequestResponse(TipsNotificationType::kWhatsNew);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample("IOS.Notifications.Tips.Interaction",
                                       TipsNotificationType::kWhatsNew, 1);
}

// Tests that the client can register a SetUpList Continuation notification.
TEST_F(TipsNotificationClientTest, SetUpListContinuationRequest) {
  WriteFirstRunSentinel();
  SetSentNotifications({TipsNotificationType::kDefaultBrowser,
                        TipsNotificationType::kWhatsNew,
                        TipsNotificationType::kSignin});
  StubGetPendingRequests(nil);
  ExpectNotificationRequest(TipsNotificationType::kSetUpListContinuation);

  base::RunLoop run_loop;
  client_->OnSceneActiveForegroundBrowserReady(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Sent",
      TipsNotificationType::kSetUpListContinuation, 1);
}

// Tests that the client handles a SetUpList Continuation notification response.
TEST_F(TipsNotificationClientTest, SetUpListContinuationHandle) {
  StubPrepareToPresentModal();
  id mock_handler = OCMProtocolMock(@protocol(ContentSuggestionsCommands));
  OCMExpect([mock_handler showSetUpListSeeMoreMenu]);
  [browser_->GetCommandDispatcher()
      startDispatchingToTarget:mock_handler
                   forProtocol:@protocol(ContentSuggestionsCommands)];

  id mock_response =
      MockRequestResponse(TipsNotificationType::kSetUpListContinuation);
  client_->HandleNotificationInteraction(mock_response);

  EXPECT_OCMOCK_VERIFY(mock_handler);
  histogram_tester_.ExpectUniqueSample(
      "IOS.Notifications.Tips.Interaction",
      TipsNotificationType::kSetUpListContinuation, 1);
}
