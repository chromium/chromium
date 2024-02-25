// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/timer.h"

#include <memory>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class TimerPerfTest : public testing::Test {
 public:
  void NopTask(TimerBase*) {}

  void RecordStartRunTime(TimerBase*) { run_start_ = base::ThreadTicks::Now(); }

  void RecordEndRunTime(TimerBase*) {
    run_end_ = base::ThreadTicks::Now();
    loop_.Quit();
  }

  void Run() { loop_.Run(); }

  test::TaskEnvironment task_environment_;
  base::ThreadTicks run_start_;
  base::ThreadTicks run_end_;
  base::RunLoop loop_;
};

TEST_F(TimerPerfTest, PostAndRunTimers) {
  const int kNumIterations = 10000;
  Vector<std::unique_ptr<TaskRunnerTimer<TimerPerfTest>>> timers(
      kNumIterations);
  for (int i = 0; i < kNumIterations; i++) {
    timers[i] = std::make_unique<TaskRunnerTimer<TimerPerfTest>>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), this,
        &TimerPerfTest::NopTask);
  }

  TaskRunnerTimer<TimerPerfTest> measure_run_start(
      scheduler::GetSingleThreadTaskRunnerForTesting(), this,
      &TimerPerfTest::RecordStartRunTime);
  TaskRunnerTimer<TimerPerfTest> measure_run_end(
      scheduler::GetSingleThreadTaskRunnerForTesting(), this,
      &TimerPerfTest::RecordEndRunTime);

  measure_run_start.StartOneShot(base::TimeDelta(), FROM_HERE);
  base::ThreadTicks post_start = base::ThreadTicks::Now();
  for (int i = 0; i < kNumIterations; i++) {
    timers[i]->StartOneShot(base::TimeDelta(), FROM_HERE);
  }
  base::ThreadTicks post_end = base::ThreadTicks::Now();
  measure_run_end.StartOneShot(base::TimeDelta(), FROM_HERE);

  Run();

  double posting_time = (post_end - post_start).InMicrosecondsF();
  double posting_time_us_per_call =
      posting_time / static_cast<double>(kNumIterations);
  LOG(INFO) << "TimerBase::startOneShot cost (us/call) "
            << posting_time_us_per_call << " (total " << posting_time << " us)";
  LOG(INFO) << "Time to run " << kNumIterations << " trivial tasks (us) "
            << (run_end_ - run_start_).InMicroseconds();
}

TEST_F(TimerPerfTest, PostThenCancelTenThousandTimers) {
  const int kNumIterations = 10000;
  Vector<std::unique_ptr<TaskRunnerTimer<TimerPerfTest>>> timers(
      kNumIterations);
  for (int i = 0; i < kNumIterations; i++) {
    timers[i] = std::make_unique<TaskRunnerTimer<TimerPerfTest>>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), this,
        &TimerPerfTest::NopTask);
  }

  TaskRunnerTimer<TimerPerfTest> measure_run_start(
      scheduler::GetSingleThreadTaskRunnerForTesting(), this,
      &TimerPerfTest::RecordStartRunTime);
  TaskRunnerTimer<TimerPerfTest> measure_run_end(
      scheduler::GetSingleThreadTaskRunnerForTesting(), this,
      &TimerPerfTest::RecordEndRunTime);

  measure_run_start.StartOneShot(base::TimeDelta(), FROM_HERE);
  base::ThreadTicks post_start = base::ThreadTicks::Now();
  for (int i = 0; i < kNumIterations; i++) {
    timers[i]->StartOneShot(base::TimeDelta(), FROM_HERE);
  }
  base::ThreadTicks post_end = base::ThreadTicks::Now();
  measure_run_end.StartOneShot(base::TimeDelta(), FROM_HERE);

  base::ThreadTicks cancel_start = base::ThreadTicks::Now();
  for (int i = 0; i < kNumIterations; i++) {
    timers[i]->Stop();
  }
  base::ThreadTicks cancel_end = base::ThreadTicks::Now();

  Run();

  double posting_time = (post_end - post_start).InMicrosecondsF();
  double posting_time_us_per_call =
      posting_time / static_cast<double>(kNumIterations);
  LOG(INFO) << "TimerBase::startOneShot cost (us/call) "
            << posting_time_us_per_call << " (total " << posting_time << " us)";

  double cancel_time = (cancel_end - cancel_start).InMicrosecondsF();
  double cancel_time_us_per_call =
      cancel_time / static_cast<double>(kNumIterations);
  LOG(INFO) << "TimerBase::stop cost (us/call) " << cancel_time_us_per_call
            << " (total " << cancel_time << " us)";
  LOG(INFO) << "Time to run " << kNumIterations << " canceled tasks (us) "
            << (run_end_ - run_start_).InMicroseconds();
}

}  // namespace blink
