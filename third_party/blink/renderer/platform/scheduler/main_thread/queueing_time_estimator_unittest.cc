// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/queueing_time_estimator.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/test/test_queueing_time_estimator_client.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace scheduler {

namespace {

struct BucketExpectation {
  int sample;
  int count;
};

class QueueingTimeEstimatorTest : public testing::Test {
 protected:
  static Vector<BucketExpectation> GetFineGrained(
      const Vector<BucketExpectation>& expected) {
    Vector<BucketExpectation> fine_grained(expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      fine_grained[i].sample = expected[i].sample * 1000;
      fine_grained[i].count = expected[i].count;
    }
    return fine_grained;
  }

  void TestHistogram(const std::string& name,
                     int total,
                     const Vector<BucketExpectation>& expectations) {
    histogram_tester.ExpectTotalCount(name, total);
    int sum = 0;
    for (const auto& expected : expectations) {
      histogram_tester.ExpectBucketCount(name, expected.sample, expected.count);
      sum += expected.count;
    }
    EXPECT_EQ(total, sum);
  }

  HistogramTester histogram_tester;
  base::TimeTicks time;
  TestQueueingTimeEstimatorClient client;
};

}  // namespace

// Three tasks of one second each, all within a 5 second window. Expected
// queueing time is the probability of falling into one of these tasks (3/5),
// multiplied by the expected queueing time within a task (0.5 seconds). Thus we
// expect a queueing time of 0.3 seconds.
TEST_F(QueueingTimeEstimatorTest, AllTasksWithinWindow) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 1, time);
  for (int i = 0; i < 3; ++i) {
    estimator.OnExecutionStarted(time);
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnExecutionStopped(time);
  }

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(300)));
  Vector<BucketExpectation> expected = {{300, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 1, expected);
  Vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration3", 1,
                fine_grained);
}

// One 20 second long task, starting 3 seconds into the first window.
// Window 1: Probability of being within task = 2/5. Expected delay within task:
// avg(20, 18). Total expected queueing time = 7.6s.
// Window 2: Probability of being within task = 1. Expected delay within task:
// avg(18, 13). Total expected queueing time = 15.5s.
// Window 5: Probability of being within task = 3/5. Expected delay within task:
// avg(3, 0). Total expected queueing time = 0.9s.
TEST_F(QueueingTimeEstimatorTest, MultiWindowTask) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 1, time);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(3000);

  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(20000);
  estimator.OnExecutionStopped(time);

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(7600),
                                   base::TimeDelta::FromMilliseconds(15500),
                                   base::TimeDelta::FromMilliseconds(10500),
                                   base::TimeDelta::FromMilliseconds(5500),
                                   base::TimeDelta::FromMilliseconds(900)));
  Vector<BucketExpectation> expected = {
      {900, 1}, {5500, 1}, {7600, 1}, {10500, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 5, expected);
  // Split here is different: only 7600 and 10500 get grouped up.
  Vector<BucketExpectation> fine_grained = {
      {900 * 1000, 1}, {5500 * 1000, 1}, {7600 * 1000, 2}, {15500 * 1000, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration3", 5,
                fine_grained);
}

// If a task is too long, we assume it's invalid. Perhaps the user's machine
// went to sleep during a task, resulting in an extremely long task. Ignore
// these long tasks completely.
TEST_F(QueueingTimeEstimatorTest, IgnoreExtremelyLongTasks) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 1, time);
  time += base::TimeDelta::FromMilliseconds(5000);
  // Start with a 1 second task.
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(4000);

  // Now perform an invalid task. This will cause the windows involving this
  // task to be ignored.
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(35000);
  estimator.OnExecutionStopped(time);

  // Perform another 1 second task.
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  // Add a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  // Now perform another invalid task. This will cause the windows involving
  // this task to be ignored. Therefore, the previous task is ignored.
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(35000);
  estimator.OnExecutionStopped(time);

  // Flush by adding a task.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(100),
                                   base::TimeDelta::FromMilliseconds(100)));
  Vector<BucketExpectation> expected = {{100, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  Vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration3", 2,
                fine_grained);
}

// If we idle for too long, ignore idling time, even if the estimator is
// enabled. Perhaps the user's machine went to sleep while we were idling.
TEST_F(QueueingTimeEstimatorTest, IgnoreExtremelyLongIdlePeriods) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 1, time);
  time += base::TimeDelta::FromMilliseconds(5000);
  // Start with a 1 second task.
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(4000);
  // Dummy task to ensure this window is reported.
  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  // Now go idle for long. This will cause the windows involving this
  // time to be ignored.
  time += base::TimeDelta::FromMilliseconds(35000);

  // Perform another 1 second task.
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  // Add a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  // Now go idle again. This will cause the windows involving this idle period
  // to be ignored. Therefore, the previous task is ignored.
  time += base::TimeDelta::FromMilliseconds(35000);

  // Flush by adding a task.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(100),
                                   base::TimeDelta::FromMilliseconds(100)));
  Vector<BucketExpectation> expected = {{100, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  Vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration3", 2,
                fine_grained);
}

// ^ Instantaneous queuing time
// |
// |
// |   |\                                          .
// |   |  \                                        .
// |   |    \                                      .
// |   |      \                                    .
// |   |        \             |                    .
// ------------------------------------------------> Time
//     |s|s|s|s|s|
//     |---win---|
//       |---win---|
//         |---win---|
TEST_F(QueueingTimeEstimatorTest, SlidingWindowOverOneTask) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 5, time);
  time += base::TimeDelta::FromMilliseconds(1000);

  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  Vector<base::TimeDelta> expected_durations = {
      base::TimeDelta::FromMilliseconds(900),
      base::TimeDelta::FromMilliseconds(1600),
      base::TimeDelta::FromMilliseconds(2100),
      base::TimeDelta::FromMilliseconds(2400),
      base::TimeDelta::FromMilliseconds(2500),
      base::TimeDelta::FromMilliseconds(1600),
      base::TimeDelta::FromMilliseconds(900),
      base::TimeDelta::FromMilliseconds(400),
      base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0)};
  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAreArray(expected_durations));
  // UMA reported only on disjoint windows.
  Vector<BucketExpectation> expected = {{0, 1}, {2500, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
}

// ^ Instantaneous queuing time
// |
// |
// |   |\                                            .
// |   | \                                           .
// |   |  \                                          .
// |   |   \  |\                                     .
// |   |    \ | \           |                        .
// ------------------------------------------------> Time
//     |s|s|s|s|s|
//     |---win---|
//       |---win---|
//         |---win---|
TEST_F(QueueingTimeEstimatorTest, SlidingWindowOverTwoTasksWithinFirstWindow) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 5, time);
  time += base::TimeDelta::FromMilliseconds(1000);

  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(500);

  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  Vector<base::TimeDelta> expected_durations = {
      base::TimeDelta::FromMilliseconds(400),
      base::TimeDelta::FromMilliseconds(600),
      base::TimeDelta::FromMilliseconds(625),
      base::TimeDelta::FromMilliseconds(725),
      base::TimeDelta::FromMilliseconds(725),
      base::TimeDelta::FromMilliseconds(325),
      base::TimeDelta::FromMilliseconds(125),
      base::TimeDelta::FromMilliseconds(100),
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0)};
  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAreArray(expected_durations));
  Vector<BucketExpectation> expected = {{0, 1}, {725, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  Vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration3", 2,
                fine_grained);
}

// ^ Instantaneous queuing time
// |
// |
// |           |\                                 .
// |           | \                                .
// |           |  \                               .
// |           |   \ |\                           .
// |           |    \| \           |              .
// ------------------------------------------------> Time
//     |s|s|s|s|s|
//     |---win---|
//       |---win---|
//         |---win---|
TEST_F(QueueingTimeEstimatorTest,
       SlidingWindowOverTwoTasksSpanningSeveralWindows) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 5, time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(4000);

  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnExecutionStopped(time);

  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  Vector<base::TimeDelta> expected_durations = {
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(0),
      base::TimeDelta::FromMilliseconds(400),
      base::TimeDelta::FromMilliseconds(600),
      base::TimeDelta::FromMilliseconds(700),
      base::TimeDelta::FromMilliseconds(725),
      base::TimeDelta::FromMilliseconds(725),
      base::TimeDelta::FromMilliseconds(325),
      base::TimeDelta::FromMilliseconds(125),
      base::TimeDelta::FromMilliseconds(25),
      base::TimeDelta::FromMilliseconds(0)};

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAreArray(expected_durations));
  Vector<BucketExpectation> expected = {{325, 1}, {400, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  // The two values get grouped under the same bucket in the microsecond
  // version.
  expected = {{325 * 1000, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration3", 2, expected);
}

// There are multiple windows, but some of the EQTs are not reported due to
// enabling/disabling. EQT(win1) = 0. EQT(win3) = (1500+500)/2 = 1000.
// EQT(win4) = 1/2*500/2 = 250. EQT(win7) = 1/5*200/2 = 20.
TEST_F(QueueingTimeEstimatorTest, DisabledEQTsWithSingleStepPerWindow) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(1), 1, time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(1001);

  // Second window should not be reported.
  estimator.OnRecordingStateChanged(true, time);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(456);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnRecordingStateChanged(false, time);
  time += base::TimeDelta::FromMilliseconds(343);

  // Third, fourth windows should be reported
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(1500);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(501);

  // Fifth, sixth task should not be reported
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnExecutionStopped(time);
  estimator.OnRecordingStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnRecordingStateChanged(false, time);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(999);

  // Seventh task should be reported.
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(1000),
                                   base::TimeDelta::FromMilliseconds(125),
                                   base::TimeDelta::FromMilliseconds(20)));
  Vector<BucketExpectation> expected = {{0, 1}, {20, 1}, {125, 1}, {1000, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 4, expected);
  Vector<BucketExpectation> fine_grained = GetFineGrained(expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration3", 4,
                fine_grained);
}

// We only ignore steps that contain some time span that is disabled. Thus a
// window could be made up of non-contiguous steps. The following are EQTs,
// with time deltas with respect to the end of the first, 0-time task:
// Win1: [0-1000]. EQT of step [0-1000]: 500/2*1/2 = 125. EQT(win1) = 125/5 =
// 25.
// Win2: [0-1000],[2000-3000]. EQT of [2000-3000]: (1000+200)/2*4/5 = 480.
// EQT(win2) = (125+480)/5 = 121.
// Win3: [0-1000],[2000-3000],[11000-12000]. EQT of [11000-12000]: 0. EQT(win3)
// = 121.
// Win4: [0-1000],[2000-3000],[11000-13000]. EQT of [12000-13000]:
// (1500+1400)/2*1/10 = 145. EQT(win4) = (125+480+0+145)/5 = 150.
// Win5: [0-1000],[2000-3000],[11000-14000]. EQT of [13000-14000]: (1400+400)/2
// = 900. EQT(win5) = (125+480+0+145+900)/5 = 330.
// Win6: [2000-3000],[11000-15000]. EQT of [14000-15000]: 400/2*2/5 = 80.
// EQT(win6) = (480+0+145+900+80)/5 = 321.
// Win7: [11000-16000]. EQT of [15000-16000]: (2500+1700)/2*4/5 = 1680.
// EQT(win7) = (0+145+900+80+1680)/5 = 561.
// Win8: [12000-17000]. EQT of [16000-17000]: (1700+700)/2 = 1200. EQT(win8) =
// (145+900+80+1680+1200)/5 = 801.
TEST_F(QueueingTimeEstimatorTest, DisabledEQTsWithMutipleStepsPerWindow) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 5, time);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  estimator.OnRecordingStateChanged(true, time);
  // This task should be ignored.
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnExecutionStopped(time);
  estimator.OnRecordingStateChanged(false, time);

  time += base::TimeDelta::FromMilliseconds(400);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnRecordingStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(2000);
  // These tasks should be ignored.
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnExecutionStopped(time);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(3400);
  estimator.OnExecutionStopped(time);
  estimator.OnRecordingStateChanged(false, time);

  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(1500);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnExecutionStarted(time);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnExecutionStopped(time);

  // Window with last step should not be reported.
  estimator.OnRecordingStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStarted(time);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(25),
                                   base::TimeDelta::FromMilliseconds(121),
                                   base::TimeDelta::FromMilliseconds(121),
                                   base::TimeDelta::FromMilliseconds(150),
                                   base::TimeDelta::FromMilliseconds(330),
                                   base::TimeDelta::FromMilliseconds(321),
                                   base::TimeDelta::FromMilliseconds(561),
                                   base::TimeDelta::FromMilliseconds(801)));
}

}  // namespace scheduler
}  // namespace blink
