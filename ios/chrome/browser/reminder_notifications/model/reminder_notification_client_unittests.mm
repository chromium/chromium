// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_client.h"

#import <UserNotifications/UserNotifications.h>

#import "base/apple/foundation_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/reminder_notifications/model/reminder_notification_builder.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Tests the `ReminderNotificationClient` and assocated objects.
class ReminderNotificationClientTest : public PlatformTest {
 protected:
  ReminderNotificationClientTest() {
    client_ = std::make_unique<ReminderNotificationClient>(&profile_manager_);
  }

  IOSChromeScopedTestingLocalState local_state_;
  TestProfileManagerIOS profile_manager_;
  std::unique_ptr<ReminderNotificationClient> client_;
};

#pragma mark - Test cases

// Test the ReminderNotificationBuilder that the client uses to build
// local notification requests.
TEST_F(ReminderNotificationClientTest, Builder) {
  GURL url("http://example.org");
  base::Time time = base::Time::Now() + base::Hours(1);

  ReminderNotificationBuilder* builder =
      [[ReminderNotificationBuilder alloc] initWithURL:url time:time];
  [builder setPageTitle:@"Example Page Title"];
  UNNotificationRequest* request = [builder buildRequest];
  EXPECT_NE(request, nil);
  EXPECT_NSEQ(request.content.title,
              l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_TITLE));
  EXPECT_NSEQ(request.content.userInfo[@"url"], net::NSURLWithGURL(url));
  UNCalendarNotificationTrigger* trigger =
      base::apple::ObjCCast<UNCalendarNotificationTrigger>(request.trigger);
  NSTimeInterval time_interval =
      [trigger.nextTriggerDate timeIntervalSinceDate:time.ToNSDate()];
  // Seconds are truncated when scheduling, so `time_interval` must be less
  // than 60 seconds.
  EXPECT_LT(fabs(time_interval), 60);
}
