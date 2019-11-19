// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_service_throttler.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "media/base/android/media_server_crash_listener.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// From base/media/android/media_service_throttler.cc
const int kMaxBurstClients = 10;

class MediaServiceThrottlerTest : public testing::Test {
 public:
  MediaServiceThrottlerTest() {
    throttler_ = MediaServiceThrottler::GetInstance();
    clock_.SetNowTicks(base::TimeTicks());
    throttler_->SetTickClockForTesting(&clock_);
    test_task_runner_ =
        base::MakeRefCounted<FakeSingleThreadTaskRunner>(&clock_);
    throttler_->ResetInternalStateForTesting();
    throttler_->SetCrashListenerTaskRunnerForTesting(test_task_runner_);
    base_delay_ = throttler_->GetBaseThrottlingRateForTesting();
  }

  void SimulateCrashes(int number_of_crashes) {
    for (int i = 0; i < number_of_crashes; ++i)
      throttler_->OnMediaServerCrash(false);
  }

  // Simulates the scheduling of |number_of_clients| and returns the last
  // scheduling delay.
  base::TimeDelta SimulateClientCreations(int number_of_clients) {
    for (int i = 0; i < number_of_clients - 1; ++i)
      throttler_->GetDelayForClientCreation();

    return throttler_->GetDelayForClientCreation();
  }

  base::TimeDelta GetCurrentDelayBetweenClients() {
    // Schedule two clients and return the difference between their scheduling
    // slots.
    return -(throttler_->GetDelayForClientCreation() -
             throttler_->GetDelayForClientCreation());
  }

  base::TimeTicks TestNow() { return clock_.NowTicks(); }

  MediaServiceThrottler* throttler_;
  base::SimpleTestTickClock clock_;

  base::TimeDelta base_delay_;

  scoped_refptr<FakeSingleThreadTaskRunner> test_task_runner_;

  // Necessary, or else base::ThreadTaskRunnerHandle::Get() fails.
  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaServiceThrottlerTest);
};

// Canary test case.
TEST_F(MediaServiceThrottlerTest, BaseCase) {
  EXPECT_EQ(base::TimeDelta(), throttler_->GetDelayForClientCreation());
}

// Makes sure we can "burst" schedule clients.
TEST_F(MediaServiceThrottlerTest, NoCrash_UnderBurstThreshold_ShouldNotDelay) {
  int number_burst_client_scheduled = 0;

  while (base::TimeDelta() == throttler_->GetDelayForClientCreation())
    number_burst_client_scheduled++;

  EXPECT_EQ(kMaxBurstClients, number_burst_client_scheduled);
}

// Makes sure that, when burst scheduling a client, we schedule it from
// last scheduled burst client (vs scheduling it |base_delay_| from now).
TEST_F(MediaServiceThrottlerTest, NoCrash_OverBurstThreshold_ShouldDelay) {
  // Schedule clients until the next one would be over burst threshold.
  SimulateClientCreations(kMaxBurstClients);

  // Make sure the next client that is scheduled is scheduled in its normal time
  // slot.
  EXPECT_EQ(base_delay_ * (kMaxBurstClients + 1),
            throttler_->GetDelayForClientCreation());
}

// Makes sure that clients are scheduled |base_delay| apart.
TEST_F(MediaServiceThrottlerTest,
       NoCrash_OverBurstThreshold_ShouldDelayLinearly) {
  // Schedule clients until the next one would be over burst threshold.
  SimulateClientCreations(kMaxBurstClients);

  // Delays between two clients should be |base_delay_| appart.
  EXPECT_EQ(base_delay_, GetCurrentDelayBetweenClients());

  // Delays should remain constant (GetCurrentDelayBetweenClients() is not
  // idempotent and actually schedules new clients).
  EXPECT_EQ(GetCurrentDelayBetweenClients(), GetCurrentDelayBetweenClients());
}

// Makes sure that for every |base_delay_| that has elapsed, we can burst
// schedule an extra client.
TEST_F(MediaServiceThrottlerTest,
       NoCrash_BurstThreshold_ShouldBeSlidingWindow) {
  // Schedule some clients below the burst threshold.
  SimulateClientCreations(7);

  clock_.Advance(base_delay_ * 5);

  // Make sure the passage of allows for more clients to be scheduled, since
  // 7 + 8 > kMaxBurstClients.
  EXPECT_EQ(base::TimeDelta(), SimulateClientCreations(8));
}

// Makes sure that, if not enough time has elapsed, we do not burst schedule
// new clients.
TEST_F(MediaServiceThrottlerTest,
       NoCrash_OverBurstThresholdWithTimeLapse_ShouldDelay) {
  // Schedule some clients way above the burst threshold.
  SimulateClientCreations(3 * kMaxBurstClients);

  // Advance the time so there are still 2 * kMaxBurstClients pending clients.
  clock_.Advance(base_delay_ * kMaxBurstClients);

  // Make sure delay we do not burst schedule new clients.
  EXPECT_NE(base::TimeDelta(), throttler_->GetDelayForClientCreation());
}

// Makes sure that after a certain amount of inactivity, the scheduling clock is
// reset.
TEST_F(MediaServiceThrottlerTest, NoCrash_LongInactivity_ShouldReset) {
  // Schedule two minute's worth of clients
  SimulateClientCreations(base::TimeDelta::FromMinutes(2) / base_delay_);

  // Advance the time so the scheduler perceived a full minute of inactivity.
  clock_.Advance(base::TimeDelta::FromSeconds(61));

  // Make sure new clients are burst scheduled.
  EXPECT_EQ(base::TimeDelta(), throttler_->GetDelayForClientCreation());
}

// Makes sure that crashes increase the scheduling delay.
TEST_F(MediaServiceThrottlerTest, WithCrash_BaseCase_DelayShouldIncrease) {
  // Schedule clients until the next one would be over burst threshold.
  SimulateClientCreations(kMaxBurstClients);

  base::TimeDelta no_crash_delay = GetCurrentDelayBetweenClients();

  SimulateCrashes(1);

  base::TimeDelta crash_delay = GetCurrentDelayBetweenClients();

  EXPECT_NE(base_delay_, crash_delay);
  EXPECT_GT(crash_delay, no_crash_delay);
}

// Makes sure that we tolerate 1 crash per minute.
TEST_F(MediaServiceThrottlerTest,
       WithCrash_SingleCrash_DelayShouldNotIncrease) {
  // Schedule clients until the next one would be over burst threshold.
  SimulateClientCreations(kMaxBurstClients);

  SimulateCrashes(1);
  clock_.Advance(base::TimeDelta::FromMilliseconds(1));

  // Because we use the floor function when calculating crashes, a small time
  // advance should nullify a single crash.
  EXPECT_EQ(base_delay_, GetCurrentDelayBetweenClients());
}

// Makes sure that more than 1 crash per minute causes increased delays.
TEST_F(MediaServiceThrottlerTest, WithCrash_ManyCrashes_DelayShouldIncrease) {
  // Schedule clients until the next one would be over burst threshold.
  SimulateClientCreations(kMaxBurstClients);

  SimulateCrashes(2);
  clock_.Advance(base::TimeDelta::FromMilliseconds(1));

  // The delay after crashes should be greater than the base delay.
  EXPECT_LT(base_delay_, GetCurrentDelayBetweenClients());
}

// Makes sure that an increase in server crashes leads to delay increases.
TEST_F(MediaServiceThrottlerTest,
       WithCrash_ConsecutiveCrashes_DelayShouldIncreaseWithNumberOfCrashes) {
  // Schedule clients until the next one would be over burst threshold.
  SimulateClientCreations(kMaxBurstClients);

  base::TimeDelta last_delay = base_delay_;

  for (int i = 0; i < 5; ++i) {
    SimulateCrashes(1);
    base::TimeDelta current_delay = GetCurrentDelayBetweenClients();
    EXPECT_LT(last_delay, current_delay);
    last_delay = current_delay;
  }
}

// Makes sure that crashes affect the number of burst clients we can schedule.
TEST_F(MediaServiceThrottlerTest, WithCrash_ShouldAllowFewerBurstClients) {
  int number_burst_client_scheduled = 0;

  SimulateCrashes(1);

  while (base::TimeDelta() == throttler_->GetDelayForClientCreation())
    number_burst_client_scheduled++;

  EXPECT_GT(kMaxBurstClients, number_burst_client_scheduled);
}

// Makes sure delays are capped to a certain maximal value.
TEST_F(MediaServiceThrottlerTest, WithCrash_ManyCrashes_DelayShouldMaxOut) {
  // Schedule clients until the next one would be over burst threshold.
  SimulateClientCreations(kMaxBurstClients);

  SimulateCrashes(10);
  base::TimeDelta capped_delay = GetCurrentDelayBetweenClients();

  SimulateCrashes(10);

  EXPECT_EQ(capped_delay, GetCurrentDelayBetweenClients());
}

// Makes sure a minute without crashes resets the crash counter.
TEST_F(MediaServiceThrottlerTest, WithCrash_NoCrashesForAMinute_ShouldReset) {
  SimulateCrashes(10);

  // The effective server crash count should be reset because it has been over
  // a minute since the last crash.
  clock_.Advance(base::TimeDelta::FromSeconds(61));

  SimulateClientCreations(kMaxBurstClients);

  EXPECT_EQ(base_delay_, GetCurrentDelayBetweenClients());
}

// Makes sure a steady crashes do not resets the crash counter.
TEST_F(MediaServiceThrottlerTest, WithCrash_ConstantCrashes_ShouldNotReset) {
  SimulateCrashes(9);

  // The effective server crash count should not be reset.
  clock_.Advance(base::TimeDelta::FromSeconds(59));
  SimulateCrashes(1);
  clock_.Advance(base::TimeDelta::FromSeconds(2));

  SimulateClientCreations(kMaxBurstClients);

  EXPECT_LT(base_delay_, GetCurrentDelayBetweenClients());
}

// Makes sure the crash listener shuts down after a minute of not having
// received any client creation request.
TEST_F(MediaServiceThrottlerTest, CrashListener_NoRequests_ShouldShutDown) {
  // Schedule many minutes worth of clients. This is to prove that the
  // MediaServerCrashListener's clean up happens after lack of requests, as
  // opposed to lack of actually scheduled clients.
  SimulateClientCreations(base::TimeDelta::FromMinutes(3) / base_delay_);

  // The MediaServerCrashListener should be alive, with 1s second to spare.
  clock_.Advance(base::TimeDelta::FromSeconds(59));
  test_task_runner_->RunTasks();
  EXPECT_TRUE(throttler_->IsCrashListenerAliveForTesting());

  // Requesting a new client creation should reset the interal timer, and
  // cancel the release request that was scheduled 59 seconds ago.
  throttler_->GetDelayForClientCreation();

  // The MediaServerCrashListener should be alive, with 58s second to spare.
  clock_.Advance(base::TimeDelta::FromSeconds(2));
  test_task_runner_->RunTasks();
  EXPECT_TRUE(throttler_->IsCrashListenerAliveForTesting());

  // The MediaServerCrashListener should be dead.
  clock_.Advance(base::TimeDelta::FromSeconds(59));
  test_task_runner_->RunTasks();
  EXPECT_FALSE(throttler_->IsCrashListenerAliveForTesting());
}

// Makes sure the crash listener shuts down after a minute of not having
// received any client creation request, regardless of when crashes occur.
TEST_F(MediaServiceThrottlerTest,
       CrashListener_NoRequestsWithCrashes_ShouldShutDown) {
  // Schedule many minutes worth of clients. This is to prove that the
  // MediaServerCrashListener's clean up happens after lack of requests, as
  // opposed to lack of actually scheduled clients.
  SimulateClientCreations(base::TimeDelta::FromMinutes(3) / base_delay_);

  // The MediaServerCrashListener should be alive, with 1s second to spare.
  clock_.Advance(base::TimeDelta::FromSeconds(59));
  test_task_runner_->RunTasks();
  EXPECT_TRUE(throttler_->IsCrashListenerAliveForTesting());

  // Crashes should not affect the MediaServerCrashListener's lifetime.
  SimulateCrashes(1);

  // The MediaServerCrashListener should be dead.
  clock_.Advance(base::TimeDelta::FromSeconds(2));
  test_task_runner_->RunTasks();
  EXPECT_FALSE(throttler_->IsCrashListenerAliveForTesting());
}

}  // namespace media
