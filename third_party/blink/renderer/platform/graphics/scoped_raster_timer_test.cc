// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/scoped_raster_timer.h"

#include "base/test/metrics/histogram_tester.h"
#include "cc/test/stub_decode_cache.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using testing::Test;

constexpr base::TimeDelta kExpectedCPUDuration =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime;
// kExpectedGPUDuration does not need to be related to kMockElapsedTime.
// We chose kMockElapsedTime * 2 arbitrarily to ensure that CPU, GPU, and
// Total duration values all end up in different histogram buckets.
constexpr base::TimeDelta kExpectedGPUDuration =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime * 2;

// This is a fake raster interface that will always report that GPU
// commands have finished executing in kExpectedGPUDuration microseconds.
class FakeRasterCommandsCompleted : public viz::TestRasterInterface {
 public:
  void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override {
    if (pname == GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT) {
      // Signal that commands have completed.
      *params = 1;
    } else if (pname == GL_QUERY_RESULT_EXT) {
      *params = kExpectedGPUDuration.InMicroseconds();
    } else {
      viz::TestRasterInterface::GetQueryObjectuivEXT(id, pname, params);
    }
  }
};

class ScopedRasterTimerTest : public Test {
 public:
  void SetUp() override {
    auto fake_raster_context = std::make_unique<FakeRasterCommandsCompleted>();
    test_context_provider_ =
        viz::TestContextProvider::CreateRaster(std::move(fake_raster_context));
    auto* test_raster = test_context_provider_->UnboundTestRasterInterface();
    test_raster->set_gpu_rasterization(true);
    test_raster->set_supports_gpu_memory_buffer_format(
        gfx::BufferFormat::RGBA_8888, true);
    test_raster->set_supports_gpu_memory_buffer_format(
        gfx::BufferFormat::BGRA_8888, true);

    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.supports_scanout_shared_images = true;
    test_context_provider_->SharedImageInterface()->SetCapabilities(
        shared_image_caps);

    InitializeSharedGpuContextRaster(test_context_provider_.get(),
                                     &image_decode_cache_);
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::Reset(); }

 protected:
  test::TaskEnvironment task_environment_;
  cc::StubDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

TEST_F(ScopedRasterTimerTest, UnacceleratedRasterDuration) {
  base::ScopedMockElapsedTimersForTest mock_timer;
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  std::unique_ptr<CanvasResourceProvider> provider =
      CanvasResourceProvider::CreateSharedImageProvider(
          kInfo, cc::PaintFlags::FilterQuality::kMedium,
          CanvasResourceProvider::ShouldInitialize::kCallClear,
          context_provider_wrapper_, RasterMode::kCPU,
          shared_image_usage_flags);

  ASSERT_NE(provider.get(), nullptr);

  provider->AlwaysEnableRasterTimersForTesting(true);

  base::HistogramTester histograms;

  // Trigger a flush, which will capture a raster duration measurement.
  provider->Canvas().clear(SkColors::kBlue);
  provider->ProduceCanvasResource(FlushReason::kTesting);
  provider = nullptr;

  histograms.ExpectUniqueSample(
      ScopedRasterTimer::kRasterDurationUnacceleratedHistogram,
      kExpectedCPUDuration.InMicroseconds(), 1);
  histograms.ExpectTotalCount(
      ScopedRasterTimer::kRasterDurationAcceleratedCpuHistogram, 0);
  histograms.ExpectTotalCount(
      ScopedRasterTimer::kRasterDurationAcceleratedGpuHistogram, 0);
  histograms.ExpectTotalCount(
      ScopedRasterTimer::kRasterDurationAcceleratedTotalHistogram, 0);

  SharedGpuContext::Reset();
}

TEST_F(ScopedRasterTimerTest, AcceleratedRasterDuration) {
  base::ScopedMockElapsedTimersForTest mock_timer;
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kMedium,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, gpu::SharedImageUsageSet());

  ASSERT_TRUE(!!provider);

  provider->AlwaysEnableRasterTimersForTesting(true);

  // Trigger a flush, which will capture a raster duration measurement.
  provider->Canvas().clear(SkColors::kBlue);
  provider->ProduceCanvasResource(FlushReason::kTesting);

  base::HistogramTester histograms;

  // CanvasResourceProvider destructor performs a timer check
  // on the async GPU timers.
  provider = nullptr;

  histograms.ExpectTotalCount(
      ScopedRasterTimer::kRasterDurationUnacceleratedHistogram, 0);
  histograms.ExpectUniqueSample(
      ScopedRasterTimer::kRasterDurationAcceleratedCpuHistogram,
      kExpectedCPUDuration.InMicroseconds(), 1);
  histograms.ExpectUniqueSample(
      ScopedRasterTimer::kRasterDurationAcceleratedGpuHistogram,
      kExpectedGPUDuration.InMicroseconds(), 1);
  histograms.ExpectUniqueSample(
      ScopedRasterTimer::kRasterDurationAcceleratedTotalHistogram,
      (kExpectedCPUDuration + kExpectedGPUDuration).InMicroseconds(), 1);
}

}  // namespace blink
