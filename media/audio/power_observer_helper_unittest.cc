// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/audio/power_observer_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class PowerObserverHelperTest : public testing::Test {
 public:
  PowerObserverHelperTest()
      : power_observer_helper_thread_("AliveCheckerThread"),
        suspend_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                       base::WaitableEvent::InitialState::NOT_SIGNALED),
        resume_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED) {
    power_observer_helper_thread_.StartAndWaitForTesting();
  }

  PowerObserverHelperTest(const PowerObserverHelperTest&) = delete;
  PowerObserverHelperTest& operator=(const PowerObserverHelperTest&) = delete;

  void OnSuspend() {
    EXPECT_TRUE(
        power_observer_helper_thread_.task_runner()->BelongsToCurrentThread());
    suspend_event_.Signal();
  }

  void OnResume() {
    EXPECT_TRUE(
        power_observer_helper_thread_.task_runner()->BelongsToCurrentThread());
    resume_event_.Signal();
  }

 protected:
  ~PowerObserverHelperTest() override {
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    power_observer_helper_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PowerObserverHelperTest::
                           ResetPowerObserverHelperOnPowerObserverHelperThread,
                       base::Unretained(this), &done));
    done.Wait();
  }

  void CreatePowerObserverHelper() {
    DCHECK(!power_observer_helper_);
    power_observer_helper_ = std::make_unique<PowerObserverHelper>(
        power_observer_helper_thread_.task_runner(),
        base::BindRepeating(&PowerObserverHelperTest::OnSuspend,
                            base::Unretained(this)),
        base::BindRepeating(&PowerObserverHelperTest::OnResume,
                            base::Unretained(this)));
  }

  void WaitUntilSuspendNotification() {
    suspend_event_.Wait();
    suspend_event_.Reset();
  }

  void WaitUntilResumeNotification() {
    resume_event_.Wait();
    resume_event_.Reset();
  }

  bool IsSuspending() {
    bool is_suspending = false;
    base::WaitableEvent did_check(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    power_observer_helper_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PowerObserverHelperTest::CheckIfSuspending,
                       base::Unretained(this), &is_suspending, &did_check));
    did_check.Wait();
    return is_suspending;
  }

  PowerObserverHelper* power_observer_helper() const {
    return power_observer_helper_.get();
  }

 private:
  void CheckIfSuspending(bool* is_suspending, base::WaitableEvent* done) {
    EXPECT_TRUE(
        power_observer_helper_thread_.task_runner()->BelongsToCurrentThread());
    *is_suspending = power_observer_helper_->IsSuspending();
    done->Signal();
  }

  void ResetPowerObserverHelperOnPowerObserverHelperThread(
      base::WaitableEvent* done) {
    EXPECT_TRUE(
        power_observer_helper_thread_.task_runner()->BelongsToCurrentThread());
    power_observer_helper_.reset();
    done->Signal();
  }

  // The test task environment.
  base::test::TaskEnvironment task_environment_;

  // The thread the helper is run on.
  base::Thread power_observer_helper_thread_;

  // PowerObserverHelper under test.
  std::unique_ptr<PowerObserverHelper> power_observer_helper_;

  // Events to signal a notifications.
  base::WaitableEvent suspend_event_;
  base::WaitableEvent resume_event_;
};

// Suspend and resume notifications.
TEST_F(PowerObserverHelperTest, SuspendAndResumeNotificationsTwice) {
  CreatePowerObserverHelper();
  EXPECT_FALSE(IsSuspending());

  power_observer_helper()->OnSuspend();
  WaitUntilSuspendNotification();
  EXPECT_TRUE(IsSuspending());

  power_observer_helper()->OnResume();
  WaitUntilResumeNotification();
  EXPECT_FALSE(IsSuspending());

  power_observer_helper()->OnSuspend();
  WaitUntilSuspendNotification();
  EXPECT_TRUE(IsSuspending());

  power_observer_helper()->OnResume();
  WaitUntilResumeNotification();
  EXPECT_FALSE(IsSuspending());
}

// Two suspend and two resume notifications.
TEST_F(PowerObserverHelperTest, TwoSuspendAndTwoResumeNotifications) {
  CreatePowerObserverHelper();
  EXPECT_FALSE(IsSuspending());

  power_observer_helper()->OnSuspend();
  WaitUntilSuspendNotification();
  EXPECT_TRUE(IsSuspending());

  power_observer_helper()->OnSuspend();
  WaitUntilSuspendNotification();
  EXPECT_TRUE(IsSuspending());

  power_observer_helper()->OnResume();
  WaitUntilResumeNotification();
  EXPECT_FALSE(IsSuspending());

  power_observer_helper()->OnResume();
  WaitUntilResumeNotification();
  EXPECT_FALSE(IsSuspending());
}

}  // namespace media
