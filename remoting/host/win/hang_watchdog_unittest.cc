// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/hang_watchdog.h"

#include <atomic>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class HangWatchdogTest : public testing::Test {
 protected:
  static constexpr base::TimeDelta kHangTimeout = base::Seconds(1);
  static constexpr base::TimeDelta kHeartbeatInterval = base::Milliseconds(250);

  HangWatchdog CreateWatchdog() {
    return HangWatchdog(
        kHangTimeout, kHeartbeatInterval,
        base::BindRepeating(&HangWatchdogTest::OnHang, base::Unretained(this)));
  }

  // Runs tasks on the main (watched) thread for `duration`.
  void RunTasksFor(base::TimeDelta duration) {
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), duration);
    run_loop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Signaled by each `OnHang()` call.
  base::WaitableEvent hang_detected_{
      base::WaitableEvent::ResetPolicy::AUTOMATIC};
  std::atomic<int> hang_count_ = 0;

 private:
  // Called on the watchdog thread.
  void OnHang() {
    ++hang_count_;
    hang_detected_.Signal();
  }
};

TEST_F(HangWatchdogTest, RunsOnHangWhenThreadIsBlocked) {
  HangWatchdog watchdog = CreateWatchdog();

  // Block the watched thread without running tasks.
  EXPECT_TRUE(hang_detected_.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_EQ(hang_count_, 1);
}

TEST_F(HangWatchdogTest, DoesNotRunOnHangWhenThreadIsIdle) {
  HangWatchdog watchdog = CreateWatchdog();

  RunTasksFor(2 * kHangTimeout);

  EXPECT_EQ(hang_count_, 0);
}

TEST_F(HangWatchdogTest, RunsOnHangOncePerHang) {
  HangWatchdog watchdog = CreateWatchdog();

  // Block the watched thread until the hang is reported.
  ASSERT_TRUE(hang_detected_.TimedWait(TestTimeouts::action_max_timeout()));

  // Keep blocking. The same hang isn't reported again.
  EXPECT_FALSE(hang_detected_.TimedWait(4 * kHeartbeatInterval));
  EXPECT_EQ(hang_count_, 1);

  // Recover, so that a heartbeat is recorded and checked.
  RunTasksFor(2 * kHeartbeatInterval);
  EXPECT_EQ(hang_count_, 1);

  // Block again. The new hang is reported.
  EXPECT_TRUE(hang_detected_.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_EQ(hang_count_, 2);
}

}  // namespace remoting
