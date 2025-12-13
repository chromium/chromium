// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

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
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
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

class CanvasResourceProviderTest : public Test {
 public:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::CreateRaster();
    auto* test_raster = test_context_provider_->UnboundTestRasterInterface();
    test_raster->set_max_texture_size(kMaxTextureSize);
    test_raster->set_supports_gpu_memory_buffer_format(
        gfx::BufferFormat::RGBA_8888, true);
    test_raster->set_supports_gpu_memory_buffer_format(
        gfx::BufferFormat::BGRA_8888, true);
    test_raster->set_supports_gpu_memory_buffer_format(
        gfx::BufferFormat::RGBA_F16, true);

    gpu::SharedImageCapabilities shared_image_caps;
    shared_image_caps.supports_scanout_shared_images = true;
    shared_image_caps.shared_image_swap_chain = true;
    test_context_provider_->SharedImageInterface()->SetCapabilities(
        shared_image_caps);

    InitializeSharedGpuContextRaster(test_context_provider_.get(),
                                     &image_decode_cache_);
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::Reset(); }

 protected:
  const gpu::SyncToken& GetSyncToken(const CanvasResource* resource) {
    return resource->sync_token();
  }

  void EnsureResourceRecycled(CanvasResourceProvider* provider,
                              scoped_refptr<CanvasResource>&& resource) {
    viz::TransferableResource transferable_resource;
    CanvasResource::ReleaseCallback release_callback;
    CHECK(resource->PrepareTransferableResource(
        &transferable_resource, &release_callback,
        /*needs_verified_synctoken=*/false));
    std::move(release_callback)
        .Run(std::move(resource), resource->sync_token(), false);
  }

  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ImageTrackingDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

TEST_F(CanvasResourceProviderTest,
       GetBackingClientSharedImageForExternalWrite) {
  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      gfx::Size(10, 10), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, shared_image_usage_flags);

  gpu::SyncToken sync_token;
  auto client_si = provider->GetBackingClientSharedImageForExternalWrite(
      gpu::SharedImageUsageSet(), sync_token);

  // When supplied required usages that the backing SI already supports, that
  // backing SI should be returned.
  auto client_si_with_no_new_usage_required =
      provider->GetBackingClientSharedImageForExternalWrite(
          gpu::SHARED_IMAGE_USAGE_SCANOUT, sync_token);
  EXPECT_EQ(client_si_with_no_new_usage_required, client_si);

  // When supplied required usages that the backing SI does not support, a new
  // backing SI should be created that supports the required usages.
  auto client_si_with_webgpu_usage_required =
      provider->GetBackingClientSharedImageForExternalWrite(
          gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE, sync_token);
  EXPECT_NE(client_si_with_webgpu_usage_required, client_si);
  EXPECT_TRUE(client_si_with_webgpu_usage_required->usage().HasAll(
      shared_image_usage_flags));
  EXPECT_TRUE(client_si_with_webgpu_usage_required->usage().Has(
      gpu::SHARED_IMAGE_USAGE_WEBGPU_WRITE));

  // That new backing SI should then be returned on subsequent calls with
  // already-supported usages.
  client_si_with_no_new_usage_required =
      provider->GetBackingClientSharedImageForExternalWrite(
          gpu::SHARED_IMAGE_USAGE_SCANOUT, sync_token);
  EXPECT_EQ(client_si_with_no_new_usage_required,
            client_si_with_webgpu_usage_required);
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderAcceleratedOverlay) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, color_params, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->IsSingleBuffered());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to RGBA8 on MacOS, or otherwise RGBA8 if not on
  // Windows
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kBGRA_8888_SkColorType));
#elif !BUILDFLAG(IS_WIN)
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kRGBA_8888_SkColorType));
#else
  EXPECT_TRUE(provider->GetSkImageInfo() == kInfo);
#endif
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderTexture) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, color_params, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, gpu::SharedImageUsageSet());

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->IsSingleBuffered());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->GetSkImageInfo(),
            kInfo.makeColorType(kRGBA_8888_SkColorType));

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderUnacceleratedOverlay) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, color_params, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kCPU, shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());

  // We do not support single buffering for unaccelerated low latency canvas.
  EXPECT_FALSE(provider->IsSingleBuffered());

  EXPECT_EQ(provider->GetSkImageInfo(), kInfo);

  EXPECT_FALSE(provider->IsSingleBuffered());
}

std::unique_ptr<CanvasResourceProviderSharedImage> MakeCanvasResourceProvider(
    RasterMode raster_mode,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  return CanvasResourceProvider::CreateSharedImageProvider(
      gfx::Size(10, 10), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper, raster_mode, shared_image_usage_flags);
}

scoped_refptr<CanvasResource> UpdateResource(CanvasResourceProvider* provider) {
  provider->ProduceCanvasResource(FlushReason::kOther);
  // Resource updated after draw.
  provider->Canvas().clear(SkColors::kWhite);
  return provider->ProduceCanvasResource(FlushReason::kOther);
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageEndExternalWrite) {
  // Set up this test to use OOP rasterization to be able to verify
  // conditions against the test raster interface.
  SharedGpuContext::Reset();
  auto raster_context_provider = viz::TestContextProvider::CreateRaster();
  InitializeSharedGpuContextRaster(raster_context_provider.get(),
                                   &image_decode_cache_,
                                   SetIsContextLost::kSetToFalse);

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      gfx::Size(10, 10), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
      shared_image_usage_flags);

  auto resource = provider->ProduceCanvasResource(FlushReason::kOther);
  auto old_compositor_read_sync_token = GetSyncToken(resource.get());

  // NOTE: Need to ensure that this SyncToken's release count is greater than
  // that of the last one that TestRasterInterface waited on for
  // TestRasterInterface to set this token as `last_waited_sync_token_` when it
  // waits on the token.
  gpu::SyncToken external_write_sync_token(gpu::CommandBufferNamespace::GPU_IO,
                                           gpu::CommandBufferId(), 42);

  provider->EndExternalWrite(external_write_sync_token);

  // EndExternalWrite() should have initiated a wait on
  // `external_write_sync_token` on the raster interface.
  EXPECT_EQ(raster_context_provider->GetTestRasterInterface()
                ->last_waited_sync_token(),
            external_write_sync_token);

  // In addition, it should have ensured that the resource generates a new
  // compositor read sync token on the next request for that token.
  EXPECT_NE(GetSyncToken(resource.get()), old_compositor_read_sync_token);
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageResourceRecycling) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, color_params, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_FALSE(provider->IsSingleBuffered());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to RGBA8, or BGRA8 on MacOS
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kBGRA_8888_SkColorType));
#else
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kRGBA_8888_SkColorType));
#endif

  // Same resource and sync token if we query again without updating.
  auto resource = provider->ProduceCanvasResource(FlushReason::kOther);
  auto sync_token = GetSyncToken(resource.get());
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource, provider->ProduceCanvasResource(FlushReason::kOther));
  EXPECT_EQ(sync_token, GetSyncToken(resource.get()));

  auto new_resource = UpdateResource(provider.get());
  EXPECT_NE(resource, new_resource);
  EXPECT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  auto* resource_ptr = resource.get();

  EnsureResourceRecycled(provider.get(), std::move(resource));

  provider->Canvas().clear(SkColors::kBlack);
  auto resource_again = provider->ProduceCanvasResource(FlushReason::kOther);
  EXPECT_EQ(resource_ptr, resource_again);
  EXPECT_NE(sync_token, GetSyncToken(resource_again.get()));
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderUnusedResources) {
  base::test::ScopedFeatureList feature_list{kCanvas2DReclaimUnusedResources};

  auto provider =
      MakeCanvasResourceProvider(RasterMode::kGPU, context_provider_wrapper_);

  auto resource = provider->ProduceCanvasResource(FlushReason::kOther);
  auto new_resource = UpdateResource(provider.get());
  ASSERT_NE(resource, new_resource);

  ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));

  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(provider.get(), std::move(resource));
  // The reclaim task has been posted.
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  // There is a ready-to-reuse resource
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  task_environment_.FastForwardBy(
      CanvasResourceProviderSharedImage::kUnusedResourceExpirationTime);
  // The resource is freed, don't repost the task.
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDontReclaimUnusedResourcesWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kCanvas2DReclaimUnusedResources);

  auto provider =
      MakeCanvasResourceProvider(RasterMode::kGPU, context_provider_wrapper_);

  auto resource = provider->ProduceCanvasResource(FlushReason::kOther);
  auto new_resource = UpdateResource(provider.get());
  ASSERT_NE(resource, new_resource);
  ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(provider.get(), std::move(resource));
  // There is a ready-to-reuse resource
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  // No task posted.
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderUnusedResourcesAreNotCollectedWhenYoung) {
  base::test::ScopedFeatureList feature_list{kCanvas2DReclaimUnusedResources};

  auto provider =
      MakeCanvasResourceProvider(RasterMode::kGPU, context_provider_wrapper_);

  auto resource = provider->ProduceCanvasResource(FlushReason::kOther);
  auto new_resource = UpdateResource(provider.get());
  ASSERT_NE(resource, new_resource);
  ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
  EnsureResourceRecycled(provider.get(), std::move(resource));
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  // There is a ready-to-reuse resource
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  task_environment_.FastForwardBy(
      CanvasResourceProviderSharedImage::kUnusedResourceExpirationTime -
      base::Seconds(1));
  // The reclaim task hasn't run yet.
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  resource = UpdateResource(provider.get());
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  new_resource = UpdateResource(provider.get());
  ASSERT_NE(resource, new_resource);
  ASSERT_NE(GetSyncToken(resource.get()), GetSyncToken(new_resource.get()));

  EnsureResourceRecycled(provider.get(), std::move(resource));
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  task_environment_.FastForwardBy(base::Seconds(1));

  // Too young, no release yet.
  EXPECT_TRUE(provider->HasUnusedResourcesForTesting());
  // But re-post the task to free it.
  EXPECT_TRUE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());

  task_environment_.FastForwardBy(
      CanvasResourceProviderSharedImage::kUnusedResourceExpirationTime);
  // Now it's collected.
  EXPECT_FALSE(provider->HasUnusedResourcesForTesting());
  // And no new task is posted.
  EXPECT_FALSE(
      provider->unused_resources_reclaim_timer_is_running_for_testing());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageStaticBitmapImage) {
  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      gfx::Size(10, 10), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, shared_image_usage_flags);

  ASSERT_TRUE(provider->IsValid());

  // Same resource returned until the canvas is updated.
  auto image = provider->Snapshot();
  ASSERT_TRUE(image);
  auto new_image = provider->Snapshot();
  EXPECT_EQ(image->GetSharedImage(), new_image->GetSharedImage());
  EXPECT_EQ(provider->ProduceCanvasResource(FlushReason::kOther)
                ->GetClientSharedImage(),
            image->GetSharedImage());

  // Resource updated after draw.
  provider->Canvas().clear(SkColors::kWhite);
  provider->FlushCanvas(FlushReason::kOther);
  new_image = provider->Snapshot();
  EXPECT_NE(new_image->GetSharedImage(), image->GetSharedImage());

  // Resource recycled.
  auto original_shared_image = image->GetSharedImage();
  image.reset();
  provider->Canvas().clear(SkColors::kBlack);
  provider->FlushCanvas(FlushReason::kOther);
  EXPECT_EQ(original_shared_image, provider->Snapshot()->GetSharedImage());
}

TEST_F(CanvasResourceProviderTest, Canvas2DResourceProviderBitmap) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = Canvas2DResourceProviderBitmap::CreateForTesting(
      kSize, color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->GetSkImageInfo() == kInfo);

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSoftwareSharedImage_GPUCompositing) {
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider =
          TestWebGraphicsSharedImageInterfaceProvider::Create();

  EXPECT_FALSE(
      CanvasResourceProvider::CreateSharedImageProviderForSoftwareCompositor(
          gfx::Size(10, 10),
          Canvas2DColorParams(PredefinedColorSpace::kSRGB,
                              CanvasPixelFormat::kUint8, /*has_alpha=*/true),
          CanvasResourceProvider::ShouldInitialize::kCallClear,
          test_web_shared_image_interface_provider.get()));
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSoftwareSharedImage_SWCompositing) {
  platform_->SetGpuCompositingDisabled(true);

  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider =
          TestWebGraphicsSharedImageInterfaceProvider::Create();

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider =
      CanvasResourceProvider::CreateSharedImageProviderForSoftwareCompositor(
          kSize, color_params,
          CanvasResourceProvider::ShouldInitialize::kCallClear,
          test_web_shared_image_interface_provider.get());

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->GetSkImageInfo() == kInfo);

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect2DGpuMemoryBuffer) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, color_params, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->IsSingleBuffered());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to RGBA8 on MacOS, or otherwise RGBA8 if not on
  // Windows
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kBGRA_8888_SkColorType));
#elif !BUILDFLAG(IS_WIN)
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kRGBA_8888_SkColorType));
#else
  EXPECT_TRUE(provider->GetSkImageInfo() == kInfo);
#endif
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_Bitmap) {
  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = Canvas2DResourceProviderBitmap::CreateForTesting(
      gfx::Size(kMaxTextureSize - 1, kMaxTextureSize), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  provider = Canvas2DResourceProviderBitmap::CreateForTesting(
      gfx::Size(kMaxTextureSize, kMaxTextureSize), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  provider = Canvas2DResourceProviderBitmap::CreateForTesting(
      gfx::Size(kMaxTextureSize + 1, kMaxTextureSize), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_SharedImage) {
  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      gfx::Size(kMaxTextureSize - 1, kMaxTextureSize), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, gpu::SharedImageUsageSet());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSharedImageProvider(
      gfx::Size(kMaxTextureSize, kMaxTextureSize), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, gpu::SharedImageUsageSet());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSharedImageProvider(
      gfx::Size(kMaxTextureSize + 1, kMaxTextureSize), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, gpu::SharedImageUsageSet());
  // The CanvasResourceProvider for SharedImage should not be created or valid
  // if the texture size is greater than the maximum value
  EXPECT_TRUE(!provider || !provider->IsValid());
}

TEST_F(CanvasResourceProviderTest, FlushForImage) {
  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto src_provider = CanvasResourceProvider::CreateSharedImageProvider(
      gfx::Size(10, 10), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, gpu::SharedImageUsageSet());

  auto dst_provider = CanvasResourceProvider::CreateSharedImageProvider(
      gfx::Size(10, 10), color_params,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, gpu::SharedImageUsageSet());

  MemoryManagedPaintCanvas& dst_canvas = dst_provider->Canvas();

  PaintImage paint_image =
      src_provider->Snapshot()->PaintImageForCurrentFrame();
  PaintImage::ContentId src_content_id = paint_image.GetContentIdForFrame(0u);

  EXPECT_FALSE(dst_canvas.IsCachingImage(src_content_id));

  dst_canvas.drawImage(paint_image, 0, 0, SkSamplingOptions(), nullptr);

  EXPECT_TRUE(dst_canvas.IsCachingImage(src_content_id));

  // Modify the canvas to trigger OnFlushForImage
  src_provider->Canvas().clear(SkColors::kWhite);
  // So that all the cached draws are executed
  src_provider->ProduceCanvasResource(FlushReason::kOther);

  // The paint canvas may have moved
  MemoryManagedPaintCanvas& new_dst_canvas = dst_provider->Canvas();

  // TODO(aaronhk): The resource on the src_provider should be the same before
  // and after the draw. Something about the program flow within
  // this testing framework (but not in layout tests) makes a reference to
  // the src_resource stick around throughout the FlushForImage call so the
  // src_resource changes in this test. Things work as expected for actual
  // browser code like canvas_to_canvas_draw.html.

  // OnFlushForImage should detect the modification of the source resource and
  // clear the cache of the destination canvas to avoid a copy-on-write.
  EXPECT_FALSE(new_dst_canvas.IsCachingImage(src_content_id));
}

TEST_F(CanvasResourceProviderTest, ImageCacheOnContextLost) {
  auto provider =
      MakeCanvasResourceProvider(RasterMode::kGPU, context_provider_wrapper_);

  Vector<cc::DrawImage> images = {
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(10, 10)), false,
                    SkIRect::MakeWH(10, 10),
                    cc::PaintFlags::FilterQuality::kNone, SkM44(), 0u,
                    cc::TargetColorParams()),
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(20, 20)), false,
                    SkIRect::MakeWH(5, 5), cc::PaintFlags::FilterQuality::kNone,
                    SkM44(), 0u, cc::TargetColorParams())};
  provider->Canvas().drawImage(images[0].paint_image(), 0u, 0u,
                               SkSamplingOptions(), nullptr);

  // Lose the context and ensure that the image provider is not used.
  provider->OnContextDestroyed();
  // We should unref all images on the cache when the context is destroyed.
  EXPECT_EQ(image_decode_cache_.num_locked_images(), 0);
  image_decode_cache_.set_disallow_cache_use(true);
  provider->Canvas().drawImage(images[1].paint_image(), 0u, 0u,
                               SkSamplingOptions(), nullptr);
}

TEST_F(CanvasResourceProviderTest, FlushCanvasReleasesAllReleasableOps) {
  std::unique_ptr<CanvasResourceProvider> provider =
      MakeCanvasResourceProvider(RasterMode::kGPU, context_provider_wrapper_);

  EXPECT_FALSE(provider->Recorder().HasRecordedDrawOps());
  EXPECT_FALSE(provider->Recorder().HasReleasableDrawOps());

  provider->Canvas().drawRect({0, 0, 10, 10}, cc::PaintFlags());
  EXPECT_TRUE(provider->Recorder().HasRecordedDrawOps());
  EXPECT_TRUE(provider->Recorder().HasReleasableDrawOps());

  // `FlushCanvas` releases all ops, leaving the canvas clean.
  provider->FlushCanvas(FlushReason::kOther);
  EXPECT_FALSE(provider->Recorder().HasRecordedDrawOps());
  EXPECT_FALSE(provider->Recorder().HasReleasableDrawOps());
}

TEST_F(CanvasResourceProviderTest, FlushCanvasReleasesAllOpsOutsideLayers) {
  std::unique_ptr<CanvasResourceProvider> provider =
      MakeCanvasResourceProvider(RasterMode::kGPU, context_provider_wrapper_);

  EXPECT_FALSE(provider->Recorder().HasRecordedDrawOps());
  EXPECT_FALSE(provider->Recorder().HasReleasableDrawOps());
  EXPECT_FALSE(provider->Recorder().HasSideRecording());

  // Side canvases (used for canvas 2d layers) cannot be flushed until closed.
  // Open one and validate that flushing the canvas only flushed that main
  // recording, not the side one.
  provider->Canvas().drawRect({0, 0, 10, 10}, cc::PaintFlags());
  provider->Recorder().BeginSideRecording();
  provider->Canvas().saveLayerAlphaf(0.5f);
  provider->Canvas().drawRect({0, 0, 10, 10}, cc::PaintFlags());
  EXPECT_TRUE(provider->Recorder().HasRecordedDrawOps());
  EXPECT_TRUE(provider->Recorder().HasReleasableDrawOps());
  EXPECT_TRUE(provider->Recorder().HasSideRecording());

  provider->FlushCanvas(FlushReason::kOther);
  EXPECT_TRUE(provider->Recorder().HasRecordedDrawOps());
  EXPECT_FALSE(provider->Recorder().HasReleasableDrawOps());
  EXPECT_TRUE(provider->Recorder().HasSideRecording());

  provider->Canvas().restore();
  EXPECT_TRUE(provider->Recorder().HasRecordedDrawOps());
  EXPECT_FALSE(provider->Recorder().HasReleasableDrawOps());
  EXPECT_TRUE(provider->Recorder().HasSideRecording());

  provider->Recorder().EndSideRecording();
  EXPECT_TRUE(provider->Recorder().HasRecordedDrawOps());
  EXPECT_TRUE(provider->Recorder().HasReleasableDrawOps());
  EXPECT_FALSE(provider->Recorder().HasSideRecording());

  provider->FlushCanvas(FlushReason::kOther);
  EXPECT_FALSE(provider->Recorder().HasRecordedDrawOps());
  EXPECT_FALSE(provider->Recorder().HasReleasableDrawOps());
  EXPECT_FALSE(provider->Recorder().HasSideRecording());
}

}  // namespace blink
