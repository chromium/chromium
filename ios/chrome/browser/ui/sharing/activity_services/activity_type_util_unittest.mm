// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activity_type_util.h"

#import "ios/chrome/browser/ui/sharing/activity_services/activities/print_activity.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

void StringToTypeTestHelper(NSString* activityString,
                            activity_type_util::ActivityType expectedType) {
  EXPECT_EQ(activity_type_util::TypeFromString(activityString), expectedType);
}

using ActivityTypeUtilTest = PlatformTest;

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

  PrintActivity* printActivity = [[PrintActivity alloc] initWithData:nil
                                                             handler:nil
                                                  baseViewController:nil];
  StringToTypeTestHelper(printActivity.activityType, activity_type_util::PRINT);
}

}  // namespace
