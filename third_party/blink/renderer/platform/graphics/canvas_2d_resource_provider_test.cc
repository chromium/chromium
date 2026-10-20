// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "cc/test/paint_image_matchers.h"
#include "cc/test/skia_common.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_compositing_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/buffer_types.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace blink {
namespace {

constexpr int kMaxTextureSize = 1024;

}  // namespace

class ImageTrackingDecodeCache : public cc::StubDecodeCache {
 public:
  ImageTrackingDecodeCache() = default;
  ~ImageTrackingDecodeCache() override { EXPECT_EQ(num_locked_images_, 0); }

  cc::DecodedDrawImage GetDecodedImageForDraw(
      const cc::DrawImage& image) override {
    EXPECT_FALSE(disallow_cache_use_);

    ++num_locked_images_;
    ++max_locked_images_;
    decoded_images_.push_back(image);
    SkBitmap bitmap;
    bitmap.allocPixelsFlags(SkImageInfo::MakeN32Premul(10, 10),
                            SkBitmap::kZeroPixels_AllocFlag);
    sk_sp<SkImage> sk_image = SkImages::RasterFromBitmap(bitmap);
    return cc::DecodedDrawImage(
        sk_image, nullptr, SkSize::Make(0, 0), SkSize::Make(1, 1),
        cc::PaintFlags::FilterQuality::kLow, !budget_exceeded_);
  }

  void set_budget_exceeded(bool exceeded) { budget_exceeded_ = exceeded; }
  void set_disallow_cache_use(bool disallow) { disallow_cache_use_ = disallow; }

  void DrawWithImageFinished(
      const cc::DrawImage& image,
      const cc::DecodedDrawImage& decoded_image) override {
    EXPECT_FALSE(disallow_cache_use_);
    num_locked_images_--;
  }

  const Vector<cc::DrawImage>& decoded_images() const {
    return decoded_images_;
  }
  int num_locked_images() const { return num_locked_images_; }
  int max_locked_images() const { return max_locked_images_; }

 private:
  Vector<cc::DrawImage> decoded_images_;
  int num_locked_images_ = 0;
  int max_locked_images_ = 0;
  bool budget_exceeded_ = false;
  bool disallow_cache_use_ = false;
};

class Canvas2DResourceProviderTest : public Test {
 public:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::CreateRaster();
    auto* test_raster = test_context_provider_->UnboundTestRasterInterface();
    test_raster->set_max_texture_size(kMaxTextureSize);
    test_raster->set_texture_format_bgra8888(true);
    test_raster->set_texture_half_float_linear(true);

    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.supports_scanout_shared_images = true;
#if BUILDFLAG(IS_WIN)
    shared_image_caps.shared_image_swap_chain = true;
#endif
    test_context_provider_->SharedImageInterface()->SetCapabilities(
        shared_image_caps);

    InitializeSharedGpuContext(test_context_provider_.get(),
                               &image_decode_cache_);
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::Reset(); }

 protected:
  const gpu::SyncToken& GetSyncToken(const CanvasResource* resource) {
    return resource->sync_token();
  }

  void EnsureResourceRecycled(scoped_refptr<CanvasResource>&& resource) {
    auto transferable_resource = resource->PrepareTransferableResource();
    CanvasResource::DropRefOnOwningThread(std::move(resource));
  }

  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ImageTrackingDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuCompositingTestPlatform> platform_;
};

std::unique_ptr<Canvas2DResourceProvider> MakeCanvas2DResourceProvider(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  return Canvas2DResourceProvider::CreateWithClear(
      gfx::Size(10, 10), color_params, context_provider_wrapper,
      RasterMode::kGPU, shared_image_usage_flags);
}

class Canvas2DResourceProviderSyncTokenTest
    : public Canvas2DResourceProviderTest,
      public testing::WithParamInterface<bool> {
 public:
  Canvas2DResourceProviderSyncTokenTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          features::kUseAutomaticSyncTokenManagement);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kUseAutomaticSyncTokenManagement);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         Canvas2DResourceProviderSyncTokenTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "AutomaticSyncTokens"
                                             : "ManualSyncTokens";
                         });

TEST_P(Canvas2DResourceProviderSyncTokenTest,
       DontReclaimUnusedResourcesWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kCanvas2DReclaimUnusedResources);

  auto provider = MakeCanvas2DResourceProvider(context_provider_wrapper_);

  auto resource = provider->ProduceCanvasResource(/*surface=*/nullptr);
  MemoryManagedPaintRecorder recorder(provider->Size(), nullptr);
  recorder.getRecordingCanvas().clear(SkColors::kWhite);
  provider->RasterRecord(recorder.ReleaseMainRecording(), {});
  auto new_resource = provider->ProduceCanvasResource(/*surface=*/nullptr);
  ASSERT_NE(resource, new_resource);
  if (base::FeatureList::IsEnabled(
          features::kUseAutomaticSyncTokenManagement)) {
    EXPECT_FALSE(GetSyncToken(resource.get()).HasData());
    EXPECT_FALSE(GetSyncToken(new_resource.get()).HasData());
  } else {
    ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  }
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(std::move(resource));
  // There is a ready-to-reuse resource
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  // No task posted.
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

TEST_P(Canvas2DResourceProviderSyncTokenTest,
       UnusedResourcesAreNotCollectedWhenYoung) {
  base::test::ScopedFeatureList feature_list{kCanvas2DReclaimUnusedResources};

  auto provider = MakeCanvas2DResourceProvider(context_provider_wrapper_);

  auto resource = provider->ProduceCanvasResource(/*surface=*/nullptr);
  MemoryManagedPaintRecorder recorder(provider->Size(), nullptr);
  recorder.getRecordingCanvas().clear(SkColors::kWhite);
  provider->RasterRecord(recorder.ReleaseMainRecording(), {});
  auto new_resource = provider->ProduceCanvasResource(/*surface=*/nullptr);
  ASSERT_NE(resource, new_resource);
  if (base::FeatureList::IsEnabled(
          features::kUseAutomaticSyncTokenManagement)) {
    EXPECT_FALSE(GetSyncToken(resource.get()).HasData());
    EXPECT_FALSE(GetSyncToken(new_resource.get()).HasData());
  } else {
    ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  }
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(std::move(resource));
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  // There is a ready-to-reuse resource
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  task_environment_.FastForwardBy(
      Canvas2DResourceProvider::kUnusedResourceExpirationTime -
      base::Seconds(1));
  // The reclaim task hasn't run yet.
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  recorder.getRecordingCanvas().clear(SkColors::kWhite);
  provider->RasterRecord(recorder.ReleaseMainRecording(), {});
  resource = provider->ProduceCanvasResource(/*surface=*/nullptr);
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  recorder.getRecordingCanvas().clear(SkColors::kWhite);
  provider->RasterRecord(recorder.ReleaseMainRecording(), {});
  new_resource = provider->ProduceCanvasResource(/*surface=*/nullptr);
  ASSERT_NE(resource, new_resource);
  if (base::FeatureList::IsEnabled(
          features::kUseAutomaticSyncTokenManagement)) {
    EXPECT_FALSE(GetSyncToken(resource.get()).HasData());
    EXPECT_FALSE(GetSyncToken(new_resource.get()).HasData());
  } else {
    ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  }

  EnsureResourceRecycled(std::move(resource));
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  task_environment_.FastForwardBy(base::Seconds(1));

  // Too young, no release yet.
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  // But re-post the task to free it.
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  task_environment_.FastForwardBy(
      Canvas2DResourceProvider::kUnusedResourceExpirationTime);
  // Now it's collected.
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  // And no new task is posted.
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

}  // namespace blink
