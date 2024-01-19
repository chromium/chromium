// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/model/tips_notification_client.h"

#import <UserNotifications/UserNotifications.h>

#import "base/threading/thread_restrictions.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using startup_metric_utils::FirstRunSentinelCreationResult;
using tips_notifications::IsTipsNotification;
using tips_notifications::NotificationType;

class TipsNotificationClientTest : public PlatformTest {
 protected:
  TipsNotificationClientTest() { SetupMockNotificationCenter(); }

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
  id NotificationRequestArg(NotificationType type) {
    return [OCMArg checkWithBlock:^BOOL(UNNotificationRequest* request) {
      NotificationType requested_type =
          tips_notifications::ParseType(request).value();
      EXPECT_TRUE(IsTipsNotification(request));
      EXPECT_EQ(requested_type, type);
      return YES;
    }];
  }

  // Returns a mock UNNotificationResponse for the given notification `type`.
  id MockRequestResponse(NotificationType type) {
    UNNotificationRequest* request = tips_notifications::Request(type);
    id mock_response = OCMClassMock([UNNotificationResponse class]);
    id mock_notification = OCMClassMock([UNNotification class]);
    OCMStub([mock_response notification]).andReturn(mock_notification);
    OCMStub([mock_notification request]).andReturn(request);
    return mock_response;
  }

  // Ensures that Chrome is considered as default browser.
  void SetTrueChromeLikelyDefaultBrowser() { LogOpenHTTPURLFromExternalURL(); }

  // Ensures that Chrome is not considered as default browser.
  void SetFalseChromeLikelyDefaultBrowser() { ClearDefaultBrowserPromoData(); }

  std::unique_ptr<TipsNotificationClient> client_ =
      std::make_unique<TipsNotificationClient>();
  id mock_notification_center_;
  std::unique_ptr<ScopedBlockSwizzler> notification_center_swizzler_;
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
  OCMExpect([mock_notification_center_
      removePendingNotificationRequestsWithIdentifiers:[OCMArg any]]);

  client_->OnSceneActiveForegroundBrowserReady();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

// Tests that the client can register a Default Browser notification.
TEST_F(TipsNotificationClientTest, DefaultBrowserRequest) {
  WriteFirstRunSentinel();
  SetFalseChromeLikelyDefaultBrowser();

  id request_arg = NotificationRequestArg(NotificationType::kDefaultBrowser);
  OCMExpect([mock_notification_center_ addNotificationRequest:request_arg
                                        withCompletionHandler:[OCMArg any]]);
  client_->OnSceneActiveForegroundBrowserReady();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

// Tests that the client handles a Default Browser notification response.
TEST_F(TipsNotificationClientTest, DefaultBrowserHandle) {
  id mock_response = MockRequestResponse(NotificationType::kDefaultBrowser);
  client_->HandleNotificationInteraction(mock_response);
  // TODO(crbug.com/1517910) verify DefaultBrowser interaction.
}

// Tests that the client can register a Whats New notification.
TEST_F(TipsNotificationClientTest, WhatsNewRequest) {
  WriteFirstRunSentinel();
  SetTrueChromeLikelyDefaultBrowser();

  id request_arg = NotificationRequestArg(NotificationType::kWhatsNew);
  OCMExpect([mock_notification_center_ addNotificationRequest:request_arg
                                        withCompletionHandler:[OCMArg any]]);
  client_->OnSceneActiveForegroundBrowserReady();

  EXPECT_OCMOCK_VERIFY(mock_notification_center_);
}

// Tests that the client handles a Whats New notification response.
TEST_F(TipsNotificationClientTest, WhatsNewHandle) {
  id mock_response = MockRequestResponse(NotificationType::kWhatsNew);
  client_->HandleNotificationInteraction(mock_response);
  // TODO(crbug.com/1517911) verify WhatsNew interaction.
}
