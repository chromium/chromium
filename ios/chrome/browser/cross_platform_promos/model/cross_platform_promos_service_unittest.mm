// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service.h"

#import <UIKit/UIKit.h>

#import "base/json/values_util.h"
#import "base/test/task_environment.h"
#import "base/time/clock.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Expects `time1` and `time2` to be within `delta` of each other.
#define EXPECT_TIME_WITHIN(time1, time2, delta) \
  EXPECT_LT((time1 - time2).magnitude(), delta)

// Test suite for the `CrossPlatformPromosService`.
class CrossPlatformPromosServiceTest : public PlatformTest {
 public:
  CrossPlatformPromosServiceTest() {
    profile_ = TestProfileIOS::Builder().Build();
    prefs_ = profile_->GetPrefs();
    service_ = CrossPlatformPromosServiceFactory::GetForProfile(profile_.get());
  }

  // Simulate that the app was foregrounded.
  void SimulateAppForegrounded() {
    service_->OnApplicationWillEnterForeground();
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<CrossPlatformPromosService> service_;
};

// Tests that foregrounding the app records a new active day.
TEST_F(CrossPlatformPromosServiceTest, RecordActiveDay_AddNewDay) {
  SimulateAppForegrounded();
  const base::Value::List& active_days =
      prefs_->GetList(prefs::kCrossPlatformPromosActiveDays);
  EXPECT_EQ(1u, active_days.size());
}

// Tests that multiple app foregrounds doesn't add duplicate days.
TEST_F(CrossPlatformPromosServiceTest, RecordActiveDay_AddDuplicateDay) {
  // Advance to noon on a new day to avoid timezone issues at midnight.
  base::Time now = task_environment_.GetMockClock()->Now();
  base::Time tomorrow = (now + base::Days(1)).LocalMidnight();
  task_environment_.FastForwardBy((tomorrow - now) + base::Hours(12));

  SimulateAppForegrounded();
  task_environment_.FastForwardBy(base::Seconds(1));
  SimulateAppForegrounded();
  const base::Value::List& active_days =
      prefs_->GetList(prefs::kCrossPlatformPromosActiveDays);
  EXPECT_EQ(1u, active_days.size());
}

// Tests that old active days are pruned.
TEST_F(CrossPlatformPromosServiceTest, RecordActiveDay_PruneOldDays) {
  SimulateAppForegrounded();
  task_environment_.FastForwardBy(base::Days(30));
  SimulateAppForegrounded();
  const base::Value::List& active_days =
      prefs_->GetList(prefs::kCrossPlatformPromosActiveDays);
  EXPECT_EQ(1u, active_days.size());
  std::optional<base::Time> stored_time = base::ValueToTime(active_days[0]);
  EXPECT_LT(stored_time.value() - base::Time::Now(), base::Days(1));
}

// Tests that when the app becomes active for 16 days, the pref gets set.
TEST_F(CrossPlatformPromosServiceTest, OnAppDidBecomeActive) {
  // The pref should be unset initially.
  EXPECT_EQ(base::Time(),
            prefs_->GetTime(prefs::kCrossPlatformPromosIOS16thActiveDay));

  base::Time first_day = base::Time::Now();

  // Record 16 active days.
  for (int i = 0; i < 16; ++i) {
    SimulateAppForegrounded();
    task_environment_.FastForwardBy(base::Days(1));
  }

  // The pref should now be set to a Time within the last day.
  base::Time active_16th_day =
      prefs_->GetTime(prefs::kCrossPlatformPromosIOS16thActiveDay);
  EXPECT_TIME_WITHIN(active_16th_day, first_day, base::Days(1));
}
