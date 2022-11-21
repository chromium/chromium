// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/events/test/keyboard_hook_monitor_utils.h"
#endif

namespace media {

namespace {

class UserInputMonitorTest : public testing::Test {
 protected:
  // testing::Test.
  void SetUp() override {
#if BUILDFLAG(IS_OZONE)
    if (ui::OzonePlatform::GetPlatformNameForTest() == "drm") {
      // OzonePlatformDrm::InitializeUI hangs in tests on the DRM platform.
      GTEST_SKIP();
    }
    // Initialise Ozone in single process mode, as all tests do.
    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    ui::OzonePlatform::InitializeForUI(params);
#endif
  }
};

}  // namespace

TEST_F(UserInputMonitorTest, CreatePlatformSpecific) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
#else
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  std::unique_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  if (!monitor)
    return;

  monitor->EnableKeyPressMonitoring();
  monitor->DisableKeyPressMonitoring();

  monitor.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserInputMonitorTest, CreatePlatformSpecificWithMapping) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
#else
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  std::unique_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  if (!monitor)
    return;

  base::ReadOnlySharedMemoryMapping readonly_mapping =
      static_cast<UserInputMonitorBase*>(monitor.get())
          ->EnableKeyPressMonitoringWithMapping()
          .Map();
  EXPECT_EQ(0u, ReadKeyPressMonitorCount(readonly_mapping));
  monitor->DisableKeyPressMonitoring();

  monitor.reset();
  base::RunLoop().RunUntilIdle();

  // Check that read only region remains valid after disable.
  EXPECT_EQ(0u, ReadKeyPressMonitorCount(readonly_mapping));
}

TEST_F(UserInputMonitorTest, ReadWriteKeyPressMonitorCount) {
  std::unique_ptr<base::MappedReadOnlyRegion> shmem =
      std::make_unique<base::MappedReadOnlyRegion>(
          base::ReadOnlySharedMemoryRegion::Create(sizeof(uint32_t)));
  ASSERT_TRUE(shmem->IsValid());

  constexpr uint32_t count = 10;
  WriteKeyPressMonitorCount(shmem->mapping, count);
  base::ReadOnlySharedMemoryMapping readonly_mapping = shmem->region.Map();
  EXPECT_EQ(count, ReadKeyPressMonitorCount(readonly_mapping));
}

#if BUILDFLAG(IS_WIN)

//
// Windows specific scenarios which require simulating keyboard hook events.
//

TEST_F(UserInputMonitorTest, BlockMonitoringAfterMonitoringEnabled) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  std::unique_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  if (!monitor)
    return;

  monitor->EnableKeyPressMonitoring();
  ui::SimulateKeyboardHookRegistered();
  ui::SimulateKeyboardHookUnregistered();
  monitor->DisableKeyPressMonitoring();

  monitor.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserInputMonitorTest, BlockMonitoringBeforeMonitoringEnabled) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  std::unique_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  if (!monitor)
    return;

  ui::SimulateKeyboardHookRegistered();
  monitor->EnableKeyPressMonitoring();
  ui::SimulateKeyboardHookUnregistered();
  monitor->DisableKeyPressMonitoring();

  monitor.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserInputMonitorTest, UnblockMonitoringAfterMonitoringDisabled) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  std::unique_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  if (!monitor)
    return;

  monitor->EnableKeyPressMonitoring();
  ui::SimulateKeyboardHookRegistered();
  monitor->DisableKeyPressMonitoring();
  ui::SimulateKeyboardHookUnregistered();

  monitor.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserInputMonitorTest, BlockKeypressMonitoringWithSharedMemoryBuffer) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  std::unique_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::SingleThreadTaskRunner::GetCurrentDefault());

  if (!monitor)
    return;

  base::ReadOnlySharedMemoryMapping readonly_mapping =
      static_cast<UserInputMonitorBase*>(monitor.get())
          ->EnableKeyPressMonitoringWithMapping()
          .Map();
  EXPECT_EQ(0u, ReadKeyPressMonitorCount(readonly_mapping));
  ui::SimulateKeyboardHookRegistered();
  EXPECT_EQ(0u, ReadKeyPressMonitorCount(readonly_mapping));
  ui::SimulateKeyboardHookUnregistered();
  EXPECT_EQ(0u, ReadKeyPressMonitorCount(readonly_mapping));
  monitor->DisableKeyPressMonitoring();

  monitor.reset();
  base::RunLoop().RunUntilIdle();

  // Check that read only region remains valid after disable.
  EXPECT_EQ(0u, ReadKeyPressMonitorCount(readonly_mapping));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace media
