// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_watchdog_thread.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/power_monitor_test.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace gpu {

namespace {
// |kExtraGPUJobTimeForTesting| is the extra time the gpu main/test thread
// spends after GpuWatchdogTimeout. Theoretically, any extra time such as 1 ms
// should be enough to trigger the watchdog kill. However, it can cause test
// flakiness when the time is too short.

constexpr auto kGpuWatchdogTimeoutForTesting = base::Milliseconds(120);
constexpr auto kExtraGPUJobTimeForTesting = base::Milliseconds(500);

// For slow machines like Win 7, Mac 10.xx and Android L/M/N.
[[maybe_unused]] constexpr auto kGpuWatchdogTimeoutForTestingSlow =
    base::Milliseconds(240);
[[maybe_unused]] constexpr auto kExtraGPUJobTimeForTestingSlow =
    base::Milliseconds(1000);

// For Fuchsia in which GpuWatchdogTest.GpuInitializationAndRunningTasks test
// is flaky.
[[maybe_unused]] constexpr auto kGpuWatchdogTimeoutForTestingSlowest =
    base::Milliseconds(1000);
[[maybe_unused]] constexpr auto kExtraGPUJobTimeForTestingSlowest =
    base::Milliseconds(4000);

// On Windows, the gpu watchdog check if the main thread has used the full
// thread time. We want to detect the case in which the main thread is swapped
// out by the OS scheduler. The task on windows is simiulated by reading
// TimeTicks instead of Sleep().
void SimpleTask(base::TimeDelta duration, base::TimeDelta extra_time) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  auto start_timetick = base::TimeTicks::Now();
  do {
  } while ((base::TimeTicks::Now() - start_timetick) < duration);

  base::PlatformThread::Sleep(extra_time);

#else
  base::PlatformThread::Sleep(duration + extra_time);
#endif
}
}  // namespace

class GpuWatchdogTest : public testing::Test {
 public:
  GpuWatchdogTest() {}

  void LongTaskWithReportProgress(base::TimeDelta duration,
                                  base::TimeDelta report_delta);

#if BUILDFLAG(IS_ANDROID)
  void LongTaskFromBackgroundToForeground(
      base::TimeDelta duration,
      base::TimeDelta extra_time,
      base::TimeDelta time_to_switch_to_foreground);
#endif

  // Implements testing::Test
  void SetUp() override;

 protected:
  ~GpuWatchdogTest() override = default;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop;
  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;
  base::TimeDelta timeout_ = kGpuWatchdogTimeoutForTesting;
  base::TimeDelta extra_gpu_job_time_ = kExtraGPUJobTimeForTesting;
  base::TimeDelta full_thread_time_on_windows_ = base::TimeDelta();
};

class GpuWatchdogPowerTest : public GpuWatchdogTest {
 public:
  GpuWatchdogPowerTest() {}

  void LongTaskOnResume(base::TimeDelta duration,
                        base::TimeDelta extra_time,
                        base::TimeDelta time_to_power_resume);

  // Implements testing::Test
  void SetUp() override;
  void TearDown() override;

 protected:
  ~GpuWatchdogPowerTest() override = default;
  base::test::ScopedPowerMonitorTestSource power_monitor_source_;
};

void GpuWatchdogTest::SetUp() {
  ASSERT_TRUE(base::SingleThreadTaskRunner::HasCurrentDefault());
  ASSERT_TRUE(base::CurrentThread::IsSet());

  enum TimeOutType {
    kNormal,
    kSlow,
    kSlowest,
  };

  TimeOutType timeout_type = kNormal;

#if BUILDFLAG(IS_MAC)
  // Use a slow timeout for for MacBookPro model < MacBookPro14,1.
  //
  // As per EveryMac, laptops older than MacBookPro14,1 max out at macOS 12
  // Monterey. When macOS 13 is the minimum required version for Chromium, this
  // check can be removed.
  //
  // Reference:
  //   https://everymac.com/systems/by_capability/mac-specs-by-machine-model-machine-id.html
  std::string model_str = base::SysInfo::HardwareModelName();
  std::optional<base::SysInfo::HardwareModelNameSplit> split =
      base::SysInfo::SplitHardwareModelNameDoNotUse(model_str);

  if (split && split.value().category == "MacBookPro" &&
      split.value().model < 14) {
    timeout_type = kSlow;
  }

#elif BUILDFLAG(IS_ANDROID)
  int32_t major_version = 0;
  int32_t minor_version = 0;
  int32_t bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);

  // For Android version < Android Pie (Version 9)
  if (major_version < 9) {
    timeout_type = kSlow;
  }

#elif BUILDFLAG(IS_FUCHSIA)
  timeout_type = kSlowest;
#endif

  if (timeout_type == kSlow) {
    timeout_ = kGpuWatchdogTimeoutForTestingSlow;
    extra_gpu_job_time_ = kExtraGPUJobTimeForTestingSlow;
  } else if (timeout_type == kSlowest) {
    timeout_ = kGpuWatchdogTimeoutForTestingSlowest;
    extra_gpu_job_time_ = kExtraGPUJobTimeForTestingSlowest;
  }

#if BUILDFLAG(IS_WIN)
  full_thread_time_on_windows_ = timeout_ * kMaxCountOfMoreGpuThreadTimeAllowed;
#endif

  watchdog_thread_ = gpu::GpuWatchdogThread::Create(
      /*start_backgrounded=*/false,
      /*timeout=*/timeout_,
      /*restart_factor=*/kRestartFactor,
      /*test_mode=*/true, /*thread_name=*/"GpuWatchdog");
}

void GpuWatchdogPowerTest::SetUp() {
  GpuWatchdogTest::SetUp();

  // Report GPU init complete.
  watchdog_thread_->OnInitComplete();
}

void GpuWatchdogPowerTest::TearDown() {
  GpuWatchdogTest::TearDown();
  watchdog_thread_.reset();
}

// This task will run for duration_ms milliseconds. It will also call watchdog
// ReportProgress() every report_delta_ms milliseconds.
void GpuWatchdogTest::LongTaskWithReportProgress(base::TimeDelta duration,
                                                 base::TimeDelta report_delta) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::TimeTicks end;

  do {
    SimpleTask(report_delta, /*extra_time=*/base::TimeDelta());
    watchdog_thread_->ReportProgress();
    end = base::TimeTicks::Now();
  } while (end - start <= duration);
}

#if BUILDFLAG(IS_ANDROID)
void GpuWatchdogTest::LongTaskFromBackgroundToForeground(
    base::TimeDelta duration,
    base::TimeDelta extra_time,
    base::TimeDelta time_to_switch_to_foreground) {
  // Chrome is running in the background first.
  watchdog_thread_->OnBackgrounded();
  SimpleTask(time_to_switch_to_foreground, /*extra_time=*/base::TimeDelta());
  // Now switch Chrome to the foreground after the specified time
  watchdog_thread_->OnForegrounded();
  SimpleTask(duration, extra_time);
}
#endif

void GpuWatchdogPowerTest::LongTaskOnResume(
    base::TimeDelta duration,
    base::TimeDelta extra_time,
    base::TimeDelta time_to_power_resume) {
  // Stay in power suspension mode first.
  power_monitor_source_.GenerateSuspendEvent();

  SimpleTask(time_to_power_resume, /*extra_time=*/base::TimeDelta());

  // Now wake up on power resume.
  power_monitor_source_.GenerateResumeEvent();
  // Continue the GPU task for the remaining time.
  SimpleTask(duration, extra_time);
}

// Normal GPU Initialization.
TEST_F(GpuWatchdogTest, GpuInitializationComplete) {
  // Assume GPU initialization takes a quarter of WatchdogTimeout.
  auto normal_task_time = timeout_ / 4;

  SimpleTask(normal_task_time, /*extra_time=*/base::TimeDelta());
  watchdog_thread_->OnInitComplete();

  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_FALSE(result);
}

// GPU Hang In Initialization.
TEST_F(GpuWatchdogTest, GpuInitializationHang) {
  auto allowed_time = timeout_ * 2 + full_thread_time_on_windows_;

  // GPU init takes longer than timeout.
  SimpleTask(allowed_time, /*extra_time=*/extra_gpu_job_time_);

  // Gpu hangs. OnInitComplete() is not called
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
  // retry on failure.
}

// Normal GPU Initialization and Running Task.
TEST_F(GpuWatchdogTest, GpuInitializationAndRunningTasks) {
  // Assume GPU initialization takes quarter of WatchdogTimeout time.
  auto normal_task_time = timeout_ / 4;
  SimpleTask(normal_task_time, /*extra_time=*/base::TimeDelta());
  watchdog_thread_->OnInitComplete();

  // Start running GPU tasks. Watchdog function WillProcessTask(),
  // DidProcessTask() and ReportProgress() are tested.
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SimpleTask, normal_task_time,
                                /*extra_time=*/base::TimeDelta()));
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SimpleTask, normal_task_time,
                                /*extra_time=*/base::TimeDelta()));

  // This long task takes 6X timeout to finish, longer than timeout. But it
  // reports progress every quarter of watchdog |timeout_|, so this is an
  // expected normal behavior.
  auto normal_long_task_time = timeout_ * 6;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogTest::LongTaskWithReportProgress,
                                base::Unretained(this), normal_long_task_time,
                                /*report_progress_time*/ timeout_ / 4));

  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // Everything should be fine. No GPU hang detected.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_FALSE(result);
}

// GPU Hang when running a task.
TEST_F(GpuWatchdogTest, GpuRunningATaskHang) {
  // Report gpu init complete
  watchdog_thread_->OnInitComplete();

  // Start running a GPU task.
  auto allowed_time = timeout_ * 2 + full_thread_time_on_windows_;

  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SimpleTask, allowed_time, extra_gpu_job_time_));

  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // This GPU task takes too long. A GPU hang should be detected.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(GpuWatchdogTest, ChromeInBackground) {
  // Chrome starts in the background.
  watchdog_thread_->OnBackgrounded();

  // Gpu init takes longer than 6x watchdog |timeout_|. This is normal since
  // Chrome is running in the background.
  auto normal_long_task_time = timeout_ * 6;
  SimpleTask(normal_long_task_time, /*extra_time=*/base::TimeDelta());

  // Report GPU init complete.
  watchdog_thread_->OnInitComplete();

  // Run a task that takes 6x watchdog |timeout_| longer.This is normal since
  // Chrome is running in the background.
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SimpleTask, normal_long_task_time,
                                /*extra_time=*/base::TimeDelta()));
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

  // A task stays in the background for watchdog |timeout_| then switches to the
  // foreground and runs longer than the first-time foreground watchdog timeout
  // allowed.
  auto allowed_time = timeout_ * (kRestartFactor + 1);
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuWatchdogTest::LongTaskFromBackgroundToForeground,
                     base::Unretained(this), /*duration*/ allowed_time,
                     /*extra_time=*/extra_gpu_job_time_,
                     /*time_to_switch_to_foreground*/ timeout_ / 4));

  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // It takes too long to finish a task after switching to the foreground.
  // A GPU hang should be detected.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
}
#endif

TEST_F(GpuWatchdogTest, GpuInitializationPause) {
  // Running for watchdog |timeout_|/4 in the beginning of GPU init.
  SimpleTask(timeout_ / 4,
             /*extra_time=*/base::TimeDelta());
  watchdog_thread_->PauseWatchdog();

  // The Gpu init continues for another 6x watchdog |timeout_| after the pause.
  // This is normal since watchdog is paused.
  auto normal_long_task_time = timeout_ * 6;
  SimpleTask(normal_long_task_time, /*extra_time=*/base::TimeDelta());

  // No GPU hang is detected when the watchdog is paused.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_FALSE(result);

  // Continue the watchdog now.
  watchdog_thread_->ResumeWatchdog();

  // The Gpu init continues for longer than allowed init time.
  auto allowed_time = timeout_ * 2 + full_thread_time_on_windows_;

  SimpleTask(allowed_time, /*extra_time=*/extra_gpu_job_time_);

  // A GPU hang should be detected.
  result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
}

TEST_F(GpuWatchdogPowerTest, GpuOnSuspend) {
  // watchdog_thread_->OnInitComplete() is called in SetUp

  // Enter power suspension mode.
  power_monitor_source_.GenerateSuspendEvent();

  // Run a task that takes 6x watchdog |timeout_|.
  auto normal_long_task_time = timeout_ * 6;
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SimpleTask, normal_long_task_time,
                                /*extra_time=*/base::TimeDelta()));
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

  // This task stays in the suspension mode for watchdog |timeout_|/4, and it
  // wakes up on power resume and then runs a job that is longer than the
  // watchdog resume restart timeout.
  auto allowed_time =
      timeout_ * (kRestartFactor + 1) + full_thread_time_on_windows_;

  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuWatchdogPowerTest::LongTaskOnResume,
                                base::Unretained(this),
                                /*duration*/ allowed_time,
                                /*extra_time=*/extra_gpu_job_time_,
                                /*time_to_power_resume*/ timeout_ / 4));

  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // It takes too long to finish this task after power resume. A GPU hang should
  // be detected.
  bool result = watchdog_thread_->IsGpuHangDetectedForTesting();
  EXPECT_TRUE(result);
}

}  // namespace gpu
