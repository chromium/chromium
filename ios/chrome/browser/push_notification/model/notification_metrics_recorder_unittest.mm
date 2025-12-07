// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/notification_metrics_recorder.h"

#import <UserNotifications/UserNotifications.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_run_loop_timeout.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using base::test::ScopedRunLoopTimeout;

// A fake implementation of the NotificationClassifier protocol for use in
// tests.
@interface FakeNotificationClassifier : NSObject <NotificationClassifier>
// The notification type to be returned by the classifier.
@property(nonatomic, assign) NotificationType notificationType;
@end

@implementation FakeNotificationClassifier

- (NotificationType)classifyNotification:(UNNotification*)notification {
  return self.notificationType;
}

@end

class NotificationMetricsRecorderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    CreateNotificationCenter();
    classifier_ = [[FakeNotificationClassifier alloc] init];
    classifier_.notificationType = NotificationType::kTipsEnhancedSafeBrowsing;
    metrics_recorder_ = [[NotificationMetricsRecorder alloc]
        initWithNotificationCenter:notification_center_];
    metrics_recorder_.classifier = classifier_;
  }

  // Creates a stub UNNotification with the given `identifier`.
  UNNotification* StubNotification(NSString* identifier) {
    UNNotificationRequest* request = [UNNotificationRequest
        requestWithIdentifier:identifier
                      content:[[UNNotificationContent alloc] init]
                      trigger:nil];
    UNNotification* notification =
        [OCMockObject mockForClass:[UNNotification class]];
    OCMStub(notification.request).andReturn(request);
    return notification;
  }

  // Creates a mock UNUserNotificationCenter with a stubbed
  // `getDeliveredNotificationsWithCompletionHandler` that returns a copy of
  // `delivered_notifications_`.
  void CreateNotificationCenter() {
    notification_center_ = OCMClassMock([UNUserNotificationCenter class]);
    delivered_notifications_ = [NSMutableArray array];
    auto block = ^(void (^completionHandler)(NSArray<UNNotification*>*)) {
      completionHandler([this->delivered_notifications_ copy]);
      return YES;
    };
    OCMStub(
        [notification_center_ getDeliveredNotificationsWithCompletionHandler:
                                  [OCMArg checkWithBlock:block]]);
  }

  // Calls the metric recorder's `handleDeliveredNotificationsWithClosure:` and
  // waits for the closure to be called.
  void HandleDeliveredNotifications() {
    ScopedRunLoopTimeout scoped_timeout(FROM_HERE, base::Seconds(5));
    base::RunLoop run_loop;
    [metrics_recorder_
        handleDeliveredNotificationsWithClosure:run_loop.QuitClosure()];
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::HistogramTester histogram_tester_;
  id notification_center_;
  NotificationMetricsRecorder* metrics_recorder_;
  FakeNotificationClassifier* classifier_;
  NSMutableArray<UNNotification*>* delivered_notifications_;
};

// Tests that when handleDeliveredNotifications is called, new notifications are
// recorded as delivered, and notifications that are no longer present are
// recorded as dismissed.
TEST_F(NotificationMetricsRecorderTest, TestHandleDeliveredNotifications) {
  // Setup the notification center mock to return a notification.
  UNNotification* notification = StubNotification(@"id1");
  [delivered_notifications_ addObject:notification];

  // Call handleDeliveredNotifications and verify that the notification is
  // recorded as delivered.
  HandleDeliveredNotifications();
  histogram_tester_.ExpectBucketCount(
      "IOS.Notification.Received", NotificationType::kTipsEnhancedSafeBrowsing,
      1);
  histogram_tester_.ExpectTotalCount("IOS.Notification.Received", 1);

  // Setup the notification center mock to return no notifications.
  [delivered_notifications_ removeAllObjects];

  // Call handleDeliveredNotifications and verify that the notification is
  // recorded as dismissed.
  HandleDeliveredNotifications();
  histogram_tester_.ExpectBucketCount(
      "IOS.Notification.Dismissed", NotificationType::kTipsEnhancedSafeBrowsing,
      1);
  histogram_tester_.ExpectTotalCount("IOS.Notification.Dismissed", 1);
}

// Tests that when a new notification arrives while another is being handled,
// only the new notification is recorded as delivered.
TEST_F(NotificationMetricsRecorderTest,
       TestNewNotificationArrivesWhileAnotherIsHandled) {
  // Setup the notification center mock to return a notification.
  UNNotification* notification1 = StubNotification(@"id1");
  [delivered_notifications_ addObject:notification1];

  // Call handleDeliveredNotifications and verify that the notification is
  // recorded as delivered.
  HandleDeliveredNotifications();
  histogram_tester_.ExpectBucketCount(
      "IOS.Notification.Received", NotificationType::kTipsEnhancedSafeBrowsing,
      1);
  histogram_tester_.ExpectTotalCount("IOS.Notification.Received", 1);

  // Setup the notification center mock to return a second notification.
  classifier_.notificationType = NotificationType::kSafetyCheckUpdateChrome;
  UNNotification* notification2 = StubNotification(@"id2");
  [delivered_notifications_ addObject:notification2];

  // Call handleDeliveredNotifications and verify that only the new notification
  // is recorded as delivered.
  HandleDeliveredNotifications();
  histogram_tester_.ExpectBucketCount(
      "IOS.Notification.Received", NotificationType::kSafetyCheckUpdateChrome,
      1);
  histogram_tester_.ExpectBucketCount(
      "IOS.Notification.Received", NotificationType::kTipsEnhancedSafeBrowsing,
      1);
  histogram_tester_.ExpectTotalCount("IOS.Notification.Received", 2);
}

// Tests that when recordInteraction is called for a notification that has
// already been handled, the interaction is recorded, and the notification is
// removed from the handled list.
TEST_F(NotificationMetricsRecorderTest, TestRecordInteraction) {
  // Setup the notification center mock to return a notification.
  UNNotification* notification = StubNotification(@"id1");
  [delivered_notifications_ addObject:notification];

  // Call handleDeliveredNotifications to handle the notification.
  HandleDeliveredNotifications();
  ASSERT_TRUE([metrics_recorder_ wasDelivered:notification]);

  // Record an interaction with the notification.
  [metrics_recorder_ recordInteraction:notification];
  histogram_tester_.ExpectBucketCount(
      "IOS.Notification.Interaction",
      NotificationType::kTipsEnhancedSafeBrowsing, 1);
  histogram_tester_.ExpectTotalCount("IOS.Notification.Interaction", 1);
  histogram_tester_.ExpectTotalCount("IOS.Notification.Received", 1);

  // Setup the notification center mock to return no notifications.
  [delivered_notifications_ removeAllObjects];
  HandleDeliveredNotifications();
  histogram_tester_.ExpectTotalCount("IOS.Notification.Interaction", 1);
  histogram_tester_.ExpectTotalCount("IOS.Notification.Received", 1);
}

// Tests that when recordInteraction is called for a notification that has not
// been handled, both the delivery and interaction are recorded.
TEST_F(NotificationMetricsRecorderTest, TestRecordInteractionNotHandled) {
  UNNotification* notification = StubNotification(@"id1");

  // Record an interaction with the notification.
  [metrics_recorder_ recordInteraction:notification];
  histogram_tester_.ExpectBucketCount(
      "IOS.Notification.Received", NotificationType::kTipsEnhancedSafeBrowsing,
      1);
  histogram_tester_.ExpectTotalCount("IOS.Notification.Received", 1);
  histogram_tester_.ExpectBucketCount(
      "IOS.Notification.Interaction",
      NotificationType::kTipsEnhancedSafeBrowsing, 1);
  histogram_tester_.ExpectTotalCount("IOS.Notification.Interaction", 1);
}
