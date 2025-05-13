// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

#import <UserNotifications/UserNotifications.h>

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_prefs.h"
#import "ios/chrome/browser/safety_check_notifications/utils/constants.h"
#import "ios/chrome/browser/safety_check_notifications/utils/utils.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_push_notification_client.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PushNotificationClientTest : public PlatformTest {
 protected:
  PushNotificationClientTest() {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());

    client_ = std::make_unique<SendTabPushNotificationClient>();
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

  void TearDown() override {
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
    PlatformTest::TearDown();
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

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<PushNotificationClient> client_;
  id mock_notification_center_;
  std::unique_ptr<ScopedBlockSwizzler> notification_center_swizzler_;
};

#pragma mark - Test cases

// Tests that scheduling a tips notification is delayed by 1 day when there is
// an already scheduled safety check notification.
TEST_F(PushNotificationClientTest, VerifyDelayNotification) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNotificationCollisionManagement);
  push_notification_prefs::RegisterLocalStatePrefs(local_state_->registry());

  id safety_check_notification = OCMClassMock([UNNotificationRequest class]);
  OCMStub([safety_check_notification identifier])
      .andReturn(kSafetyCheckUpdateChromeNotificationID);

  StubGetPendingRequests(@[ safety_check_notification ]);

  ScheduledNotificationRequest tip_request = {
      kTipsNotificationId,
      ContentForTipsNotificationType(TipsNotificationType::kDefaultBrowser,
                                     false, ""),
      TipsNotificationTriggerDelta(false, TipsNotificationUserType::kUnknown)};

  id request_check =
      [OCMArg checkWithBlock:^BOOL(UNNotificationRequest* request) {
        // verify that it is scheduled for longer than 3 days (the default),
        // which is a proxy that it is set to 4 days without having to be
        // exactly correct.
        UNTimeIntervalNotificationTrigger* trigger =
            (UNTimeIntervalNotificationTrigger*)request.trigger;
        EXPECT_GT(trigger.timeInterval, base::Days(3).InSecondsF());
        return YES;
      }];

  auto completionCaller = ^BOOL(void (^completion)(NSError* error)) {
    completion(nil);
    return YES;
  };
  OCMExpect([mock_notification_center_
      addNotificationRequest:request_check
       withCompletionHandler:[OCMArg checkWithBlock:completionCaller]]);

  base::RunLoop run_loop;
  client_->CheckRateLimitBeforeSchedulingNotification(
      tip_request, base::IgnoreArgs<NSError*>(run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}
