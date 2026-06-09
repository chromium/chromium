// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "gpu/config/gpu_blocklist.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_mode.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {
std::vector<uint32_t> GetActiveBlocklistEntryIDs() {
  GPUInfo gpu_info;
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GpuFeatureInfo gpu_feature_info =
      ComputeGpuFeatureInfo(gpu_info, GpuPreferences(), &command_line, nullptr);
  std::unique_ptr<gpu::GpuBlocklist> blocklist(gpu::GpuBlocklist::Create());
  CHECK(blocklist.get());
  CHECK_GT(blocklist->max_entry_id(), 0u);

  std::vector<uint32_t> active_blocklist_entry_ids =
      blocklist->GetEntryIDsFromIndices(
          gpu_feature_info.applied_gpu_blocklist_entries);
  EXPECT_EQ(gpu_feature_info.applied_gpu_blocklist_entries.size(),
            active_blocklist_entry_ids.size());
  return active_blocklist_entry_ids;
}
}  // namespace

TEST(GpuUtilTest, GetGpuFeatureInfo_WorkaroundFromCommandLine) {
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    GPUInfo gpu_info;
    GpuFeatureInfo gpu_feature_info = ComputeGpuFeatureInfo(
        gpu_info, GpuPreferences(), &command_line, nullptr);
    EXPECT_FALSE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }

  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(GpuDriverBugWorkaroundTypeToString(
                                       USE_GPU_DRIVER_WORKAROUND_FOR_TESTING),
                                   "1");
    GPUInfo gpu_info;
    GpuFeatureInfo gpu_feature_info = ComputeGpuFeatureInfo(
        gpu_info, GpuPreferences(), &command_line, nullptr);
    EXPECT_TRUE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }

  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kGpuDriverBugListTestGroup, "1");
    // See gpu/config/gpu_driver_bug_list.json, test_group 1, entry 215.
    GPUInfo gpu_info;
    GpuFeatureInfo gpu_feature_info = ComputeGpuFeatureInfo(
        gpu_info, GpuPreferences(), &command_line, nullptr);
    EXPECT_TRUE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }

  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kGpuDriverBugListTestGroup, "1");
    command_line.AppendSwitchASCII(GpuDriverBugWorkaroundTypeToString(
                                       USE_GPU_DRIVER_WORKAROUND_FOR_TESTING),
                                   "0");
    // See gpu/config/gpu_driver_bug_list.json, test_group 1, entry 215.
    GPUInfo gpu_info;
    GpuFeatureInfo gpu_feature_info = ComputeGpuFeatureInfo(
        gpu_info, GpuPreferences(), &command_line, nullptr);
    EXPECT_FALSE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }
}

TEST(GpuUtilTest, GetGpuFeatureInfo_WorkaroundFromFeatureFlag) {
  {
    GPUInfo gpu_info;
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    GpuFeatureInfo gpu_feature_info = ComputeGpuFeatureInfo(
        gpu_info, GpuPreferences(), &command_line, nullptr);
    EXPECT_FALSE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }
  {
    base::test::ScopedFeatureList enable_feature;
    enable_feature.InitAndEnableFeatureWithParameters(
        features::kGPUDriverBugListTestGroup, {{"test_group", "1"}});
    // See gpu/config/gpu_driver_bug_list.json, test_group 1, entry 215.
    GPUInfo gpu_info;
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    GpuFeatureInfo gpu_feature_info = ComputeGpuFeatureInfo(
        gpu_info, GpuPreferences(), &command_line, nullptr);
    EXPECT_TRUE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }

  {
    base::test::ScopedFeatureList enable_feature;
    enable_feature.InitAndEnableFeature(features::kGPUDriverBugListTestGroup);
    // See gpu/config/gpu_driver_bug_list.json, test_group 1, entry 215.
    GPUInfo gpu_info;
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    GpuFeatureInfo gpu_feature_info = ComputeGpuFeatureInfo(
        gpu_info, GpuPreferences(), &command_line, nullptr);
    EXPECT_FALSE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }
}  // namespace

TEST(GpuUtilTest, GetGpuFeatureInfo_SoftwareRenderingFromFeatureFlag) {
  {
    std::vector<uint32_t> active_blocklist_entry_ids =
        GetActiveBlocklistEntryIDs();
    // See software_rendering_list.json, test_group 1, entry 152.
    EXPECT_EQ(std::count(active_blocklist_entry_ids.begin(),
                         active_blocklist_entry_ids.end(), 152),
              0);
    EXPECT_EQ(std::count(active_blocklist_entry_ids.begin(),
                         active_blocklist_entry_ids.end(), 153),
              0);
  }
  {
    base::test::ScopedFeatureList enable_feature;
    enable_feature.InitAndEnableFeatureWithParameters(
        features::kGPUBlockListTestGroup, {{"test_group", "1"}});
    std::vector<uint32_t> active_blocklist_entry_ids =
        GetActiveBlocklistEntryIDs();
    // See software_rendering_list.json, test_group 1, entry 152.
    EXPECT_EQ(std::count(active_blocklist_entry_ids.begin(),
                         active_blocklist_entry_ids.end(), 152),
              1);
    // See software_rendering_list.json, test_group 2, entry 153
    EXPECT_EQ(std::count(active_blocklist_entry_ids.begin(),
                         active_blocklist_entry_ids.end(), 153),
              0);
  }
}

// Graphite is enabled; no fallback should occur.
TEST(GpuUtilTest, GrContextType_GraphiteValidNoFallback) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GPUInfo gpu_info;
  GpuPreferences gpu_preferences;
  gpu_preferences.gr_context_type = GrContextType::kGraphiteDawn;
  gpu_preferences.fallback_gr_context_types = {GrContextType::kGL};

  GpuFeatureInfo gpu_feature_info =
      ComputeGpuFeatureInfo(gpu_info, gpu_preferences, &command_line, nullptr);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] =
      kGpuFeatureStatusEnabled;

  EXPECT_TRUE(TryFallbackGrContextTypesIfNeeded(
      gpu_feature_info, gpu_preferences, gpu_info, &command_line));
  EXPECT_EQ(gpu_preferences.gr_context_type, GrContextType::kGraphiteDawn);
  ASSERT_EQ(gpu_preferences.fallback_gr_context_types.size(), 1u);
  EXPECT_EQ(gpu_preferences.fallback_gr_context_types[0], GrContextType::kGL);
}

// Graphite is blocklisted; should fall back to GL.
TEST(GpuUtilTest, GrContextType_GraphiteFallbackToGL) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GPUInfo gpu_info;
  GpuPreferences gpu_preferences;
  gpu_preferences.gr_context_type = GrContextType::kGraphiteDawn;
  gpu_preferences.fallback_gr_context_types = {GrContextType::kGL};

  GpuFeatureInfo gpu_feature_info =
      ComputeGpuFeatureInfo(gpu_info, gpu_preferences, &command_line, nullptr);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] =
      kGpuFeatureStatusBlocklisted;

  EXPECT_TRUE(TryFallbackGrContextTypesIfNeeded(
      gpu_feature_info, gpu_preferences, gpu_info, &command_line));
  EXPECT_EQ(gpu_preferences.gr_context_type, GrContextType::kGL);
  EXPECT_TRUE(gpu_preferences.fallback_gr_context_types.empty());
}

// Graphite is blocklisted and no fallback is available;
// TryFallbackGrContextTypesIfNeeded should return false.
TEST(GpuUtilTest, GrContextType_GraphiteNoFallbackAvailable) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GPUInfo gpu_info;
  GpuPreferences gpu_preferences;
  gpu_preferences.gr_context_type = GrContextType::kGraphiteDawn;

  GpuFeatureInfo gpu_feature_info =
      ComputeGpuFeatureInfo(gpu_info, gpu_preferences, &command_line, nullptr);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] =
      kGpuFeatureStatusBlocklisted;

  EXPECT_FALSE(TryFallbackGrContextTypesIfNeeded(
      gpu_feature_info, gpu_preferences, gpu_info, &command_line));
  EXPECT_EQ(gpu_preferences.gr_context_type, GrContextType::kGraphiteDawn);
}

// Graphite is blocklisted with fallbacks [kVulkan, kGL] (kGL tried first);
// should stop at GL, leaving kVulkan in the fallback list.
TEST(GpuUtilTest, GrContextType_GraphiteFallbackToGLWithVulkanRemaining) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  GPUInfo gpu_info;
  GpuPreferences gpu_preferences;
  gpu_preferences.gr_context_type = GrContextType::kGraphiteDawn;
  // kGL at front (tried first), kVulkan at back.
  gpu_preferences.fallback_gr_context_types = {GrContextType::kGL,
                                               GrContextType::kVulkan};

  GpuFeatureInfo gpu_feature_info =
      ComputeGpuFeatureInfo(gpu_info, gpu_preferences, &command_line, nullptr);
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_SKIA_GRAPHITE] =
      kGpuFeatureStatusBlocklisted;

  EXPECT_TRUE(TryFallbackGrContextTypesIfNeeded(
      gpu_feature_info, gpu_preferences, gpu_info, &command_line));
  EXPECT_EQ(gpu_preferences.gr_context_type, GrContextType::kGL);
  ASSERT_EQ(gpu_preferences.fallback_gr_context_types.size(), 1u);
  EXPECT_EQ(gpu_preferences.fallback_gr_context_types[0],
            GrContextType::kVulkan);
}

}  // namespace gpu
