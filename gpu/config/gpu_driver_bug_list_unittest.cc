// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_driver_bug_list.h"

#include <vector>

#include "base/command_line.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GpuDriverBugListTest : public testing::Test {
 public:
  GpuDriverBugListTest() = default;
  ~GpuDriverBugListTest() override = default;
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(GpuDriverBugListTest, CurrentListForARM) {
  std::unique_ptr<GpuDriverBugList> list = GpuDriverBugList::Create();
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "ARM";
  gpu_info.gl_renderer = "MALi_T604";
  gpu_info.gl_version = "OpenGL ES 2.0";
  std::set<int> bugs =
      list->MakeDecision(GpuControlList::kOsAndroid, "4.1", gpu_info, {});
  EXPECT_EQ(1u, bugs.count(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}

TEST_F(GpuDriverBugListTest, CurrentListForImagination) {
  std::unique_ptr<GpuDriverBugList> list = GpuDriverBugList::Create();
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "Imagination Technologies";
  gpu_info.gl_renderer = "PowerVR SGX 540";
  gpu_info.gl_version = "OpenGL ES 2.0";
  std::set<int> bugs =
      list->MakeDecision(GpuControlList::kOsAndroid, "4.1", gpu_info, {});
  EXPECT_EQ(1u, bugs.count(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(GpuDriverBugListTest, AppendSingleWorkaround) {
  base::CommandLine command_line(0, nullptr);
  command_line.AppendSwitch(GpuDriverBugWorkaroundTypeToString(
      DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE));
  std::set<int> workarounds;
  workarounds.insert(EXIT_ON_CONTEXT_LOST);
  workarounds.insert(GL_CLEAR_BROKEN);
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

#if BUILDFLAG(IS_WIN)
TEST_F(GpuDriverBugListTest, DisableDx12InfoCollection) {
  std::unique_ptr<GpuDriverBugList> list = GpuDriverBugList::Create();
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;

  for (const std::string& driver_version :
       {"10", "20.19.14.9999", "20.19.15.4326", "20.19.15.4531",
        "20.19.15.4835", "20.19.16.0", "20.20.1.0"}) {
    gpu_info.gpu.driver_version = driver_version;
    std::set<int> workarounds =
        list->MakeDecision(GpuControlList::kOsWin, "10.0.19045", gpu_info, {});
    EXPECT_EQ(1u, workarounds.count(DISABLE_DX12_INFO_COLLECTION));
  }

  for (const std::string& driver_version : {"20.20.1.1", "20.20.1.2"}) {
    gpu_info.gpu.driver_version = driver_version;
    std::set<int> workarounds =
        list->MakeDecision(GpuControlList::kOsWin, "10.0.19045", gpu_info, {});
    EXPECT_EQ(0u, workarounds.count(DISABLE_DX12_INFO_COLLECTION));
  }

  GPUInfo hybrid_gpu_info;
  hybrid_gpu_info.gpu.vendor_id = 0x8086;
  hybrid_gpu_info.secondary_gpus.emplace_back();
  hybrid_gpu_info.secondary_gpus.back().vendor_id = 0x10de;
  hybrid_gpu_info.secondary_gpus.back().driver_version = "32.0.15.6094";
  hybrid_gpu_info.secondary_gpus.back().active = true;
  hybrid_gpu_info.gpu.driver_version = "20.19.15.4531";
  std::set<int> hybrid_workarounds = list->MakeDecision(
      GpuControlList::kOsWin, "10.0.19045", hybrid_gpu_info, {});
  EXPECT_EQ(1u, hybrid_workarounds.count(DISABLE_DX12_INFO_COLLECTION));

  GPUInfo discrete_primary_gpu_info;
  discrete_primary_gpu_info.gpu.vendor_id = 0x10de;
  discrete_primary_gpu_info.gpu.driver_version = "32.0.15.6094";
  discrete_primary_gpu_info.secondary_gpus.emplace_back();
  discrete_primary_gpu_info.secondary_gpus.back().vendor_id = 0x8086;
  discrete_primary_gpu_info.secondary_gpus.back().driver_version =
      "20.19.15.4531";
  discrete_primary_gpu_info.secondary_gpus.back().active = true;
  std::set<int> workarounds = list->MakeDecision(
      GpuControlList::kOsWin, "10.0.19045", discrete_primary_gpu_info, {});
  EXPECT_EQ(0u, workarounds.count(DISABLE_DX12_INFO_COLLECTION));
}
#endif  // BUILDFLAG(IS_WIN)

// Test for invariant "Assume the newly last added entry has the largest ID".
// See GpuControlList::GpuControlList.
// It checks gpu_driver_bug_list.json
TEST_F(GpuDriverBugListTest, TestBlocklistIsValid) {
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
