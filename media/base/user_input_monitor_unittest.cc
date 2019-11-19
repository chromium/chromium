// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/user_input_monitor.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

namespace media {

TEST(UserInputMonitorTest, CreatePlatformSpecific) {
#if defined(OS_LINUX)
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
#else
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
#endif  // defined(OS_LINUX)

  std::unique_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      base::ThreadTaskRunnerHandle::Get(), base::ThreadTaskRunnerHandle::Get());

  if (!monitor)
    return;

  monitor->EnableKeyPressMonitoring();
  monitor->DisableKeyPressMonitoring();

  monitor.reset();
  base::RunLoop().RunUntilIdle();
}

TEST(UserInputMonitorTest, CreatePlatformSpecificWithMapping) {
#if defined(OS_LINUX)
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
#else
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
#endif  // defined(OS_LINUX)

  std::unique_ptr<UserInputMonitor> monitor = UserInputMonitor::Create(
      base::ThreadTaskRunnerHandle::Get(), base::ThreadTaskRunnerHandle::Get());

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

TEST(UserInputMonitorTest, ReadWriteKeyPressMonitorCount) {
  std::unique_ptr<base::MappedReadOnlyRegion> shmem =
      std::make_unique<base::MappedReadOnlyRegion>(
          base::ReadOnlySharedMemoryRegion::Create(sizeof(uint32_t)));
  ASSERT_TRUE(shmem->IsValid());

  constexpr uint32_t count = 10;
  WriteKeyPressMonitorCount(shmem->mapping, count);
  base::ReadOnlySharedMemoryMapping readonly_mapping = shmem->region.Map();
  EXPECT_EQ(count, ReadKeyPressMonitorCount(readonly_mapping));
}

}  // namespace media
