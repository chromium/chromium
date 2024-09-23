// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor_posix.h"

#include <unistd.h>
#include <utility>

#include "base/files/file_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(MemoryUsageMonitorPosixTest, CalculateProcessFootprint) {
  test::TaskEnvironment task_environment_;
  MemoryUsageMonitorPosix monitor;

  const char kStatusFile[] =
      "First:    1\n"
      "Second:  2 kB\n"
      "VmSwap: 10 kB\n"
      "Third:  10 kB\n"
      "VmHWM:  72 kB\n"
      "Last:     8";
  const char kStatmFile[] = "100 40 25 0 0";
  uint64_t expected_swap_kb = 10;
  uint64_t expected_private_footprint_kb =
      (40 - 25) * getpagesize() / 1024 + expected_swap_kb;
  uint64_t expected_vm_size_kb = 100 * getpagesize() / 1024;
  uint64_t expected_peak_resident_kb = 72;

  base::FilePath statm_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&statm_path));
  EXPECT_TRUE(base::WriteFile(statm_path, kStatmFile));
  base::File statm_file(statm_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::FilePath status_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&status_path));
  EXPECT_TRUE(base::WriteFile(status_path, kStatusFile));
  base::File status_file(status_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);

  monitor.SetProcFiles(std::move(statm_file), std::move(status_file));

  MemoryUsage usage = monitor.GetCurrentMemoryUsage();
  EXPECT_EQ(expected_private_footprint_kb,
            static_cast<uint64_t>(usage.private_footprint_bytes / 1024));
  EXPECT_EQ(expected_swap_kb, static_cast<uint64_t>(usage.swap_bytes / 1024));
  EXPECT_EQ(expected_vm_size_kb,
            static_cast<uint64_t>(usage.vm_size_bytes / 1024));
  EXPECT_EQ(expected_peak_resident_kb,
            static_cast<uint64_t>(usage.peak_resident_bytes / 1024));
}

}  // namespace blink
