// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/renderer/platform/scheduler/test/test_queueing_time_estimator_client.h"

namespace blink {
namespace scheduler {

namespace {

static constexpr base::TimeDelta kTimeLimit =
    base::TimeDelta::FromMilliseconds(2000);
static const int kWarmupRuns = 50;
static const int kCheckInterval = 100;

class QueueingTimeEstimatorTestPerfTest : public testing::Test {
 public:
  QueueingTimeEstimatorTestPerfTest()
      : timer_(kWarmupRuns, kTimeLimit, kCheckInterval) {}
  base::LapTimer timer_;
  base::TimeTicks time;
  TestQueueingTimeEstimatorClient client;
};

}  // namespace

TEST_F(QueueingTimeEstimatorTestPerfTest, DISABLED_ManyTasks) {
  const int num_tests = 3;
  base::TimeDelta task_lengths[num_tests] = {
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromMilliseconds(50),
      base::TimeDelta::FromMilliseconds(1)};
  std::string test_descriptions[num_tests] = {"OneSecondTasks", "FiftyMsTasks",
                                              "OneMsTasks"};
  for (int i = 0; i < num_tests; ++i) {
    QueueingTimeEstimatorForTest estimator(
        &client, base::TimeDelta::FromSeconds(1), 20, time);
    time += base::TimeDelta::FromSeconds(1);
    timer_.Reset();
    do {
      estimator.OnExecutionStarted(time);
      time += task_lengths[i];
      estimator.OnExecutionStopped(time);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());
    perf_test::PrintResult("QueueingTimeEstimatorTestPerfTest", "ManyTasks",
                           test_descriptions[i], timer_.LapsPerSecond(),
                           "tasks/s", true);
  }
}

}  // namespace scheduler
}  // namespace blink
