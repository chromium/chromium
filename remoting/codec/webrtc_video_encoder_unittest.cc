// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder.h"

#include "base/system/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

struct ThreadCountTestCase {
  const char* name;
  int frame_size;
  int num_processors;
  int expected_threads;
};

}  // namespace

TEST(WebrtcVideoEncoderTest, GetEncoderThreadCount_ResolutionTiers) {
  // With 64 logical CPUs, `(num_processors + 1) / 2 == 32`, so none of the
  // resolution tiers are capped by core count.
  constexpr ThreadCountTestCase kCases[] = {
      {"Sub720p_Min", 320, 64, 1},
      {"Sub720p_UpperBoundary", 719, 64, 1},
      {"720p_LowerBoundary", 720, 64, 2},
      {"720p_Portrait1080Width", 1080, 64, 2},
      {"720p_UpperBoundary", 1279, 64, 2},
      {"1280_LowerBoundary", 1280, 64, 4},
      {"1280_UpperBoundary", 1919, 64, 4},
      {"1080p_LowerBoundary", 1920, 64, 8},
      {"1440p", 2560, 64, 8},
      {"1440p_UpperBoundary", 3839, 64, 8},
      {"4K_LowerBoundary", 3840, 64, 16},
      {"4K_UpperBoundary", 5119, 64, 16},
      {"5K_LowerBoundary", 5120, 64, 32},
      {"8K", 7680, 64, 32},
  };

  for (const auto& tc : kCases) {
    SCOPED_TRACE(tc.name);
    EXPECT_EQ(WebrtcVideoEncoder::GetEncoderThreadCountForTesting(
                  tc.frame_size, tc.num_processors),
              tc.expected_threads);
  }
}

TEST(WebrtcVideoEncoderTest, GetEncoderThreadCount_ProcessorCountCap) {
  constexpr ThreadCountTestCase kCases[] = {
      // Single-core and dual-core hosts never drop below 1 thread.
      {"1Cpu_1080p", 1920, 1, 1},
      {"2Cpus_1080p", 1920, 2, 1},
      {"4Cpus_1080p", 1920, 4, 2},
      {"8Cpus_1080p", 1920, 8, 4},
      // 10-14 logical CPUs yield 5-7 threads at 1080p.
      {"10Cpus_1080p", 1920, 10, 5},
      {"12Cpus_1080p", 1920, 12, 6},
      {"14Cpus_1080p", 1920, 14, 7},
      // 15 logical CPUs reach the full 8-thread 1080p tier while capping 4K.
      {"15Cpus_1080p", 1920, 15, 8},
      {"15Cpus_4K", 3840, 15, 8},
      {"31Cpus_4K", 3840, 31, 16},
      {"31Cpus_5K", 5120, 31, 16},
      {"63Cpus_5K", 5120, 63, 32},
  };

  for (const auto& tc : kCases) {
    SCOPED_TRACE(tc.name);
    EXPECT_EQ(WebrtcVideoEncoder::GetEncoderThreadCountForTesting(
                  tc.frame_size, tc.num_processors),
              tc.expected_threads);
  }

  // Verify the production entry point matches the host processor count.
  const int host_cpus = base::SysInfo::NumberOfProcessors();
  EXPECT_EQ(
      WebrtcVideoEncoder::GetEncoderThreadCount(1920),
      WebrtcVideoEncoder::GetEncoderThreadCountForTesting(1920, host_cpus));
}

}  // namespace remoting
