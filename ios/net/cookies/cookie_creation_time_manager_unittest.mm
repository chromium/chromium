// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_creation_time_manager.h"

#import <Foundation/Foundation.h>
#include <stdint.h>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class CookieCreationTimeManagerTest : public PlatformTest {
 public:
  NSHTTPCookie* GetCookie(NSString* cookie_line) {
    NSArray* cookies = [NSHTTPCookie
        cookiesWithResponseHeaderFields:@{ @"Set-Cookie" : cookie_line }
                                 forURL:[NSURL URLWithString:@"http://foo"]];
    if ([cookies count] != 1)
      return nil;

    return [cookies objectAtIndex:0];
  }

 protected:
  CookieCreationTimeManager creation_time_manager_;
};

TEST_F(CookieCreationTimeManagerTest, SetAndGet) {
  NSHTTPCookie* cookie = GetCookie(@"A=B");
  ASSERT_TRUE(cookie);
  base::Time creation_time = base::Time::Now();
  creation_time_manager_.SetCreationTime(cookie, creation_time);
  EXPECT_EQ(creation_time, creation_time_manager_.GetCreationTime(cookie));
}

TEST_F(CookieCreationTimeManagerTest, GetFromSystemCookie) {
  NSHTTPCookie* cookie = GetCookie(@"A=B");

  // The creation time of a cookie that was never set through the
  // CookieCreationTimeManager should be retrieved from the system with 1
  // second precision.
  base::Time time = creation_time_manager_.GetCreationTime(cookie);
  base::Time now = base::Time::Now();
  ASSERT_FALSE(time.is_null());
  int64_t delta = (now - time).InMilliseconds();
  // On iOS 8, the range is (0, 1000) ms. The intervals tested are actually
  // 1200 ms to allow some imprecision.
  EXPECT_GT(delta, -100);
  EXPECT_LT(delta, 1100);
}

TEST_F(CookieCreationTimeManagerTest, MakeUniqueCreationTime) {
  base::Time now = base::Time::Now();

  // |now| is not used yet, so MakeUniqueCreationTime() should return that.
  base::Time creation_time = creation_time_manager_.MakeUniqueCreationTime(now);
  EXPECT_EQ(now, creation_time);

  NSHTTPCookie* cookie1 = GetCookie(@"A=B");
  ASSERT_TRUE(cookie1);
  creation_time_manager_.SetCreationTime(cookie1, creation_time);
  // |now| is used by cookie1, MakeUniqueCreationTime() should return the
  // incremented value.
  creation_time = creation_time_manager_.MakeUniqueCreationTime(now);
  EXPECT_EQ(base::Time::FromInternalValue(now.ToInternalValue() + 1),
            creation_time);

  // Delete |cookie1|.
  creation_time_manager_.DeleteCreationTime(cookie1);
  // |now| is available again because |cookie1| was deleted.
  creation_time = creation_time_manager_.MakeUniqueCreationTime(now);
  EXPECT_EQ(now, creation_time);

  creation_time_manager_.SetCreationTime(GetCookie(@"C=D"), creation_time);
  // Override |C| with a cookie that has a different time, to make |now|
  // available again.
  creation_time_manager_.SetCreationTime(GetCookie(@"C=E"),
                                         now - base::Milliseconds(1));
  // |now| is available again because |C| was overriden.
  creation_time = creation_time_manager_.MakeUniqueCreationTime(now);
  EXPECT_EQ(now, creation_time);

  creation_time_manager_.SetCreationTime(GetCookie(@"F=G"), creation_time);
  // Delete all creation times.
  creation_time_manager_.Clear();
  // |now| is available again because all creation times were cleared.
  creation_time = creation_time_manager_.MakeUniqueCreationTime(now);
  EXPECT_EQ(now, creation_time);
}

TEST_F(CookieCreationTimeManagerTest, MakeUniqueCreationTimeConflicts) {
  base::Time creation_time = base::Time::Now();
  int64_t time_internal_value = creation_time.ToInternalValue();
  // Insert two cookies with consecutive times.
  creation_time_manager_.SetCreationTime(GetCookie(@"A=B"), creation_time);
  creation_time_manager_.SetCreationTime(
      GetCookie(@"C=D"),
      base::Time::FromInternalValue(time_internal_value + 1));

  // MakeUniqueCreationTime() should insert at |time_internal_value + 2|.
  base::Time time =
      creation_time_manager_.MakeUniqueCreationTime(creation_time);
  EXPECT_EQ(time_internal_value + 2, time.ToInternalValue());
  creation_time_manager_.SetCreationTime(GetCookie(@"E=F"), time);

  // Leave an available slot at |time_internal_value + 3| and insert another
  // cookie at |time_internal_value + 4|.
  creation_time_manager_.SetCreationTime(
      GetCookie(@"G=H"),
      base::Time::FromInternalValue(time_internal_value + 4));

  // MakeUniqueCreationTime() should use the available slot.
  time = creation_time_manager_.MakeUniqueCreationTime(creation_time);
  EXPECT_EQ(time_internal_value + 3, time.ToInternalValue());
}

}  // namespace net
