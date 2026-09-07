// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/alive_checker.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
constexpr int kCheckIntervalMs = 10;
constexpr int kTimeoutMs = 50;
}  // namespace

class MockPowerObserverHelper : public PowerObserverHelper {
 public:
  MockPowerObserverHelper(scoped_refptr<base::SequencedTaskRunner> task_runner,
                          base::RepeatingClosure suspend_callback,
                          base::RepeatingClosure resume_callback)

      : PowerObserverHelper(std::move(task_runner),
                            std::move(suspend_callback),
                            std::move(resume_callback)) {}

  bool IsSuspending() const override {
    DCHECK(TaskRunnerForTesting()->RunsTasksInCurrentSequence());
    return is_suspending_;
  }

  void Suspend() {
    DCHECK(TaskRunnerForTesting()->RunsTasksInCurrentSequence());
    is_suspending_ = true;
    SuspendCallbackForTesting()->Run();
  }

  void Resume() {
    DCHECK(TaskRunnerForTesting()->RunsTasksInCurrentSequence());
    is_suspending_ = false;
    ResumeCallbackForTesting()->Run();
  }

 private:
  bool is_suspending_ = false;
};

class AliveCheckerTest : public testing::Test {
 public:
  AliveCheckerTest()
      : alive_checker_thread_("AliveCheckerThread"),
        detected_dead_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED) {
    alive_checker_thread_.StartAndWaitForTesting();
  }

  AliveCheckerTest(const AliveCheckerTest&) = delete;
  AliveCheckerTest& operator=(const AliveCheckerTest&) = delete;

  void OnDetectedDead() {
    EXPECT_TRUE(alive_checker_thread_.task_runner()->BelongsToCurrentThread());
    detected_dead_event_.Signal();
  }

  std::unique_ptr<PowerObserverHelper> CreatePowerObserverHelper(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::RepeatingClosure suspend_callback,
      base::RepeatingClosure resume_callback) {
    std::unique_ptr<MockPowerObserverHelper> mock_power_observer_helper =
        std::make_unique<MockPowerObserverHelper>(std::move(task_runner),
                                                  std::move(suspend_callback),
                                                  std::move(resume_callback));
    mock_power_observer_helper_ = mock_power_observer_helper.get();
    return mock_power_observer_helper;
  }

 protected:
  ~AliveCheckerTest() override {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    alive_checker_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AliveCheckerTest::ResetAliveCheckerOnAliveCheckerThread,
                       base::Unretained(this), &done));
    done.Wait();
  }

  void CreateAliveChecker(bool stop_at_first_alive_notification,
                          bool pause_check_during_suspend) {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    alive_checker_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &AliveCheckerTest::CreateAliveCheckerOnAliveCheckerThread,
            base::Unretained(this), stop_at_first_alive_notification,
            pause_check_during_suspend, &done));
    done.Wait();
  }

  void StartAliveChecker() {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    alive_checker_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](AliveChecker* checker, base::WaitableEvent* done) {
                         checker->Start();
                         done->Signal();
                       },
                       base::Unretained(alive_checker_.get()), &done));
    done.Wait();
  }

  // Notifies |alive_checker_| that we're alive, and if
  // |remaining_notifications| > 1, posts a delayed task to itself on
  // |alive_checker_thread_| with |remaining_notifications| decreased by 1. Can
  // be called on any task runner.
  void NotifyAliveMultipleTimes(int remaining_notifications,
                                base::TimeDelta delay) {
    alive_checker_->NotifyAlive();
    if (remaining_notifications > 1) {
      alive_checker_thread_.task_runner()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AliveCheckerTest::NotifyAliveMultipleTimes,
                         base::Unretained(this), remaining_notifications - 1,
                         delay),
          delay);
    }
  }

  void WaitUntilDetectedDead() {
    detected_dead_event_.Wait();
    detected_dead_event_.Reset();
  }

  // Returns true if the dead callback (AliveCheckerTest::OnDetectedDead) is run
  // by the AliveChecker, false if timed out.
  bool WaitUntilDetectedDeadWithTimeout(base::TimeDelta timeout) {
    bool signaled = detected_dead_event_.TimedWait(timeout);
    detected_dead_event_.Reset();
    return signaled;
  }

  // Calls AliveChecker::DetectedDead() on the |alive_checker_thread_| and
  // returns the result.
  bool GetDetectedDead() {
    bool detected_dead = false;
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    alive_checker_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AliveCheckerTest::GetDetectedDeadOnAliveCheckerThread,
                       base::Unretained(this), &detected_dead, &done));
    done.Wait();
    return detected_dead;
  }

  // The test task environment.
  base::test::TaskEnvironment task_environment_;

  // The thread the checker is run on.
  base::Thread alive_checker_thread_;

  // AliveChecker under test.
  std::unique_ptr<AliveChecker> alive_checker_;

  // Mocks suspend status. Set in CreatePowerObserverHelper, owned by
  // |alive_checker_|.
  raw_ptr<MockPowerObserverHelper> mock_power_observer_helper_;

 private:
  void CreateAliveCheckerOnAliveCheckerThread(
      bool stop_at_first_alive_notification,
      bool pause_check_during_suspend,
      base::WaitableEvent* done) {
    EXPECT_TRUE(alive_checker_thread_.task_runner()->BelongsToCurrentThread());

    if (pause_check_during_suspend) {
      alive_checker_ = std::make_unique<AliveChecker>(
          base::BindRepeating(&AliveCheckerTest::OnDetectedDead,
                              base::Unretained(this)),
          base::Milliseconds(kCheckIntervalMs), base::Milliseconds(kTimeoutMs),
          stop_at_first_alive_notification,
          base::BindOnce(&AliveCheckerTest::CreatePowerObserverHelper,
                         base::Unretained(this)));
    } else {
      alive_checker_ = std::make_unique<AliveChecker>(
          base::BindRepeating(&AliveCheckerTest::OnDetectedDead,
                              base::Unretained(this)),
          base::Milliseconds(kCheckIntervalMs), base::Milliseconds(kTimeoutMs),
          stop_at_first_alive_notification,
          /*pause_check_during_suspend=*/false);
    }

    done->Signal();
  }

  void GetDetectedDeadOnAliveCheckerThread(bool* detected_dead,
                                           base::WaitableEvent* done) {
    EXPECT_TRUE(alive_checker_thread_.task_runner()->BelongsToCurrentThread());
    *detected_dead = alive_checker_->DetectedDead();
    done->Signal();
  }

  void ResetAliveCheckerOnAliveCheckerThread(base::WaitableEvent* done) {
    EXPECT_TRUE(alive_checker_thread_.task_runner()->BelongsToCurrentThread());
    mock_power_observer_helper_ = nullptr;
    alive_checker_.reset();
    done->Signal();
  }

  // Event to signal that we got a dead detection callback.
  base::WaitableEvent detected_dead_event_;
};

// Start the checker, don't send alive notifications, and run until it detects
// dead. Verify that it only detects once.
TEST_F(AliveCheckerTest, NoAliveNotificationsDetectOnce) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/false,
                     /*pause_check_during_suspend=*/false);

  StartAliveChecker();
  EXPECT_FALSE(GetDetectedDead());

  WaitUntilDetectedDead();
  EXPECT_TRUE(GetDetectedDead());

  // Verify that AliveChecker doesn't detect (runs the callback) a second time.
  // It can take up to the timeout + the check interval until detection. Add a
  // margin to this. The detect state should still be that we have detected
  // dead.
  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_TRUE(GetDetectedDead());
}

// Setup the checker to stop at first alive notification. Start it and notify
// that the client is alive once. Verify that we get no dead detection.
TEST_F(AliveCheckerTest, StopAtFirstAliveNotification_DoNotify) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/true,
                     /*pause_check_during_suspend=*/false);

  StartAliveChecker();
  alive_checker_->NotifyAlive();

  // It can take up to the timeout + the check interval until detection. Add a
  // margin to this.
  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_FALSE(GetDetectedDead());
}

// Setup the checker to stop at first alive notification. Start it and run until
// it detects dead.
TEST_F(AliveCheckerTest, StopAtFirstAliveNotification_DontNotify) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/true,
                     /*pause_check_during_suspend=*/false);
  StartAliveChecker();
  WaitUntilDetectedDead();
  EXPECT_TRUE(GetDetectedDead());
}

// Setup the checker to pause checking when suspended. Suspend and verify that
// it doesn't detect dead. Start the checker, don't send alive notifications,
// and and verify that it doesn't detect dead. Resume and run until it detects
// dead.
TEST_F(AliveCheckerTest, SuspendResume_StartBetweenSuspendAndResume) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/false,
                     /*pause_check_during_suspend=*/true);
  ASSERT_TRUE(mock_power_observer_helper_);

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Suspend,
                                base::Unretained(mock_power_observer_helper_)));

  StartAliveChecker();

  // It can take up to the timeout + the check interval until detection. Add a
  // margin to this.
  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_FALSE(GetDetectedDead());

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Resume,
                                base::Unretained(mock_power_observer_helper_)));

  WaitUntilDetectedDead();
  EXPECT_TRUE(GetDetectedDead());
}

// Setup the checker to stop at first alive notification and pause checking when
// suspended. Start the checker, send one alive notifications, and verify it
// doesn't detect dead. Suspend and verify that it doesn't detect dead. Resume
// and and verify that it doesn't detect dead.
TEST_F(AliveCheckerTest, SuspendResumeWithAutoStop_NotifyBeforeSuspend) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/true,
                     /*pause_check_during_suspend=*/true);
  ASSERT_TRUE(mock_power_observer_helper_);

  StartAliveChecker();
  alive_checker_->NotifyAlive();

  // It can take up to the timeout + the check interval until detection. Add a
  // margin to this.
  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_FALSE(GetDetectedDead());

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Suspend,
                                base::Unretained(mock_power_observer_helper_)));

  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_FALSE(GetDetectedDead());

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Resume,
                                base::Unretained(mock_power_observer_helper_)));

  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_FALSE(GetDetectedDead());
}

// Setup the checker to stop at first alive notification and pause checking when
// suspended. Start the checker, suspend. Send one alive notification and
// verify it doesn't detected dead. Resume and verify it doesn't detected dead.
TEST_F(AliveCheckerTest,
       SuspendResumeWithAutoStop_NotifyBetweenSuspendAndResume) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/true,
                     /*pause_check_during_suspend=*/true);
  ASSERT_TRUE(mock_power_observer_helper_);

  StartAliveChecker();

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Suspend,
                                base::Unretained(mock_power_observer_helper_)));

  alive_checker_->NotifyAlive();

  // It can take up to the timeout + the check interval until detection. Add a
  // margin to this.
  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_FALSE(GetDetectedDead());

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Resume,
                                base::Unretained(mock_power_observer_helper_)));

  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_FALSE(GetDetectedDead());
}

// Setup the checker to stop at first alive notification and pause checking when
// suspended. Start the checker, suspend, resume, send one alive notification
// and verify it doesn't detected dead.
TEST_F(AliveCheckerTest, SuspendResumeWithAutoStop_NotifyAfterResume) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/true,
                     /*pause_check_during_suspend=*/true);
  ASSERT_TRUE(mock_power_observer_helper_);

  StartAliveChecker();

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Suspend,
                                base::Unretained(mock_power_observer_helper_)));

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Resume,
                                base::Unretained(mock_power_observer_helper_)));

  alive_checker_->NotifyAlive();

  // It can take up to the timeout + the check interval until detection. Add a
  // margin to this.
  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_FALSE(GetDetectedDead());
}

// Setup the checker to stop at first alive notification and pause checking when
// suspended. Start the checker suspend, and and verify it doesn't detected
// dead. Resume and run until it detects dead.
TEST_F(AliveCheckerTest, SuspendResumeWithAutoStop_DontNotify) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/true,
                     /*pause_check_during_suspend=*/true);
  ASSERT_TRUE(mock_power_observer_helper_);

  StartAliveChecker();

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Suspend,
                                base::Unretained(mock_power_observer_helper_)));

  // It can take up to the timeout + the check interval until detection. Add a
  // margin to this.
  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(
      base::Milliseconds(kTimeoutMs + kCheckIntervalMs + 10)));
  EXPECT_FALSE(GetDetectedDead());

  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockPowerObserverHelper::Resume,
                                base::Unretained(mock_power_observer_helper_)));

  WaitUntilDetectedDead();
  EXPECT_TRUE(GetDetectedDead());
}

TEST_F(AliveCheckerTest, ContinuousNotifications_StayAliveThenDetectDead) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/false,
                     /*pause_check_during_suspend=*/false);

  StartAliveChecker();
  EXPECT_FALSE(GetDetectedDead());

  // Timeout is kTimeoutMs (50ms). Notify every 15ms for 6 times (75ms total),
  // which exceeds the 50ms timeout.
  NotifyAliveMultipleTimes(/*remaining_notifications=*/6,
                           base::Milliseconds(15));

  // Wait for 75ms; dead detection should not fire during active notifications.
  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(base::Milliseconds(75)));
  EXPECT_FALSE(GetDetectedDead());

  // Now stop notifying and wait for dead detection to occur.
  WaitUntilDetectedDead();
  EXPECT_TRUE(GetDetectedDead());
}

TEST_F(AliveCheckerTest,
       NotifyAliveWhileTaskRunnerBlockedPreventsDeadDetection) {
  CreateAliveChecker(/*stop_at_first_alive_notification=*/false,
                     /*pause_check_during_suspend=*/false);

  StartAliveChecker();
  EXPECT_FALSE(GetDetectedDead());

  base::WaitableEvent thread_blocked(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent unblock_thread(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Block the task runner deterministically.
  alive_checker_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WaitableEvent* thread_blocked,
                        base::WaitableEvent* unblock_thread) {
                       thread_blocked->Signal();
                       unblock_thread->Wait();
                     },
                     &thread_blocked, &unblock_thread));

  // Wait until the task runner is actively blocked.
  thread_blocked.Wait();

  // Sleep 60ms (exceeding 50ms timeout) while thread remains blocked.
  base::PlatformThread::Sleep(base::Milliseconds(60));

  // Call NotifyAlive() while thread is blocked. Updates atomics synchronously.
  alive_checker_->NotifyAlive();

  // Unblock the worker thread.
  unblock_thread.Signal();

  // Overdue check runs immediately. Since NotifyAlive() was called <50ms ago,
  // dead detection must not trigger.
  EXPECT_FALSE(WaitUntilDetectedDeadWithTimeout(base::Milliseconds(15)));
  EXPECT_FALSE(GetDetectedDead());

  // Wait for timeout to expire without further notifications.
  WaitUntilDetectedDead();
  EXPECT_TRUE(GetDetectedDead());
}

}  // namespace media
