// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "base/test/task_environment.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/task/current_thread.h"
#include "base/test/power_monitor_test_base.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {
constexpr auto kGpuWatchdogTimeoutForTesting =
    base::TimeDelta::FromMilliseconds(1000);

// This task will run for duration_ms milliseconds.
void SimpleTask(base::TimeDelta duration) {
  base::PlatformThread::Sleep(duration);
}
}  // namespace

class GpuWatchdogTest : public testing::Test {
 public:
  GpuWatchdogTest() {}

  void LongTaskWithReportProgress(base::TimeDelta duration,
                                  base::TimeDelta report_delta);

  void LongTaskFromBackgroundToForeground(
      base::TimeDelta duration,
      base::TimeDelta time_to_switch_to_foreground);

  // Implements testing::Test
  void SetUp() override;

 protected:
  ~GpuWatchdogTest() override = default;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop;
  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;
};

class GpuWatchdogPowerTest : public GpuWatchdogTest {
 public:
  GpuWatchdogPowerTest() {}

  void LongTaskOnResume(base::TimeDelta duration,
                        base::TimeDelta time_to_power_resume);

  // Implements testing::Test
  void SetUp() override;
  void TearDown() override;

 protected:
  ~GpuWatchdogPowerTest() override = default;
  base::PowerMonitorTestSource* power_monitor_source_ = nullptr;
};

void GpuWatchdogTest::SetUp() {
  ASSERT_TRUE(base::ThreadTaskRunnerHandle::IsSet());
  ASSERT_TRUE(base::CurrentThread::IsSet());

  // Set watchdog timeout to 1000 milliseconds
  watchdog_thread_ = gpu::GpuWatchdogThread::Create(
      /*start_backgrounded*/ false,
      /*timeout*/ kGpuWatchdogTimeoutForTesting,
      /*init_factor*/ kInitFactor,
      /*restart_factor*/ kRestartFactor,
      /*test_mode*/ true);
}

void GpuWatchdogPowerTest::SetUp() {
  GpuWatchdogTest::SetUp();

  // Report GPU init complete.
  watchdog_thread_->OnInitComplete();

  // Create a power monitor test source.
  auto power_monitor_source = std::make_unique<base::PowerMonitorTestSource>();
  power_monitor_source_ = power_monitor_source.get();
  base::PowerMonitor::Initialize(std::move(power_monitor_source));
  watchdog_thread_->AddPowerObserver();

  // Wait until the power observer is added on the watchdog thread
  watchdog_thread_->WaitForPowerObserverAddedForTesting();
}

void GpuWatchdogPowerTest::TearDown() {
  GpuWatchdogTest::TearDown();
  watchdog_thread_.reset();
  base::PowerMonitor::ShutdownForTesting();
}

// This task will run for duration_ms milliseconds. It will also call watchdog
// ReportProgress() every report_delta_ms milliseconds.
void GpuWatchdogTest::LongTaskWithReportProgress(base::TimeDelta duration,
                                                 base::TimeDelta report_delta) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks end;

  do {
    base::PlatformThread::Sleep(report_delta);
    watchdog_thread_->ReportProgress();
    end = base::TimeTicks::Now();
  } while (end - start <= duration);
}

void GpuWatchdogTest::LongTaskFromBackgroundToForeground(
    base::TimeDelta duration,
    base::TimeDelta time_to_switch_to_foreground) {
  // Chrome is running in the background first.
  watchdog_thread_->OnBackgrounded();
  base::PlatformThread::Sleep(time_to_switch_to_foreground);
  // Now switch Chrome to the foreground after the specified time
  watchdog_thread_->OnForegrounded();
  base::PlatformThread::Sleep(duration - time_to_switch_to_foreground);
}

void GpuWatchdogPowerTest::LongTaskOnResume(
    base::TimeDelta duration,
    base::TimeDelta time_to_power_resume) {
  // Stay in power suspension mode first.
  power_monitor_source_->GenerateSuspendEvent();

  base::PlatformThread::Sleep(time_to_power_resume);

  // Now wake up on power resume.
  power_monitor_source_->GenerateResumeEvent();
  // Continue the GPU task for the remaining time.
  base::PlatformThread::Sleep(duration - time_to_power_resume);
}

// GPU Hang In Initialization
TEST_F(GpuWatchdogTest, GpuInitializationHang) {
  // GPU init takes longer than timeout.
#if defined(OS_WIN)
  SimpleTask(
      kGpuWatchdogTimeoutForTesting * kInitFactor +
      kGpuWatchdogTimeoutForTesting * kMaxCountOfMoreGpuThreadTimeAllowed +
      base::TimeDelta::FromMilliseconds(3000));
#else
  SimpleTask(kGpuWatchdogTimeoutForTesting * kInitFactor +
             base::TimeDelta::FromMilliseconds(3000));
#endif

  // Gpu hangs. OnInitComplete() is not called

  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
}

// Normal GPU Initialization and Running Task
TEST_F(GpuWatchdogTest, GpuInitializationAndRunningTasks) {
  // Assume GPU initialization takes 300 milliseconds.
  SimpleTask(base::TimeDelta::FromMilliseconds(300));
  watchdog_thread_->OnInitComplete();

  // Start running GPU tasks. Watchdog function WillProcessTask(),
  // DidProcessTask() and ReportProgress() are tested.
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, base::TimeDelta::FromMilliseconds(500)));
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, base::TimeDelta::FromMilliseconds(500)));

  // This long task takes 3000 milliseconds to finish, longer than timeout.
  // But it reports progress every 500 milliseconds
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogTest::LongTaskWithReportProgress,
                                base::Unretained(this),
                                kGpuWatchdogTimeoutForTesting +
                                    base::TimeDelta::FromMilliseconds(2000),
                                base::TimeDelta::FromMilliseconds(500)));

  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // Everything should be fine. No GPU hang detected.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_FALSE(result);
}

// GPU Hang when running a task
TEST_F(GpuWatchdogTest, GpuRunningATaskHang) {
  // Report gpu init complete
  watchdog_thread_->OnInitComplete();

  // Start running a GPU task.
#if defined(OS_WIN)
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, kGpuWatchdogTimeoutForTesting * 2 +
                                      kGpuWatchdogTimeoutForTesting *
                                          kMaxCountOfMoreGpuThreadTimeAllowed +
                                      base::TimeDelta::FromMilliseconds(4000)));
#else
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, kGpuWatchdogTimeoutForTesting * 2 +
                                      base::TimeDelta::FromMilliseconds(4000)));
#endif

  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // This GPU task takes too long. A GPU hang should be detected.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
}

TEST_F(GpuWatchdogTest, ChromeInBackground) {
  // Chrome starts in the background.
  watchdog_thread_->OnBackgrounded();

  // Gpu init (3000 ms) takes longer than timeout (2000 ms).
  SimpleTask(kGpuWatchdogTimeoutForTesting * kInitFactor +
             base::TimeDelta::FromMilliseconds(1000));

  // Report GPU init complete.
  watchdog_thread_->OnInitComplete();

  // Run a task that takes longer (3000 milliseconds) than timeout.
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, kGpuWatchdogTimeoutForTesting * 2 +
                                      base::TimeDelta::FromMilliseconds(1000)));
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // The gpu might be slow when running in the background. This is ok.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_FALSE(result);
}

TEST_F(GpuWatchdogTest, GpuSwitchingToForegroundHang) {
  // Report GPU init complete.
  watchdog_thread_->OnInitComplete();

  // A task stays in the background for 200 milliseconds, and then
  // switches to the foreground and runs for 6000 milliseconds. This is longer
  // than the first-time foreground watchdog timeout (2000 ms).
#if defined(OS_WIN)
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogTest::LongTaskFromBackgroundToForeground,
                     base::Unretained(this),
                     /*duration*/ kGpuWatchdogTimeoutForTesting * 2 +
                         kGpuWatchdogTimeoutForTesting *
                             kMaxCountOfMoreGpuThreadTimeAllowed +
                         base::TimeDelta::FromMilliseconds(4200),
                     /*time_to_switch_to_foreground*/
                     base::TimeDelta::FromMilliseconds(200)));
#else
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogTest::LongTaskFromBackgroundToForeground,
                     base::Unretained(this),
                     /*duration*/ kGpuWatchdogTimeoutForTesting * 2 +
                         base::TimeDelta::FromMilliseconds(4200),
                     /*time_to_switch_to_foreground*/
                     base::TimeDelta::FromMilliseconds(200)));
#endif

  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // It takes too long to finish a task after switching to the foreground.
  // A GPU hang should be detected.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
}

TEST_F(GpuWatchdogTest, GpuInitializationPause) {
  // Running for 100 ms in the beginning of GPU init.
  SimpleTask(base::TimeDelta::FromMilliseconds(100));
  watchdog_thread_->PauseWatchdog();

  // The Gpu init continues for another (init timeout + 1000) ms after the pause
  SimpleTask(kGpuWatchdogTimeoutForTesting * kInitFactor +
             base::TimeDelta::FromMilliseconds(1000));

  // No GPU hang is detected when the watchdog is paused.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_FALSE(result);

  // Continue the watchdog now.
  watchdog_thread_->ResumeWatchdog();
  // The Gpu init continues for (init timeout + 4000) ms.
#if defined(OS_WIN)
  SimpleTask(
      kGpuWatchdogTimeoutForTesting * kInitFactor +
      kGpuWatchdogTimeoutForTesting * kMaxCountOfMoreGpuThreadTimeAllowed +
      base::TimeDelta::FromMilliseconds(4000));
#else
  SimpleTask(kGpuWatchdogTimeoutForTesting * kInitFactor +
             base::TimeDelta::FromMilliseconds(4000));
#endif

  // A GPU hang should be detected.
  result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
}

TEST_F(GpuWatchdogPowerTest, GpuOnSuspend) {
  // watchdog_thread_->OnInitComplete() is called in SetUp

  // Enter power suspension mode.
  power_monitor_source_->GenerateSuspendEvent();

  // Run a task that takes longer (5000 milliseconds) than timeout.
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, kGpuWatchdogTimeoutForTesting * 2 +
                                      base::TimeDelta::FromMilliseconds(3000)));
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // A task might take long time to finish after entering suspension mode.
  // This one is not a GPU hang.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_FALSE(result);
}

TEST_F(GpuWatchdogPowerTest, GpuOnResumeHang) {
  // watchdog_thread_->OnInitComplete() is called in SetUp

  // This task stays in the suspension mode for 200 milliseconds, and it
  // wakes up on power resume and then runs for 6000 milliseconds. This is
  // longer than the watchdog resume timeout (2000 ms).
#if defined(OS_WIN)
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuWatchdogPowerTest::LongTaskOnResume, base::Unretained(this),
          /*duration*/ kGpuWatchdogTimeoutForTesting * kRestartFactor +
              kGpuWatchdogTimeoutForTesting *
                  kMaxCountOfMoreGpuThreadTimeAllowed +
              base::TimeDelta::FromMilliseconds(4200),
          /*time_to_power_resume*/
          base::TimeDelta::FromMilliseconds(200)));
#else
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuWatchdogPowerTest::LongTaskOnResume, base::Unretained(this),
          /*duration*/ kGpuWatchdogTimeoutForTesting * kRestartFactor +
              base::TimeDelta::FromMilliseconds(4200),
          /*time_to_power_resume*/
          base::TimeDelta::FromMilliseconds(200)));
#endif

  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // It takes too long to finish this task after power resume. A GPU hang should
  // be detected.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
}

}  // namespace gpu
