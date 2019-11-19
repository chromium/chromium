// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor.h"

#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ScreenlockMonitorTestSource : public ScreenlockMonitorSource {
 public:
  ScreenlockMonitorTestSource() {
    DCHECK(base::MessageLoopCurrent::Get())
        << "ScreenlocMonitorTestSource requires a MessageLoop.";
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
 protected:
  ScreenlockMonitorTest() {
    screenlock_monitor_source_ = new ScreenlockMonitorTestSource();
    screenlock_monitor_ = std::make_unique<ScreenlockMonitor>(
        std::unique_ptr<ScreenlockMonitorSource>(screenlock_monitor_source_));
  }
  ~ScreenlockMonitorTest() override = default;

 protected:
  ScreenlockMonitorTestSource* screenlock_monitor_source_;
  std::unique_ptr<ScreenlockMonitor> screenlock_monitor_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockMonitorTest);
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

}  // namespace content
