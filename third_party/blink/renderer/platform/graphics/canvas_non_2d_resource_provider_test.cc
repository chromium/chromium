// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_non_2d_resource_provider.h"

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "cc/test/stub_decode_cache.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_compositing_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/buffer_types.h"

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

}  // namespace

class CanvasNon2DResourceProviderTest : public testing::Test {
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

  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ImageTrackingDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuCompositingTestPlatform> platform_;
};

TEST_F(CanvasNon2DResourceProviderTest, BeginExternalOverwrite) {
  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(10, 10), color_params, context_provider_wrapper_,
      shared_image_usage_flags);

  gpu::SyncToken sync_token;

  // The same ClientSharedImage should be returned from sequential calls to
  // BeginExternalOverwrite().
  auto client_si = provider->BeginExternalOverwrite(sync_token);
  provider->EndExternalWrite(sync_token);
  auto client_si_from_second_call =
      provider->BeginExternalOverwrite(sync_token);
  EXPECT_EQ(client_si_from_second_call, client_si);
}

TEST_F(CanvasNon2DResourceProviderTest, AcceleratedOverlay) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasNon2DResourceProvider::Create(
      kSize, color_params, context_provider_wrapper_, shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider && provider->IsValid());
  EXPECT_FALSE(provider->IsSoftware());
  EXPECT_TRUE(provider->IsSingleBuffered());
  // As it is an accelerated canvas, it will internally force it to RGBA8 on
  // MacOS, or otherwise RGBA8 if not on Windows
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(GetSkImageInfo(provider.get()) ==
              kInfo.makeColorType(kBGRA_8888_SkColorType));
#elif !BUILDFLAG(IS_WIN)
  EXPECT_TRUE(GetSkImageInfo(provider.get()) ==
              kInfo.makeColorType(kRGBA_8888_SkColorType));
#else
  EXPECT_TRUE(GetSkImageInfo(provider.get()) == kInfo);
#endif
}

TEST_F(CanvasNon2DResourceProviderTest, Texture) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasNon2DResourceProvider::Create(
      kSize, color_params, context_provider_wrapper_,
      gpu::SharedImageUsageSet());

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider && provider->IsValid());
  EXPECT_FALSE(provider->IsSoftware());
  EXPECT_FALSE(provider->IsSingleBuffered());
  // As it is an accelerated canvas, it will internally force it to kRGBA8
  EXPECT_EQ(GetSkImageInfo(provider.get()),
            kInfo.makeColorType(kRGBA_8888_SkColorType));

  EXPECT_FALSE(provider->IsSingleBuffered());
}

class CanvasNon2DResourceProviderSyncTokenTest
    : public CanvasNon2DResourceProviderTest,
      public testing::WithParamInterface<bool> {
 public:
  CanvasNon2DResourceProviderSyncTokenTest() {
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
                         CanvasNon2DResourceProviderSyncTokenTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "AutomaticSyncTokens"
                                             : "ManualSyncTokens";
                         });

TEST_P(CanvasNon2DResourceProviderSyncTokenTest, EndExternalWrite) {
  // Set up this test to use GPU rasterization to be able to verify
  // conditions against the test raster interface.
  SharedGpuContext::Reset();
  auto raster_context_provider = viz::TestContextProvider::CreateRaster();
  InitializeSharedGpuContext(raster_context_provider.get(),
                             &image_decode_cache_,
                             SetIsContextLost::kSetToFalse);

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(10, 10), color_params,
      SharedGpuContext::ContextProviderWrapper(), shared_image_usage_flags);

  auto resource = provider->ProduceCanvasResource();
  auto old_compositor_read_sync_token = GetSyncToken(resource.get());

  // NOTE: Need to ensure that this SyncToken's release count is greater than
  // that of the last one that TestRasterInterface waited on for
  // TestRasterInterface to set this token as `last_waited_sync_token_` when it
  // waits on the token.
  gpu::SyncToken external_write_sync_token(gpu::CommandBufferNamespace::GPU_IO,
                                           gpu::CommandBufferId(), 42);

  provider->EndExternalWrite(external_write_sync_token);

  if (base::FeatureList::IsEnabled(
          features::kUseAutomaticSyncTokenManagement)) {
    // The client-visible SyncToken becomes empty after EndAccess.
    EXPECT_FALSE(GetSyncToken(resource.get()).HasData());
  } else {
    // EndExternalWrite() should have initiated a wait on
    // `external_write_sync_token` on the raster interface.
    EXPECT_EQ(raster_context_provider->GetTestRasterInterface()
                  ->last_waited_sync_token(),
              external_write_sync_token);

    // In addition, it should have ensured that the resource generates a new
    // compositor read sync token on the next request for that token.
    EXPECT_NE(GetSyncToken(resource.get()), old_compositor_read_sync_token);
  }
}

TEST_F(CanvasNon2DResourceProviderTest, SoftwareSharedImage_GPUCompositing) {
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider =
          TestWebGraphicsSharedImageInterfaceProvider::Create();

  EXPECT_FALSE(CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
      gfx::Size(10, 10),
      Canvas2DColorParams(PredefinedColorSpace::kSRGB, gfx::HDRMetadata(),
                          CanvasPixelFormat::kUint8, /*has_alpha=*/true),
      test_web_shared_image_interface_provider.get()));
}

TEST_F(CanvasNon2DResourceProviderTest, SoftwareSharedImage_SWCompositing) {
  platform_->SetGpuCompositingDisabled(true);

  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider =
          TestWebGraphicsSharedImageInterfaceProvider::Create();

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
      kSize, color_params, test_web_shared_image_interface_provider.get());

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider && provider->IsValid());
  EXPECT_TRUE(provider->IsSoftware());
  EXPECT_TRUE(GetSkImageInfo(provider.get()) == kInfo);

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasNon2DResourceProviderTest,
       ConcurrentReadWriteUsageResultsInSingleBuffering) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasNon2DResourceProvider::Create(
      kSize, color_params, context_provider_wrapper_, shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider && provider->IsValid());
  EXPECT_FALSE(provider->IsSoftware());
  EXPECT_TRUE(provider->IsSingleBuffered());
  // As it is an accelerated canvas, it will internally force it to RGBA8 on
  // MacOS, or otherwise RGBA8 if not on Windows
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(GetSkImageInfo(provider.get()) ==
              kInfo.makeColorType(kBGRA_8888_SkColorType));
#elif !BUILDFLAG(IS_WIN)
  EXPECT_TRUE(GetSkImageInfo(provider.get()) ==
              kInfo.makeColorType(kRGBA_8888_SkColorType));
#else
  EXPECT_TRUE(GetSkImageInfo(provider.get()) == kInfo);
#endif
}

TEST_F(CanvasNon2DResourceProviderTest,
       DimensionsExceedMaxTextureSize_SharedImage) {
  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(kMaxTextureSize - 1, kMaxTextureSize), color_params,
      context_provider_wrapper_, gpu::SharedImageUsageSet());
  EXPECT_TRUE(provider && provider->IsValid());
  provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(kMaxTextureSize, kMaxTextureSize), color_params,
      context_provider_wrapper_, gpu::SharedImageUsageSet());
  EXPECT_TRUE(provider && provider->IsValid());
  provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(kMaxTextureSize + 1, kMaxTextureSize), color_params,
      context_provider_wrapper_, gpu::SharedImageUsageSet());
  // The provider should not be created if the texture size is greater than
  // the maximum value.
  EXPECT_FALSE(provider);
}

}  // namespace blink
