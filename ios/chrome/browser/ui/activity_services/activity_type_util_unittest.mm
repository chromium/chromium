// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"

#include "ios/chrome/browser/ui/activity_services/activities/print_activity.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
