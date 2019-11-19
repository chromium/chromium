// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GpuDriverBugListTest : public testing::Test {
 public:
  GpuDriverBugListTest() = default;
  ~GpuDriverBugListTest() override = default;
};

#if defined(OS_ANDROID)
TEST_F(GpuDriverBugListTest, CurrentListForARM) {
  std::unique_ptr<GpuDriverBugList> list = GpuDriverBugList::Create();
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "ARM";
  gpu_info.gl_renderer = "MALi_T604";
  gpu_info.gl_version = "OpenGL ES 2.0";
  std::set<int> bugs = list->MakeDecision(
      GpuControlList::kOsAndroid, "4.1", gpu_info);
  EXPECT_EQ(1u, bugs.count(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}

TEST_F(GpuDriverBugListTest, CurrentListForImagination) {
  std::unique_ptr<GpuDriverBugList> list = GpuDriverBugList::Create();
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "Imagination Technologies";
  gpu_info.gl_renderer = "PowerVR SGX 540";
  gpu_info.gl_version = "OpenGL ES 2.0";
  std::set<int> bugs = list->MakeDecision(
      GpuControlList::kOsAndroid, "4.1", gpu_info);
  EXPECT_EQ(1u, bugs.count(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}
#endif  // OS_ANDROID

TEST_F(GpuDriverBugListTest, AppendSingleWorkaround) {
  base::CommandLine command_line(0, nullptr);
  command_line.AppendSwitch(GpuDriverBugWorkaroundTypeToString(
      DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE));
  std::set<int> workarounds;
  workarounds.insert(EXIT_ON_CONTEXT_LOST);
  workarounds.insert(INIT_VERTEX_ATTRIBUTES);
  EXPECT_EQ(2u, workarounds.size());
  GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &workarounds, command_line);
  EXPECT_EQ(3u, workarounds.size());
  EXPECT_EQ(1u, workarounds.count(DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE));
}

TEST_F(GpuDriverBugListTest, AppendForceGPUWorkaround) {
  base::CommandLine command_line(0, nullptr);
  command_line.AppendSwitch(
      GpuDriverBugWorkaroundTypeToString(FORCE_HIGH_PERFORMANCE_GPU));
  std::set<int> workarounds;
  workarounds.insert(EXIT_ON_CONTEXT_LOST);
  workarounds.insert(FORCE_LOW_POWER_GPU);
  EXPECT_EQ(2u, workarounds.size());
  EXPECT_EQ(1u, workarounds.count(FORCE_LOW_POWER_GPU));
  GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &workarounds, command_line);
  EXPECT_EQ(2u, workarounds.size());
  EXPECT_EQ(0u, workarounds.count(FORCE_LOW_POWER_GPU));
  EXPECT_EQ(1u, workarounds.count(FORCE_HIGH_PERFORMANCE_GPU));
}

// Test for invariant "Assume the newly last added entry has the largest ID".
// See GpuControlList::GpuControlList.
// It checks gpu_driver_bug_list.json
TEST_F(GpuDriverBugListTest, TestBlacklistIsValid) {
  std::unique_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  auto max_entry_id = list->max_entry_id();

  std::vector<uint32_t> indices(list->num_entries());
  int current = 0;
  std::generate(indices.begin(), indices.end(),
                [&current] () { return current++; });

  auto entries = list->GetEntryIDsFromIndices(indices);
  auto real_max_entry_id = *std::max_element(entries.begin(), entries.end());
  EXPECT_EQ(real_max_entry_id, max_entry_id);
}

}  // namespace gpu
