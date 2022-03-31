// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/install_time_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using InstallTimeUtilTest = PlatformTest;

TEST_F(InstallTimeUtilTest, ComputeInstallationTime) {
  const base::Time null_time = base::Time();
  const base::Time now = base::Time::Now();
  const base::Time one_month_ago = now - base::Days(30);
  const base::Time sentinel =
      base::Time::FromTimeT(install_time_util::kUnknownInstallDate);

  base::Time install_time;
  base::TimeDelta delta_from_now;

  // Case 1: On first run, always set the install time to Now.
  install_time =
      install_time_util::ComputeInstallationTimeInternal(true, null_time);
  delta_from_now = install_time - now;
  EXPECT_FALSE(install_time.is_null());
  EXPECT_TRUE(delta_from_now.InSeconds() < 100);

  // Case 2: First run, but there was already an install time in NSUserDefaults.
  // Ignore the NSUserDefaults time and return Now.
  install_time =
      install_time_util::ComputeInstallationTimeInternal(true, one_month_ago);
  delta_from_now = install_time - now;
  EXPECT_FALSE(install_time.is_null());
  EXPECT_TRUE(delta_from_now.InSeconds() < 100);

  // Case 3: Not first run, and NSUserDefaults didn't have an install time.
  // Should return the sentinel value.
  install_time =
      install_time_util::ComputeInstallationTimeInternal(false, null_time);
  EXPECT_FALSE(install_time.is_null());
  EXPECT_EQ(sentinel, install_time);

  // Case 4: Not first run, and NSUserDefaults had an install time.  Should
  // migrate that to LocalState.
  install_time =
      install_time_util::ComputeInstallationTimeInternal(false, one_month_ago);
  EXPECT_FALSE(install_time.is_null());
  EXPECT_EQ(one_month_ago, install_time);
}
