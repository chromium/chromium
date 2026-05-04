// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/user_interface_style_recorder.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "testing/platform_test.h"

#import "base/test/task_environment.h"
#import "base/test/icu_test_util.h"
#import "base/test/scoped_libc_timezone_override.h"

namespace {

class UserInterfaceStyleRecorderTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedRestoreDefaultTimezone timezone_{"UTC"};
  base::test::ScopedLibcTimezoneOverride libc_timezone_{"UTC"};
};

TEST_F(UserInterfaceStyleRecorderTest, RecordDarkModeMetrics_MidnightTo6AM) {
  base::HistogramTester tester;
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_hour = exploded.hour;

  // Target: 1 AM
  int hours_to_advance = (25 - current_hour + 1) % 24;
  task_environment_.AdvanceClock(base::Hours(hours_to_advance));

  UserInterfaceStyleRecorder* recorder = [[UserInterfaceStyleRecorder alloc]
      initWithUserInterfaceStyle:UIUserInterfaceStyleDark];
  (void)recorder;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  tester.ExpectBucketCount("UserInterfaceStyle.CurrentlyUsed", 2, 1);
  tester.ExpectBucketCount("UserInterfaceStyle.CurrentlyUsed.MidnightTo6AM", 2,
                           1);
  tester.ExpectTotalCount("UserInterfaceStyle.CurrentlyUsed.MidnightTo6AM", 1);
}

TEST_F(UserInterfaceStyleRecorderTest, RecordDarkModeMetrics_6AMToNoon) {
  base::HistogramTester tester;
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_hour = exploded.hour;

  // Target: 7 AM
  int hours_to_advance = (25 - current_hour + 7) % 24;
  task_environment_.AdvanceClock(base::Hours(hours_to_advance));

  UserInterfaceStyleRecorder* recorder = [[UserInterfaceStyleRecorder alloc]
      initWithUserInterfaceStyle:UIUserInterfaceStyleDark];
  (void)recorder;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  tester.ExpectBucketCount("UserInterfaceStyle.CurrentlyUsed", 2, 1);
  tester.ExpectBucketCount("UserInterfaceStyle.CurrentlyUsed.6AMToNoon", 2, 1);
  tester.ExpectTotalCount("UserInterfaceStyle.CurrentlyUsed.6AMToNoon", 1);
}

TEST_F(UserInterfaceStyleRecorderTest, RecordDarkModeMetrics_NoonTo6PM) {
  base::HistogramTester tester;
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_hour = exploded.hour;

  // Target: 1 PM (13)
  int hours_to_advance = (25 - current_hour + 13) % 24;
  task_environment_.AdvanceClock(base::Hours(hours_to_advance));

  UserInterfaceStyleRecorder* recorder = [[UserInterfaceStyleRecorder alloc]
      initWithUserInterfaceStyle:UIUserInterfaceStyleDark];
  (void)recorder;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  tester.ExpectBucketCount("UserInterfaceStyle.CurrentlyUsed", 2, 1);
  tester.ExpectBucketCount("UserInterfaceStyle.CurrentlyUsed.NoonTo6PM", 2, 1);
  tester.ExpectTotalCount("UserInterfaceStyle.CurrentlyUsed.NoonTo6PM", 1);
}

TEST_F(UserInterfaceStyleRecorderTest, RecordDarkModeMetrics_6PMToMidnight) {
  base::HistogramTester tester;
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_hour = exploded.hour;

  // Target: 7 PM (19)
  int hours_to_advance = (25 - current_hour + 19) % 24;
  task_environment_.AdvanceClock(base::Hours(hours_to_advance));

  UserInterfaceStyleRecorder* recorder = [[UserInterfaceStyleRecorder alloc]
      initWithUserInterfaceStyle:UIUserInterfaceStyleDark];
  (void)recorder;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  tester.ExpectBucketCount("UserInterfaceStyle.CurrentlyUsed", 2, 1);
  tester.ExpectBucketCount("UserInterfaceStyle.CurrentlyUsed.6PMToMidnight", 2,
                           1);
  tester.ExpectTotalCount("UserInterfaceStyle.CurrentlyUsed.6PMToMidnight", 1);
}

TEST_F(UserInterfaceStyleRecorderTest, RecordUnsupportedStyle) {
  base::HistogramTester tester;

  UserInterfaceStyleRecorder* recorder = [[UserInterfaceStyleRecorder alloc]
      initWithUserInterfaceStyle:(UIUserInterfaceStyle)99];
  (void)recorder;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil];

  tester.ExpectBucketCount("UserInterfaceStyle.CurrentlyUsed", 3, 1);
}

}  // namespace
