// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activity_type_util.h"

#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/print_activity.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/send_tab_to_self_activity.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

void StringToTypeTestHelper(NSString* activityString,
                            activity_type_util::ActivityType expectedType) {
  EXPECT_EQ(activity_type_util::TypeFromString(activityString), expectedType);
}

using ActivityTypeUtilTest = PlatformTest;

// Tests mapping from activity type strings to ActivityType enum values.
TEST_F(ActivityTypeUtilTest, StringToTypeTest) {
  StringToTypeTestHelper(@"", activity_type_util::UNKNOWN);
  StringToTypeTestHelper(@"foo", activity_type_util::UNKNOWN);
  StringToTypeTestHelper(@"com.google", activity_type_util::UNKNOWN);
  StringToTypeTestHelper(@"com.google.", activity_type_util::GOOGLE_UNKNOWN);
  StringToTypeTestHelper(@"com.google.Gmail",
                         activity_type_util::GOOGLE_UNKNOWN);
  StringToTypeTestHelper(@"com.google.Gmail.Bar",
                         activity_type_util::GOOGLE_GMAIL);
  StringToTypeTestHelper(@"com.apple.UIKit.activity.Mail",
                         activity_type_util::NATIVE_MAIL);
  StringToTypeTestHelper(@"com.apple.UIKit.activity.Mail.Qux",
                         activity_type_util::UNKNOWN);
  StringToTypeTestHelper(@"com.google.chrome.sendTabToSelfActivity",
                         activity_type_util::SEND_TAB_TO_SELF);
  StringToTypeTestHelper(@"com.google.chrome.sendTabToSelfActivity.device_guid",
                         activity_type_util::SEND_TAB_TO_SELF);

  PrintActivity* printActivity = [[PrintActivity alloc] initWithData:nil
                                                             handler:nil
                                                  baseViewController:nil];
  StringToTypeTestHelper(printActivity.activityType, activity_type_util::PRINT);

  SendTabToSelfActivity* sendTabToSelfActivity =
      [[SendTabToSelfActivity alloc] initWithData:nil handler:nil];
  StringToTypeTestHelper(sendTabToSelfActivity.activityType,
                         activity_type_util::SEND_TAB_TO_SELF);
}

// Tests that RecordMetricForActivity logs the MobileShareMenuSendTabToSelf
// action for SEND_TAB_TO_SELF.
TEST_F(ActivityTypeUtilTest, RecordMetricForActivity_SendTabToSelf) {
  base::UserActionTester user_action_tester;
  activity_type_util::RecordMetricForActivity(
      activity_type_util::SEND_TAB_TO_SELF);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("MobileShareMenuSendTabToSelf"));
}

}  // namespace
