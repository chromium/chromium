// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/queueing_time_estimator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/test_queueing_time_estimator_client.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"

namespace blink {
namespace scheduler {

namespace {

struct BucketExpectation {
  int sample;
  int count;
};

class QueueingTimeEstimatorTest : public testing::Test {
 protected:
  static std::vector<BucketExpectation> GetFineGrained(
      const std::vector<BucketExpectation>& expected) {
    std::vector<BucketExpectation> fine_grained(expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      fine_grained[i].sample = expected[i].sample * 1000;
      fine_grained[i].count = expected[i].count;
    }
    return fine_grained;
  }

  void TestHistogram(const std::string& name,
                     int total,
                     const std::vector<BucketExpectation>& expectations) {
    histogram_tester.ExpectTotalCount(name, total);
    int sum = 0;
    for (const auto& expected : expectations) {
      histogram_tester.ExpectBucketCount(name, expected.sample, expected.count);
      sum += expected.count;
    }
    EXPECT_EQ(total, sum);
  }

  void TestSplitSumsTotal(base::TimeDelta* expected_sums, int num_windows) {
    for (int window = 1; window < num_windows; ++window) {
      base::TimeDelta sum;
      // Add up the reported split EQTs for that window.
      for (const auto& entry : client.split_eqts())
        sum += entry.second[window - 1];
      // Divide sum by two because we're also adding the split by frame type.
      sum /= 2.0;
      // Compare the split sum and the reported EQT for the disjoint window.
      EXPECT_EQ(expected_sums[window - 1], sum);
      EXPECT_EQ(expected_sums[window - 1],
                client.expected_queueing_times()[5 * window - 1]);
    }
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
    estimator.OnExecutionStarted(time, nullptr);
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnExecutionStopped(time);
  }

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(300)));
  std::vector<BucketExpectation> expected = {{300, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 1, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
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
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(3000);

  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(20000);
  estimator.OnExecutionStopped(time);

  // Flush the data by adding a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(7600),
                                   base::TimeDelta::FromMilliseconds(15500),
                                   base::TimeDelta::FromMilliseconds(10500),
                                   base::TimeDelta::FromMilliseconds(5500),
                                   base::TimeDelta::FromMilliseconds(900)));
  std::vector<BucketExpectation> expected = {
      {900, 1}, {5500, 1}, {7600, 1}, {10500, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 5, expected);
  // Split here is different: only 7600 and 10500 get grouped up.
  std::vector<BucketExpectation> fine_grained = {
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
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(4000);

  // Now perform an invalid task. This will cause the windows involving this
  // task to be ignored.
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(35000);
  estimator.OnExecutionStopped(time);

  // Perform another 1 second task.
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  // Add a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  // Now perform another invalid task. This will cause the windows involving
  // this task to be ignored. Therefore, the previous task is ignored.
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(35000);
  estimator.OnExecutionStopped(time);

  // Flush by adding a task.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(100),
                                   base::TimeDelta::FromMilliseconds(100)));
  std::vector<BucketExpectation> expected = {{100, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
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
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(4000);
  // Dummy task to ensure this window is reported.
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  // Now go idle for long. This will cause the windows involving this
  // time to be ignored.
  time += base::TimeDelta::FromMilliseconds(35000);

  // Perform another 1 second task.
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  // Add a task in the next window.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  // Now go idle again. This will cause the windows involving this idle period
  // to be ignored. Therefore, the previous task is ignored.
  time += base::TimeDelta::FromMilliseconds(35000);

  // Flush by adding a task.
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(100),
                                   base::TimeDelta::FromMilliseconds(100)));
  std::vector<BucketExpectation> expected = {{100, 2}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
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

  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(5000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  std::vector<base::TimeDelta> expected_durations = {
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
  std::vector<BucketExpectation> expected = {{0, 1}, {2500, 1}};
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

  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(500);

  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  std::vector<base::TimeDelta> expected_durations = {
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
  std::vector<BucketExpectation> expected = {{0, 1}, {725, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 2, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
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
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(4000);

  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnExecutionStopped(time);

  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(6000);

  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  std::vector<base::TimeDelta> expected_durations = {
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
  std::vector<BucketExpectation> expected = {{325, 1}, {400, 1}};
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
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(1001);

  // Second window should not be reported.
  estimator.OnRecordingStateChanged(true, time);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(456);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnRecordingStateChanged(false, time);
  time += base::TimeDelta::FromMilliseconds(343);

  // Third, fourth windows should be reported
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1500);
  estimator.OnExecutionStopped(time);
  time += base::TimeDelta::FromMilliseconds(501);

  // Fifth, sixth task should not be reported
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnExecutionStopped(time);
  estimator.OnRecordingStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnRecordingStateChanged(false, time);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(999);

  // Seventh task should be reported.
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  EXPECT_THAT(client.expected_queueing_times(),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(1000),
                                   base::TimeDelta::FromMilliseconds(125),
                                   base::TimeDelta::FromMilliseconds(20)));
  std::vector<BucketExpectation> expected = {
      {0, 1}, {20, 1}, {125, 1}, {1000, 1}};
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 4, expected);
  std::vector<BucketExpectation> fine_grained = GetFineGrained(expected);
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
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(500);
  estimator.OnExecutionStopped(time);

  estimator.OnRecordingStateChanged(true, time);
  // This task should be ignored.
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnExecutionStopped(time);
  estimator.OnRecordingStateChanged(false, time);

  time += base::TimeDelta::FromMilliseconds(400);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnRecordingStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(2000);
  // These tasks should be ignored.
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnExecutionStopped(time);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(3400);
  estimator.OnExecutionStopped(time);
  estimator.OnRecordingStateChanged(false, time);

  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(1500);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(2500);
  estimator.OnExecutionStopped(time);

  // Window with last step should not be reported.
  estimator.OnRecordingStateChanged(true, time);
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStarted(time, nullptr);
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

// Split ExpectedQueueingTime only reports once per disjoint window. The
// following is a detailed explanation of EQT per window and task queue:
// Window 1: A 3000ms default queue task contributes 900 to that EQT.
// Window 2: After 3000ms, the first 2000ms from a 3000ms default task: 800 EQT
// for that.
// Window 3: The remaining 100 EQT for default type. Also 1000ms tasks (which
// contribute 100) for FrameLoading, FrameThrottleable, and Unthrottled.
// Window 4: 600 ms tasks (which contribute 36) for each of the buckets except
// other. Two 300 ms (each contributing 9) and one 200 ms tasks (contributes 4)
// for the other bucket.
TEST_F(QueueingTimeEstimatorTest, SplitEQTByTaskQueueType) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 5, time);
  time += base::TimeDelta::FromMilliseconds(5000);
  // Dummy task to initialize the estimator.
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);

  // Beginning of window 1.
  time += base::TimeDelta::FromMilliseconds(500);
  scoped_refptr<MainThreadTaskQueueForTest> default_queue(
      new MainThreadTaskQueueForTest(QueueType::kDefault));
  estimator.OnExecutionStarted(time, default_queue.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(1500);

  // Beginning of window 2.
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnExecutionStarted(time, default_queue.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  // 1000 ms after beginning of window 3.
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  scoped_refptr<MainThreadTaskQueueForTest> frame_loading_queue(
      new MainThreadTaskQueueForTest(QueueType::kFrameLoading));
  scoped_refptr<MainThreadTaskQueueForTest> frame_throttleable_queue(
      new MainThreadTaskQueueForTest(QueueType::kFrameThrottleable));
  scoped_refptr<MainThreadTaskQueueForTest> unthrottled_queue(
      new MainThreadTaskQueueForTest(QueueType::kUnthrottled));
  MainThreadTaskQueue* queues_for_thousand[] = {frame_loading_queue.get(),
                                                frame_throttleable_queue.get(),
                                                unthrottled_queue.get()};
  for (auto* queue : queues_for_thousand) {
    estimator.OnExecutionStarted(time, queue);
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnExecutionStopped(time);
  }

  // Beginning of window 4.
  scoped_refptr<MainThreadTaskQueueForTest> frame_pausable_queue(
      new MainThreadTaskQueueForTest(QueueType::kFramePausable));
  scoped_refptr<MainThreadTaskQueueForTest> compositor_queue(
      new MainThreadTaskQueueForTest(QueueType::kCompositor));
  MainThreadTaskQueue* queues_for_six_hundred[] = {
      default_queue.get(),
      frame_loading_queue.get(),
      frame_throttleable_queue.get(),
      frame_pausable_queue.get(),
      unthrottled_queue.get(),
      compositor_queue.get()};
  for (auto* queue : queues_for_six_hundred) {
    estimator.OnExecutionStarted(time, queue);
    time += base::TimeDelta::FromMilliseconds(600);
    estimator.OnExecutionStopped(time);
  }
  time += base::TimeDelta::FromMilliseconds(600);

  // The following task contributes to "Other" because kControl is not a
  // supported queue type.
  scoped_refptr<MainThreadTaskQueueForTest> control_queue(
      new MainThreadTaskQueueForTest(QueueType::kControl));
  estimator.OnExecutionStarted(time, control_queue.get());
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnExecutionStopped(time);

  // The following task contributes to "Other" because kTest is not a supported
  // queue type.
  scoped_refptr<MainThreadTaskQueueForTest> test_queue(
      new MainThreadTaskQueueForTest(QueueType::kTest));
  estimator.OnExecutionStarted(time, test_queue.get());
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnExecutionStopped(time);

  // The following task contributes to "Other" because there is no task queue.
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(200);
  estimator.OnExecutionStopped(time);

  // End of window 4. Now check the vectors per task queue type.
  EXPECT_THAT(client.QueueTypeValues(QueueType::kDefault),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(900),
                                   base::TimeDelta::FromMilliseconds(800),
                                   base::TimeDelta::FromMilliseconds(100),
                                   base::TimeDelta::FromMilliseconds(36)));
  // The 800 and 900 values get grouped into a single bucket.
  std::vector<BucketExpectation> expected = {{36, 1}, {100, 1}, {800, 2}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByTaskQueue2.Default", 4,
                GetFineGrained(expected));

  EXPECT_THAT(client.QueueTypeValues(QueueType::kFrameLoading),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(100),
                                   base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 2}, {36, 1}, {100, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueue2.FrameLoading", 4,
      GetFineGrained(expected));

  EXPECT_THAT(client.QueueTypeValues(QueueType::kFrameThrottleable),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(100),
                                   base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 2}, {36, 1}, {100, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueue2.FrameThrottleable", 4,
      GetFineGrained(expected));

  EXPECT_THAT(client.QueueTypeValues(QueueType::kFramePausable),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 3}, {36, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueue2.FramePausable", 4,
      GetFineGrained(expected));

  EXPECT_THAT(client.QueueTypeValues(QueueType::kUnthrottled),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(100),
                                   base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 2}, {36, 1}, {100, 1}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByTaskQueue2.Unthrottled", 4,
      GetFineGrained(expected));

  EXPECT_THAT(client.QueueTypeValues(QueueType::kCompositor),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(36)));
  expected = {{0, 3}, {36, 1}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByTaskQueue2.Compositor",
                4, GetFineGrained(expected));

  EXPECT_THAT(client.QueueTypeValues(QueueType::kOther),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(22)));
  expected = {{0, 3}, {22, 1}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByTaskQueue2.Other", 4,
                GetFineGrained(expected));

  // Check that the sum of split EQT equals the total EQT for each window.
  base::TimeDelta expected_sums[] = {base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(400),
                                     base::TimeDelta::FromMilliseconds(238)};
  EXPECT_THAT(client.FrameStatusValues(FrameStatus::kNone),
              testing::ElementsAreArray(expected_sums));
  expected = {{238, 1}, {400, 1}, {800, 1}, {900, 1}};
  // The 800 and 900 values end up grouped up in the fine-grained version.
  std::vector<BucketExpectation> fine_grained = {
      {238 * 1000, 1}, {400 * 1000, 1}, {800 * 1000, 2}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByFrameStatus2.Other", 4,
                fine_grained);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 4, expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration3", 4,
                fine_grained);
  TestSplitSumsTotal(expected_sums, 5);
}

// Split ExpectedQueueingTime only reports once per disjoint window. The
// following is a detailed explanation of EQT per window and frame type:
// Window 1: A 3000ms task in a background main frame contributes 900 to that
// EQT.
// Window 2: Two 2000ms tasks in a visible main frame: 400 each, total 800
// EQT.
// Window 3: 3000ms task in a visible main frame: 900 EQT for that type. Also,
// the first 2000ms from a 3000ms task in a background main frame: 800 EQT for
// that.
// Window 4: The remaining 100 EQT for background main frame. Also 1000ms
// tasks (which contribute 100) for kSameOriginVisible, kSameOriginHidden,
// and kCrossOriginVisible.
// Window 5: 400 ms tasks (which contribute 16) for each of the buckets except
// other. Two 300 ms (each contributing 9) and one 800 ms tasks (contributes
// 64) for the other bucket.
TEST_F(QueueingTimeEstimatorTest, SplitEQTByFrameStatus) {
  QueueingTimeEstimatorForTest estimator(
      &client, base::TimeDelta::FromSeconds(5), 5, time);
  time += base::TimeDelta::FromMilliseconds(5000);
  // Dummy task to initialize the estimator.
  estimator.OnExecutionStarted(time, nullptr);
  estimator.OnExecutionStopped(time);
  scoped_refptr<MainThreadTaskQueueForTest> queue1(
      new MainThreadTaskQueueForTest(QueueType::kTest));

  // Beginning of window 1.
  time += base::TimeDelta::FromMilliseconds(500);
  // Scheduler with frame type: MAIN_FRAME_BACKGROUND.
  std::unique_ptr<FakeFrameScheduler> frame1 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kMainFrame)
          .Build();
  queue1->SetFrameSchedulerForTest(frame1.get());
  estimator.OnExecutionStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(1500);
  // Beginning of window 2.
  // Scheduler with frame type: MAIN_FRAME_VISIBLE.
  std::unique_ptr<FakeFrameScheduler> frame2 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kMainFrame)
          .SetIsPageVisible(true)
          .SetIsFrameVisible(true)
          .Build();
  queue1->SetFrameSchedulerForTest(frame2.get());
  estimator.OnExecutionStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnExecutionStopped(time);

  scoped_refptr<MainThreadTaskQueueForTest> queue2(
      new MainThreadTaskQueueForTest(QueueType::kTest));
  queue2->SetFrameSchedulerForTest(frame2.get());
  time += base::TimeDelta::FromMilliseconds(1000);
  estimator.OnExecutionStarted(time, queue2.get());
  time += base::TimeDelta::FromMilliseconds(2000);
  estimator.OnExecutionStopped(time);

  // Beginning of window 3.
  // Scheduler with frame type: MAIN_FRAME_VISIBLE.
  std::unique_ptr<FakeFrameScheduler> frame3 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kMainFrame)
          .SetIsPageVisible(true)
          .SetIsFrameVisible(true)
          .SetIsExemptFromThrottling(true)
          .Build();
  queue1->SetFrameSchedulerForTest(frame3.get());
  estimator.OnExecutionStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  estimator.OnExecutionStopped(time);

  // Scheduler with frame type: MAIN_FRAME_BACKGROUND.
  std::unique_ptr<FakeFrameScheduler> frame4 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kMainFrame)
          .SetIsFrameVisible(true)
          .SetIsExemptFromThrottling(true)
          .Build();
  queue1->SetFrameSchedulerForTest(frame4.get());
  estimator.OnExecutionStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(3000);
  // 1000 ms after beginning of window 4.
  estimator.OnExecutionStopped(time);

  time += base::TimeDelta::FromMilliseconds(1000);
  // Scheduler with frame type: SAME_ORIGIN_VISIBLE.
  std::unique_ptr<FakeFrameScheduler> frame5 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .SetIsFrameVisible(true)
          .Build();
  // Scheduler with frame type: SAME_ORIGIN_HIDDEN.
  std::unique_ptr<FakeFrameScheduler> frame6 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .Build();
  // Scheduler with frame type: CROSS_ORIGIN_VISIBLE.
  std::unique_ptr<FakeFrameScheduler> frame7 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .SetIsFrameVisible(true)
          .SetIsCrossOrigin(true)
          .Build();
  FakeFrameScheduler* schedulers_for_thousand[] = {frame5.get(), frame6.get(),
                                                   frame7.get()};
  for (auto* scheduler : schedulers_for_thousand) {
    queue1->SetFrameSchedulerForTest(scheduler);
    estimator.OnExecutionStarted(time, queue1.get());
    time += base::TimeDelta::FromMilliseconds(1000);
    estimator.OnExecutionStopped(time);
  }

  // Beginning of window 5.
  // Scheduler with frame type: MAIN_FRAME_HIDDEN.
  std::unique_ptr<FakeFrameScheduler> frame8 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kMainFrame)
          .SetIsPageVisible(true)
          .Build();
  // Scheduler with frame type: SAME_ORIGIN_BACKGROUND.
  std::unique_ptr<FakeFrameScheduler> frame9 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kSubframe)
          .Build();
  // Scheduler with frame type: CROSS_ORIGIN_HIDDEN.
  std::unique_ptr<FakeFrameScheduler> frame10 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kSubframe)
          .SetIsPageVisible(true)
          .SetIsCrossOrigin(true)
          .Build();
  // Scheduler with frame type: CROSS_ORIGIN_BACKGROUND.
  std::unique_ptr<FakeFrameScheduler> frame11 =
      FakeFrameScheduler::Builder()
          .SetFrameType(FrameScheduler::FrameType::kSubframe)
          .SetIsCrossOrigin(true)
          .Build();
  // One scheduler per supported frame type, excluding "Other".
  FakeFrameScheduler* schedulers_for_four_hundred[] = {
      frame2.get(), frame1.get(), frame8.get(),  frame5.get(), frame6.get(),
      frame9.get(), frame7.get(), frame10.get(), frame11.get()};
  for (auto* scheduler : schedulers_for_four_hundred) {
    queue1->SetFrameSchedulerForTest(scheduler);
    estimator.OnExecutionStarted(time, queue1.get());
    time += base::TimeDelta::FromMilliseconds(400);
    estimator.OnExecutionStopped(time);
  }

  // The following tasks contribute to "Other" because there is no frame.
  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnExecutionStopped(time);

  queue1->DetachFromFrameScheduler();
  estimator.OnExecutionStarted(time, queue1.get());
  time += base::TimeDelta::FromMilliseconds(300);
  estimator.OnExecutionStopped(time);

  estimator.OnExecutionStarted(time, nullptr);
  time += base::TimeDelta::FromMilliseconds(800);
  estimator.OnExecutionStopped(time);

  // End of window 5. Now check the vectors per frame type.
  EXPECT_THAT(client.FrameStatusValues(FrameStatus::kMainFrameBackground),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(900),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(800),
                                   base::TimeDelta::FromMilliseconds(100),
                                   base::TimeDelta::FromMilliseconds(16)));
  std::vector<BucketExpectation> expected = {
      {0, 1}, {16, 1}, {100, 1}, {800, 2}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByFrameStatus2."
      "MainFrameBackground",
      5, GetFineGrained(expected));

  EXPECT_THAT(client.FrameStatusValues(FrameStatus::kMainFrameVisible),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(800),
                                   base::TimeDelta::FromMilliseconds(900),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(16)));
  expected = {{0, 2}, {16, 1}, {800, 2}};
  TestHistogram(
      "RendererScheduler.ExpectedQueueingTimeByFrameStatus2.MainFrameVisible",
      5, GetFineGrained(expected));

  struct FrameExpectation {
    FrameStatus frame_status;
    std::string name;
  };
  FrameExpectation three_expected[] = {
      {FrameStatus::kSameOriginVisible,
       "RendererScheduler.ExpectedQueueingTimeByFrameStatus2."
       "SameOriginVisible"},
      {FrameStatus::kSameOriginHidden,
       "RendererScheduler.ExpectedQueueingTimeByFrameStatus2.SameOriginHidden"},
      {FrameStatus::kCrossOriginVisible,
       "RendererScheduler.ExpectedQueueingTimeByFrameStatus2."
       "CrossOriginVisible"},
  };
  for (const auto& frame_expectation : three_expected) {
    EXPECT_THAT(client.FrameStatusValues(frame_expectation.frame_status),
                testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(100),
                                     base::TimeDelta::FromMilliseconds(16)));
    expected = {{0, 3}, {16, 1}, {100, 1}};
    TestHistogram(frame_expectation.name, 5, GetFineGrained(expected));
  }

  FrameExpectation more_expected[] = {
      {FrameStatus::kMainFrameHidden,
       "RendererScheduler.ExpectedQueueingTimeByFrameStatus2."
       "MainFrameHidden"},
      {FrameStatus::kSameOriginBackground,
       "RendererScheduler.ExpectedQueueingTimeByFrameStatus2."
       "SameOriginBackground"},
      {FrameStatus::kCrossOriginHidden,
       "RendererScheduler.ExpectedQueueingTimeByFrameStatus2."
       "CrossOriginHidden"},
      {FrameStatus::kCrossOriginBackground,
       "RendererScheduler.ExpectedQueueingTimeByFrameStatus2."
       "CrossOriginBackground"}};
  for (const auto& frame_expectation : more_expected) {
    EXPECT_THAT(client.FrameStatusValues(frame_expectation.frame_status),
                testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(0),
                                     base::TimeDelta::FromMilliseconds(16)));
    expected = {{0, 4}, {16, 1}};
    TestHistogram(frame_expectation.name, 5, GetFineGrained(expected));
  }

  EXPECT_THAT(client.FrameStatusValues(FrameStatus::kNone),
              testing::ElementsAre(base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(0),
                                   base::TimeDelta::FromMilliseconds(82)));
  expected = {{0, 4}, {82, 1}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByFrameStatus2.Other", 5,
                GetFineGrained(expected));

  // Check that the sum of split EQT equals the total EQT for each window.
  base::TimeDelta expected_sums[] = {base::TimeDelta::FromMilliseconds(900),
                                     base::TimeDelta::FromMilliseconds(800),
                                     base::TimeDelta::FromMilliseconds(1700),
                                     base::TimeDelta::FromMilliseconds(400),
                                     base::TimeDelta::FromMilliseconds(226)};
  EXPECT_THAT(client.QueueTypeValues(QueueType::kOther),
              testing::ElementsAreArray(expected_sums));
  expected = {{226, 1}, {400, 1}, {800, 1}, {900, 1}, {1700, 1}};
  std::vector<BucketExpectation> fine_grained = {
      {226 * 1000, 1}, {400 * 1000, 1}, {800 * 1000, 2}, {1700 * 1000, 1}};
  TestHistogram("RendererScheduler.ExpectedQueueingTimeByTaskQueue2.Other", 5,
                fine_grained);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration", 5, expected);
  TestHistogram("RendererScheduler.ExpectedTaskQueueingDuration3", 5,
                fine_grained);
  TestSplitSumsTotal(expected_sums, 6);
}

}  // namespace scheduler
}  // namespace blink
