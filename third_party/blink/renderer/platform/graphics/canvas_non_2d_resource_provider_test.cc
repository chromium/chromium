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
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
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
      viz::ToClosestSkColorType(provider->GetSharedImageFormatForTesting()),
      provider->GetAlphaTypeForTesting(),
      provider->GetColorSpaceForTesting().ToSkColorSpace());
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
    test_context_provider_->SharedImageInterface()->SetCapabilities(
        shared_image_caps);

    InitializeSharedGpuContext(test_context_provider_.get(),
                               &image_decode_cache_);
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::Reset(); }

 protected:
  const gpu::SyncToken& GetSyncToken(const ExportedCanvasResource* resource) {
    return resource->GetResourceForTesting()->sync_token();
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
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(10, 10), GetN32FormatForCanvas(), kPremul_SkAlphaType,
      gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(),
      context_provider_wrapper_, shared_image_usage_flags);

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
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasNon2DResourceProvider::Create(
      kSize, GetN32FormatForCanvas(), kPremul_SkAlphaType,
      gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(),
      context_provider_wrapper_, shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider && provider->IsValid());
  EXPECT_FALSE(provider->IsSoftware());
  EXPECT_TRUE(provider->GetSharedImageUsageFlagsForTesting().Has(
      gpu::SHARED_IMAGE_USAGE_SCANOUT));
  // As it is an accelerated canvas, it will internally force it to BGRA8 on
  // MacOS when scanout is used, or RGBA8 otherwise.
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(GetSkImageInfo(provider.get()) ==
              kInfo.makeColorType(kBGRA_8888_SkColorType));
#else
  EXPECT_TRUE(GetSkImageInfo(provider.get()) ==
              kInfo.makeColorType(kRGBA_8888_SkColorType));
#endif
}

TEST_F(CanvasNon2DResourceProviderTest, Texture) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  auto provider = CanvasNon2DResourceProvider::Create(
      kSize, GetN32FormatForCanvas(), kPremul_SkAlphaType,
      gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(),
      context_provider_wrapper_, gpu::SharedImageUsageSet());

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider && provider->IsValid());
  EXPECT_FALSE(provider->IsSoftware());
  EXPECT_FALSE(provider->GetSharedImageUsageFlagsForTesting().Has(
      gpu::SHARED_IMAGE_USAGE_SCANOUT));
  // As it is an accelerated canvas, it will internally force it to kRGBA8
  EXPECT_EQ(GetSkImageInfo(provider.get()),
            kInfo.makeColorType(kRGBA_8888_SkColorType));
}

TEST_F(CanvasNon2DResourceProviderTest, EndExternalWrite) {
  const gpu::SharedImageUsageSet shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
  auto provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(10, 10), GetN32FormatForCanvas(), kPremul_SkAlphaType,
      gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(),
      context_provider_wrapper_, shared_image_usage_flags);

  gpu::SyncToken external_write_sync_token(gpu::CommandBufferNamespace::GPU_IO,
                                           gpu::CommandBufferId(), 42);

  provider->EndExternalWrite(external_write_sync_token);

  // EndExternalWrite() should update the acquire sync token for subsequent
  // writes and set the release sync token for compositor reads.
  gpu::SyncToken acquire_sync_token;
  provider->BeginExternalOverwrite(acquire_sync_token);
  EXPECT_EQ(acquire_sync_token, external_write_sync_token);

  auto resource = provider->ProduceCanvasResource();
  EXPECT_EQ(GetSyncToken(resource.get()), external_write_sync_token);
}

TEST_F(CanvasNon2DResourceProviderTest, SoftwareSharedImage_GPUCompositing) {
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider =
          TestWebGraphicsSharedImageInterfaceProvider::Create();

  EXPECT_FALSE(CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
      gfx::Size(10, 10), GetN32FormatForCanvas(), kPremul_SkAlphaType,
      gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(),
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

  auto provider = CanvasNon2DResourceProvider::CreateForSoftwareCompositor(
      kSize, GetN32FormatForCanvas(), kPremul_SkAlphaType,
      gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(),
      test_web_shared_image_interface_provider.get());

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider && provider->IsValid());
  EXPECT_TRUE(provider->IsSoftware());
  EXPECT_TRUE(GetSkImageInfo(provider.get()) == kInfo);
}

TEST_F(CanvasNon2DResourceProviderTest,
       DimensionsExceedMaxTextureSize_SharedImage) {
  auto provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(kMaxTextureSize - 1, kMaxTextureSize), GetN32FormatForCanvas(),
      kPremul_SkAlphaType, gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(),
      context_provider_wrapper_, gpu::SharedImageUsageSet());
  EXPECT_TRUE(provider && provider->IsValid());
  provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(kMaxTextureSize, kMaxTextureSize), GetN32FormatForCanvas(),
      kPremul_SkAlphaType, gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(),
      context_provider_wrapper_, gpu::SharedImageUsageSet());
  EXPECT_TRUE(provider && provider->IsValid());
  provider = CanvasNon2DResourceProvider::Create(
      gfx::Size(kMaxTextureSize + 1, kMaxTextureSize), GetN32FormatForCanvas(),
      kPremul_SkAlphaType, gfx::ColorSpace::CreateSRGB(), gfx::HDRMetadata(),
      context_provider_wrapper_, gpu::SharedImageUsageSet());
  // The provider should not be created if the texture size is greater than
  // the maximum value.
  EXPECT_FALSE(provider);
}

}  // namespace blink
