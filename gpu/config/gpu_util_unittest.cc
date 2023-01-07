// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include "base/command_line.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

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

}  // namespace gpu
