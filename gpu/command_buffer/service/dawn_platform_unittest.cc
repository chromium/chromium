// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_platform.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "gpu/vulkan/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu::webgpu {

class DawnPlatformTest : public testing::Test {
 public:
  DawnPlatformTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DawnPlatformTest, RecordsCacheHistogramsAfterDelay) {
  DawnPlatform platform(nullptr, nullptr, "GPU.GraphiteDawn.", true);

  // Simulate some cache hits and misses.
  platform.HistogramCustomCounts("D3D11.CompileShader.CacheHit", 1, 1, 100, 50);
  platform.HistogramCustomCounts("D3D11.CompileShader.CacheHit", 1, 1, 100, 50);
  platform.HistogramCustomCounts("D3D11.CompileShader.CacheMiss", 1, 1, 100,
                                 50);

  // The histograms should not be recorded yet.
  histogram_tester_.ExpectTotalCount(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Counts."
      "90SecondsPostStartup",
      0);
  histogram_tester_.ExpectTotalCount(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheMiss.Counts."
      "90SecondsPostStartup",
      0);
  histogram_tester_.ExpectTotalCount(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Percentage."
      "90SecondsPostStartup",
      0);

  // Fast forward time by 90 seconds to trigger the delayed task.
  task_environment_.FastForwardBy(base::Seconds(90));

  // Now, the histograms should be recorded.
  histogram_tester_.ExpectUniqueSample(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Counts."
      "90SecondsPostStartup",
      2, 1);
  histogram_tester_.ExpectUniqueSample(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheMiss.Counts."
      "90SecondsPostStartup",
      1, 1);
  // 2 hits / (2 hits + 1 miss) = 66%
  histogram_tester_.ExpectUniqueSample(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Percentage."
      "90SecondsPostStartup",
      66, 1);
}

TEST_F(DawnPlatformTest, RecordsCacheHistogramsOnFramePresented) {
  DawnPlatform platform(nullptr, nullptr, "GPU.GraphiteDawn.", true);

  // Simulate some cache hits and misses.
  platform.HistogramCustomCounts("D3D11.CompileShader.CacheHit", 1, 1, 100, 50);
  platform.HistogramCustomCounts("D3D11.CompileShader.CacheHit", 1, 1, 100, 50);
  platform.HistogramCustomCounts("D3D11.CompileShader.CacheMiss", 1, 1, 100,
                                 50);

  // The histograms should not be recorded yet.
  histogram_tester_.ExpectTotalCount(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Counts.1stPresent", 0);
  histogram_tester_.ExpectTotalCount(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Percentage.1stPresent", 0);

  // Call OnFramePresented to trigger histogram recording.
  platform.OnFramePresented();

  // Now, the histograms should be recorded.
  histogram_tester_.ExpectUniqueSample(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Counts.1stPresent", 2, 1);
  // 2 hits / (2 hits + 1 miss) = 66%
  histogram_tester_.ExpectUniqueSample(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Percentage.1stPresent", 66,
      1);

  // Call OnFramePresented again and check that histograms are not recorded a
  // second time.
  platform.OnFramePresented();
  histogram_tester_.ExpectUniqueSample(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Counts.1stPresent", 2, 1);
  histogram_tester_.ExpectUniqueSample(
      "GPU.GraphiteDawn.D3D11.CompileShader.CacheHit.Percentage.1stPresent", 66,
      1);
}

TEST_F(DawnPlatformTest, RecordsAbsoluteHistograms) {
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_VULKAN)
  {
    // Test Graphite path: uses custom unified names.
    DawnPlatform platform(nullptr, nullptr, "GPU.GraphiteDawn.", false);
    platform.HistogramCustomCounts("Vulkan.CreateGraphicsPipelines.CacheHit",
                                   10, 1, 100, 50);
    histogram_tester_.ExpectUniqueSample(
        "GPU.Vulkan.SkiaContext.vkCreateGraphicsPipelinesUS", 10, 1);
  }
#endif

  {
    // Test WebGPU path: no changes to name.
    DawnPlatform platform(nullptr, nullptr, "GPU.WebGPU.", false);
    platform.HistogramCustomCounts("Custom.Metric", 20, 1, 100, 50);
    histogram_tester_.ExpectUniqueSample("GPU.WebGPU.Custom.Metric", 20, 1);
  }
}

}  // namespace gpu::webgpu
