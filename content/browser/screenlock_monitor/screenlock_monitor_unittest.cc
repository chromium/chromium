// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/screenlock_monitor/screenlock_monitor.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ScreenlockMonitorTestSource : public ScreenlockMonitorSource {
 public:
  ScreenlockMonitorTestSource() {
    DCHECK(base::CurrentThread::Get())
        << "ScreenlockMonitorTestSource requires a MessageLoop.";
  }
  ~ScreenlockMonitorTestSource() override = default;

  void GenerateScreenLockedEvent() {
    ProcessScreenlockEvent(SCREEN_LOCK_EVENT);
    base::RunLoop().RunUntilIdle();
  }

  void GenerateScreenUnlockedEvent() {
    ProcessScreenlockEvent(SCREEN_UNLOCK_EVENT);
    base::RunLoop().RunUntilIdle();
  }
};

class ScreenlockMonitorTestObserver : public ScreenlockObserver {
 public:
  ScreenlockMonitorTestObserver() : is_screen_locked_(false) {}
  ~ScreenlockMonitorTestObserver() override = default;

  // ScreenlockObserver callbacks.
  void OnScreenLocked() override { is_screen_locked_ = true; }
  void OnScreenUnlocked() override { is_screen_locked_ = false; }

  bool IsScreenLocked() { return is_screen_locked_; }

 private:
  bool is_screen_locked_;
};

class ScreenlockMonitorTest : public testing::Test {
 public:
  ScreenlockMonitorTest(const ScreenlockMonitorTest&) = delete;
  ScreenlockMonitorTest& operator=(const ScreenlockMonitorTest&) = delete;

 protected:
  ScreenlockMonitorTest() {
    screenlock_monitor_source_ = new ScreenlockMonitorTestSource();
    screenlock_monitor_ = std::make_unique<ScreenlockMonitor>(
        std::unique_ptr<ScreenlockMonitorSource>(screenlock_monitor_source_));
  }
  ~ScreenlockMonitorTest() override { screenlock_monitor_source_ = nullptr; }

 protected:
  raw_ptr<ScreenlockMonitorTestSource> screenlock_monitor_source_;
  std::unique_ptr<ScreenlockMonitor> screenlock_monitor_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ScreenlockMonitorTest, ScreenlockNotifications) {
  const int kObservers = 5;

  ScreenlockMonitorTestObserver observers[kObservers];
  for (int index = 0; index < kObservers; ++index)
    screenlock_monitor_->AddObserver(&observers[index]);

  // Pretend screen is locked.
  screenlock_monitor_source_->GenerateScreenLockedEvent();
  // Ensure all observers were notified of the event
  for (int index = 0; index < kObservers; ++index)
    EXPECT_TRUE(observers[index].IsScreenLocked());

  // Pretend screen is unlocked.
  screenlock_monitor_source_->GenerateScreenUnlockedEvent();
  // Ensure all observers were notified of the event
  for (int index = 0; index < kObservers; ++index)
    EXPECT_FALSE(observers[index].IsScreenLocked());
}

TEST_F(ScreenlockMonitorTest, HistogramTest) {
  base::HistogramTester histogram_tester;

  base::TimeDelta time_to_advance = base::Seconds(10);
  task_environment_.AdvanceClock(time_to_advance);
  screenlock_monitor_source_->GenerateScreenLockedEvent();
  // We should not log any metrics for the first lock event.
  histogram_tester.ExpectTotalCount("ScreenLocker.Unlocked.Duration", 0);
  histogram_tester.ExpectTotalCount("ScreenLocker.Locked.Duration", 0);

  time_to_advance = base::Seconds(11);
  task_environment_.AdvanceClock(time_to_advance);
  screenlock_monitor_source_->GenerateScreenUnlockedEvent();
  // A Locked event with duration should be logged on unlock.
  histogram_tester.ExpectTotalCount("ScreenLocker.Unlocked.Duration", 0);
  histogram_tester.ExpectTotalCount("ScreenLocker.Locked.Duration", 1);
  histogram_tester.ExpectUniqueTimeSample("ScreenLocker.Locked.Duration",
                                          time_to_advance, 1);

  time_to_advance = base::Seconds(12);
  task_environment_.AdvanceClock(time_to_advance);
  screenlock_monitor_source_->GenerateScreenLockedEvent();
  // A Unlocked event with duration should be logged on lock.
  histogram_tester.ExpectTotalCount("ScreenLocker.Unlocked.Duration", 1);
  histogram_tester.ExpectTotalCount("ScreenLocker.Locked.Duration", 1);
  histogram_tester.ExpectUniqueTimeSample("ScreenLocker.Unlocked.Duration",
                                          time_to_advance, 1);
}

}  // namespace content
