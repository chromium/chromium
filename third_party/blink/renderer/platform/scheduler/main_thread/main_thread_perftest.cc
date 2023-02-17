// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/testing/scoped_scheduler_overrider.h"

// Too slow with TSAN.
#if !defined(THREAD_SANITIZER)

namespace blink {
namespace scheduler {
namespace {

constexpr char kMetricPrefix[] = "MainThreadSchedulerPerfTest.";
// Includes time to |PostTask()|.
constexpr char kTimePerTask[] = "time_per_task";
// Time to |PostTask()|.
constexpr char kTimePerPostTask[] = "time_per_post_task";
// |time_per_task| - |time_per_post_task|.
constexpr char kTimePerTaskRun[] = "time_per_task_run";

class MainThreadPerfTest : public testing::Test {
 public:
  MainThreadPerfTest() = default;
  MainThreadPerfTest(const MainThreadPerfTest&) = delete;
  MainThreadPerfTest& operator=(const MainThreadPerfTest&) = delete;
  ~MainThreadPerfTest() override = default;

  void SetUp() override {
    scheduler_ = std::make_unique<MainThreadSchedulerImpl>(
        base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
            base::MessagePump::Create(base::MessagePumpType::DEFAULT),
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetPrioritySettings(CreatePrioritySettings())
                .Build()));
    scheduler_overrider_ = std::make_unique<ScopedSchedulerOverrider>(
        scheduler_.get(), scheduler_->DefaultTaskRunner());
  }

  void TearDown() override { scheduler_->Shutdown(); }

 protected:
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  std::unique_ptr<ScopedSchedulerOverrider> scheduler_overrider_;
};

TEST_F(MainThreadPerfTest, PostTaskPerformance) {
#if DCHECK_IS_ON()
  const int kTaskCount = 100000;
#else
  const int kTaskCount = 1000;
#endif
  base::RunLoop run_loop;
  auto counter_closure =
      base::BarrierClosure(kTaskCount, run_loop.QuitClosure());

  base::TimeTicks before = base::TimeTicks::Now();
  for (int i = 0; i < kTaskCount; i++) {
    scheduler_->DefaultTaskRunner()->PostTask(FROM_HERE, counter_closure);
  }
  base::TimeTicks after_post_task = base::TimeTicks::Now();
  run_loop.Run();
  base::TimeTicks after = base::TimeTicks::Now();

  perf_test::PerfResultReporter reporter(kMetricPrefix,
                                         "main_thread_post_task");
  reporter.RegisterImportantMetric(kTimePerPostTask, "ns/iteration");
  reporter.RegisterImportantMetric(kTimePerTask, "ns/iteration");
  reporter.RegisterImportantMetric(kTimePerTaskRun, "ns/iteration");

  size_t ns_per_post_task = static_cast<size_t>(
      (after_post_task - before).InNanoseconds() / kTaskCount);
  reporter.AddResult(kTimePerPostTask, ns_per_post_task);

  size_t ns_per_iteration =
      static_cast<size_t>((after - before).InNanoseconds() / kTaskCount);
  reporter.AddResult(kTimePerTask, ns_per_iteration);

  size_t ns_per_task_iteration = static_cast<size_t>(
      (after - after_post_task).InNanoseconds() / kTaskCount);
  reporter.AddResult(kTimePerTaskRun, ns_per_task_iteration);
}

}  // namespace
}  // namespace scheduler
}  // namespace blink

#endif  // defined(THREAD_SANITIZER)
