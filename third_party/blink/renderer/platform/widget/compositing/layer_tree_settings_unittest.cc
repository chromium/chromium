// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

#if BUILDFLAG(IS_ANDROID)
TEST(LayerTreeSettings, MemoryLimitIsRecorded) {
  auto policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                   gfx::Size(1920, 1080), 1.f);
  EXPECT_GT(policy.bytes_limit_when_visible, 0u);

  auto* histogram =
      base::StatisticsRecorder::FindHistogram("Blink.Compositor.MemoryLimitKb");
  ASSERT_TRUE(histogram);
  auto samples = histogram->SnapshotSamples();
  EXPECT_EQ(1, samples->TotalCount());
}
#endif

// Verify desktop memory limit calculations.
#if !BUILDFLAG(IS_ANDROID)
TEST(LayerTreeSettings, IgnoreGivenMemoryPolicy) {
  auto policy =
      GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256), gfx::Size(), 1.f);
  EXPECT_EQ(512u * 1024u * 1024u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);
}

TEST(LayerTreeSettings, LargeScreensUseMoreMemory) {
  auto policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                                   gfx::Size(4096, 2160), 1.f);
  EXPECT_EQ(977272832u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);

  policy = GetGpuMemoryPolicy(cc::ManagedMemoryPolicy(256),
                              gfx::Size(2056, 1329), 2.f);
  EXPECT_EQ(1152u * 1024u * 1024u, policy.bytes_limit_when_visible);
  EXPECT_EQ(gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE,
            policy.priority_cutoff_when_visible);
}
#endif

}  // namespace blink
