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
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
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

template <typename T>
SkImageInfo GetSkImageInfo(T* provider) {
  return SkImageInfo::Make(
      provider->Size().width(), provider->Size().height(),
      viz::ToClosestSkColorType(provider->GetSharedImageFormat()),
      provider->GetAlphaType(), provider->GetColorSpace().ToSkColorSpace());
}

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
    viz::TransferableResource transferable_resource;
    CHECK(resource->PrepareTransferableResource(
        &transferable_resource,
        /*needs_verified_synctoken=*/false));

    CanvasResource::DropRefOnOwningThread(std::move(resource));
  }

  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ImageTrackingDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuCompositingTestPlatform> platform_;
};

TEST_F(Canvas2DResourceProviderTest, UnacceleratedOverlay) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = Canvas2DResourceProvider::CreateWithClear(
      kSize, color_params, context_provider_wrapper_, RasterMode::kCPU,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider && provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());

  // We do not support single buffering for unaccelerated low latency canvas.
  EXPECT_FALSE(provider->IsSingleBuffered());

  EXPECT_EQ(GetSkImageInfo(provider.get()), kInfo);

  EXPECT_FALSE(provider->IsSingleBuffered());
}

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

TEST_F(Canvas2DResourceProviderTest, SharedImageResourceRecycling) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = Canvas2DResourceProvider::CreateWithClear(
      kSize, color_params, context_provider_wrapper_, RasterMode::kGPU,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_FALSE(provider->IsSingleBuffered());
  // As it is an accelerated canvas, it
  // will internally force it to RGBA8, or BGRA8 on MacOS
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(GetSkImageInfo(provider.get()) ==
              kInfo.makeColorType(kBGRA_8888_SkColorType));
#else
  EXPECT_TRUE(GetSkImageInfo(provider.get()) ==
              kInfo.makeColorType(kRGBA_8888_SkColorType));
#endif

  // Same resource and sync token if we query again without updating.
  auto resource = provider->ProduceCanvasResource();
  auto sync_token = GetSyncToken(resource.get());
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource, provider->ProduceCanvasResource());
  EXPECT_EQ(sync_token, GetSyncToken(resource.get()));

  provider->GetCanvasForTesting().clear(SkColors::kWhite);
  provider->RasterRecord(provider->Recorder().ReleaseMainRecording());
  auto new_resource = provider->ProduceCanvasResource();
  EXPECT_NE(resource, new_resource);
  EXPECT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  auto* resource_ptr = resource.get();

  EnsureResourceRecycled(std::move(resource));

  provider->GetCanvasForTesting().clear(SkColors::kBlack);
  provider->RasterRecord(provider->Recorder().ReleaseMainRecording());
  auto resource_again = provider->ProduceCanvasResource();
  EXPECT_EQ(resource_ptr, resource_again);
  EXPECT_NE(sync_token, GetSyncToken(resource_again.get()));
}

TEST_F(Canvas2DResourceProviderTest, UnusedResources) {
  base::test::ScopedFeatureList feature_list{kCanvas2DReclaimUnusedResources};

  auto provider = MakeCanvas2DResourceProvider(context_provider_wrapper_);

  auto resource = provider->ProduceCanvasResource();
  provider->GetCanvasForTesting().clear(SkColors::kWhite);
  provider->RasterRecord(provider->Recorder().ReleaseMainRecording());
  auto new_resource = provider->ProduceCanvasResource();
  ASSERT_NE(resource, new_resource);

  ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));

  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(std::move(resource));
  // The reclaim task has been posted.
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  // There is a ready-to-reuse resource
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  task_environment_.FastForwardBy(
      Canvas2DResourceProvider::kUnusedResourceExpirationTime);
  // The resource is freed, don't repost the task.
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

TEST_F(Canvas2DResourceProviderTest,
       DontReclaimUnusedResourcesWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kCanvas2DReclaimUnusedResources);

  auto provider = MakeCanvas2DResourceProvider(context_provider_wrapper_);

  auto resource = provider->ProduceCanvasResource();
  provider->GetCanvasForTesting().clear(SkColors::kWhite);
  provider->RasterRecord(provider->Recorder().ReleaseMainRecording());
  auto new_resource = provider->ProduceCanvasResource();
  ASSERT_NE(resource, new_resource);
  ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(std::move(resource));
  // There is a ready-to-reuse resource
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  // No task posted.
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

TEST_F(Canvas2DResourceProviderTest, UnusedResourcesAreNotCollectedWhenYoung) {
  base::test::ScopedFeatureList feature_list{kCanvas2DReclaimUnusedResources};

  auto provider = MakeCanvas2DResourceProvider(context_provider_wrapper_);

  auto resource = provider->ProduceCanvasResource();
  provider->GetCanvasForTesting().clear(SkColors::kWhite);
  provider->RasterRecord(provider->Recorder().ReleaseMainRecording());
  auto new_resource = provider->ProduceCanvasResource();
  ASSERT_NE(resource, new_resource);
  ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
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

  provider->GetCanvasForTesting().clear(SkColors::kWhite);
  provider->RasterRecord(provider->Recorder().ReleaseMainRecording());
  resource = provider->ProduceCanvasResource();
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  provider->GetCanvasForTesting().clear(SkColors::kWhite);
  provider->RasterRecord(provider->Recorder().ReleaseMainRecording());
  new_resource = provider->ProduceCanvasResource();
  ASSERT_NE(resource, new_resource);
  ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));

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

TEST_F(Canvas2DResourceProviderTest, SharedImageStaticBitmapImage) {
  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = Canvas2DResourceProvider::CreateWithClear(
      gfx::Size(10, 10), color_params, context_provider_wrapper_,
      RasterMode::kGPU, shared_image_usage_flags);

  ASSERT_NE(provider, nullptr);

  // Same resource returned until the canvas is updated.
  auto image = provider->Snapshot();
  ASSERT_TRUE(image);
  auto new_image = provider->Snapshot();
  EXPECT_EQ(image->GetSharedImage(), new_image->GetSharedImage());
  EXPECT_EQ(provider->ProduceCanvasResource()->GetSharedImage(),
            image->GetSharedImage());

  // Resource updated after draw.
  provider->GetCanvasForTesting().clear(SkColors::kWhite);
  provider->RasterRecord(provider->Recorder().ReleaseMainRecording());
  new_image = provider->Snapshot();
  EXPECT_NE(new_image->GetSharedImage(), image->GetSharedImage());

  // Resource recycled.
  auto original_shared_image = image->GetSharedImage();
  image.reset();
  provider->GetCanvasForTesting().clear(SkColors::kBlack);
  provider->RasterRecord(provider->Recorder().ReleaseMainRecording());
  EXPECT_EQ(original_shared_image, provider->Snapshot()->GetSharedImage());
}

TEST_F(Canvas2DResourceProviderTest, ImageCacheOnContextLost) {
  auto provider = MakeCanvas2DResourceProvider(context_provider_wrapper_);

  Vector<cc::DrawImage> images = {
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(10, 10)), false,
                    SkIRect::MakeWH(10, 10),
                    cc::PaintFlags::FilterQuality::kNone, SkM44(), 0u,
                    cc::TargetColorParams()),
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(20, 20)), false,
                    SkIRect::MakeWH(5, 5), cc::PaintFlags::FilterQuality::kNone,
                    SkM44(), 0u, cc::TargetColorParams())};
  provider->GetCanvasForTesting().drawImage(images[0].paint_image(), 0u, 0u,
                                            SkSamplingOptions(), nullptr);

  static_cast<WebGraphicsContext3DProviderWrapper::DestructionObserver*>(
      provider.get())
      ->OnContextDestroyed();
  // We should unref all images on the cache when the context is destroyed.
  EXPECT_EQ(image_decode_cache_.num_locked_images(), 0);
  image_decode_cache_.set_disallow_cache_use(true);
  provider->GetCanvasForTesting().drawImage(images[1].paint_image(), 0u, 0u,
                                            SkSamplingOptions(), nullptr);
}

}  // namespace blink
