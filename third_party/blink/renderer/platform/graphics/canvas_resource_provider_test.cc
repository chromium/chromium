// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "components/viz/common/resources/release_callback.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_params.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "ui/gfx/buffer_types.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace blink {

namespace {

constexpr int kMaxTextureSize = 1024;

class MockCanvasResourceDispatcherClient
    : public CanvasResourceDispatcherClient {
 public:
  MockCanvasResourceDispatcherClient() = default;

  MOCK_METHOD0(BeginFrame, bool());
  MOCK_METHOD1(SetFilterQualityInResource, void(SkFilterQuality));
};

}  // anonymous namespace

class CanvasResourceProviderTest : public Test {
 public:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::Create();
    auto* test_gl = test_context_provider_->UnboundTestContextGL();
    test_gl->set_max_texture_size(kMaxTextureSize);
    test_gl->set_support_texture_storage_image(true);
    test_gl->set_supports_shared_image_swap_chain(true);
    test_gl->set_supports_gpu_memory_buffer_format(gfx::BufferFormat::RGBA_8888,
                                                   true);
    test_gl->set_supports_gpu_memory_buffer_format(gfx::BufferFormat::BGRA_8888,
                                                   true);
    test_gl->set_supports_gpu_memory_buffer_format(gfx::BufferFormat::RGBA_F16,
                                                   true);
    InitializeSharedGpuContext(test_context_provider_.get(),
                               &image_decode_cache_);
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }

 protected:
  cc::StubDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderAcceleratedOverlay) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->ColorParams().GetSkColorType(), kRGBA_8888_SkColorType);
  EXPECT_EQ(provider->ColorParams().GetSkAlphaType(),
            kColorParams.GetSkAlphaType());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderTexture) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->ColorParams().GetSkColorType(), kRGBA_8888_SkColorType);
  EXPECT_EQ(provider->ColorParams().GetSkAlphaType(),
            kColorParams.GetSkAlphaType());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderUnacceleratedOverlay) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kCPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());

  // We do not support single buffering for unaccelerated low latency canvas.
  EXPECT_FALSE(provider->SupportsSingleBuffering());

  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().GetSkColorType(),
            kColorParams.GetSkColorType());
  EXPECT_EQ(provider->ColorParams().GetSkAlphaType(),
            kColorParams.GetSkAlphaType());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageResourceRecycling) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_FALSE(provider->IsSingleBuffered());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->ColorParams().GetSkColorType(), kRGBA_8888_SkColorType);
  EXPECT_EQ(provider->ColorParams().GetSkAlphaType(),
            kColorParams.GetSkAlphaType());

  // Same resource and sync token if we query again without updating.
  auto resource = provider->ProduceCanvasResource();
  auto sync_token = resource->GetSyncToken();
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource, provider->ProduceCanvasResource());
  EXPECT_EQ(sync_token, resource->GetSyncToken());

  // Resource updated after draw.
  provider->Canvas()->clear(SK_ColorWHITE);
  auto new_resource = provider->ProduceCanvasResource();
  EXPECT_NE(resource, new_resource);
  EXPECT_NE(sync_token, new_resource->GetSyncToken());

  // Resource recycled.
  viz::TransferableResource transferable_resource;
  viz::ReleaseCallback release_callback;
  ASSERT_TRUE(resource->PrepareTransferableResource(
      &transferable_resource, &release_callback, kUnverifiedSyncToken));
  auto* resource_ptr = resource.get();
  resource = nullptr;
  std::move(release_callback).Run(sync_token, false);

  provider->Canvas()->clear(SK_ColorBLACK);
  auto resource_again = provider->ProduceCanvasResource();
  EXPECT_EQ(resource_ptr, resource_again);
  EXPECT_NE(sync_token, resource_again->GetSyncToken());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageStaticBitmapImage) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  ASSERT_TRUE(provider->IsValid());

  // Same resource returned until the canvas is updated.
  auto image = provider->Snapshot();
  ASSERT_TRUE(image);
  auto new_image = provider->Snapshot();
  EXPECT_EQ(image->GetMailboxHolder().mailbox,
            new_image->GetMailboxHolder().mailbox);
  EXPECT_EQ(provider->ProduceCanvasResource()->GetOrCreateGpuMailbox(
                kOrderingBarrier),
            image->GetMailboxHolder().mailbox);

  // Resource updated after draw.
  provider->Canvas()->clear(SK_ColorWHITE);
  provider->FlushCanvas();
  new_image = provider->Snapshot();
  EXPECT_NE(new_image->GetMailboxHolder().mailbox,
            image->GetMailboxHolder().mailbox);

  // Resource recycled.
  auto original_mailbox = image->GetMailboxHolder().mailbox;
  image.reset();
  provider->Canvas()->clear(SK_ColorBLACK);
  provider->FlushCanvas();
  EXPECT_EQ(original_mailbox, provider->Snapshot()->GetMailboxHolder().mailbox);
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageCopyOnWriteDisabled) {
  auto* fake_context = static_cast<FakeWebGraphicsContext3DProvider*>(
      context_provider_wrapper_->ContextProvider());
  auto caps = fake_context->GetCapabilities();
  caps.disable_2d_canvas_copy_on_write = true;
  fake_context->SetCapabilities(caps);

  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU,
      true /* is_origin_top_left */, shared_image_usage_flags);

  ASSERT_TRUE(provider->IsValid());

  // Disabling copy-on-write forces a copy each time the resource is queried.
  auto resource = provider->ProduceCanvasResource();
  EXPECT_NE(resource->GetOrCreateGpuMailbox(kOrderingBarrier),
            provider->ProduceCanvasResource()->GetOrCreateGpuMailbox(
                kOrderingBarrier));
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderBitmap) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  auto provider = CanvasResourceProvider::CreateBitmapProvider(
      kSize, kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().GetSkColorType(),
            kColorParams.GetSkColorType());
  EXPECT_EQ(provider->ColorParams().GetSkAlphaType(),
            kColorParams.GetSkAlphaType());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderSharedBitmap) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  MockCanvasResourceDispatcherClient client;
  CanvasResourceDispatcher resource_dispatcher(
      &client, 1 /* client_id */, 1 /* sink_id */,
      1 /* placeholder_canvas_id */, kSize);

  auto provider = CanvasResourceProvider::CreateSharedBitmapProvider(
      kSize, kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      resource_dispatcher.GetWeakPtr());

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().GetSkColorType(),
            kColorParams.GetSkColorType());
  EXPECT_EQ(provider->ColorParams().GetSkAlphaType(),
            kColorParams.GetSkAlphaType());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect2DGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->ColorParams().GetSkColorType(), kRGBA_8888_SkColorType);
  EXPECT_EQ(provider->ColorParams().GetSkAlphaType(),
            kColorParams.GetSkAlphaType());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect3DGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  auto provider = CanvasResourceProvider::CreatePassThroughProvider(
      kSize, kLow_SkFilterQuality, kColorParams, context_provider_wrapper_,
      nullptr /*resource_dispatcher */, true /*is_origin_top_left*/);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().GetSkColorType(),
            kColorParams.GetSkColorType());
  EXPECT_EQ(provider->ColorParams().GetSkAlphaType(),
            kColorParams.GetSkAlphaType());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());

  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  scoped_refptr<ExternalCanvasResource> resource =
      ExternalCanvasResource::Create(
          mailbox, viz::ReleaseCallback(), gpu::SyncToken(), kSize,
          GL_TEXTURE_2D, kColorParams,
          SharedGpuContext::ContextProviderWrapper(), provider->CreateWeakPtr(),
          kMedium_SkFilterQuality, true /*is_origin_top_left*/,
          true /*is_overlay_candidate*/);

  // NewOrRecycledResource() would return nullptr before an ImportResource().
  EXPECT_TRUE(provider->ImportResource(resource));
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
  // NewOrRecycledResource() will always return the same |resource|.
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_Bitmap) {
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  auto provider = CanvasResourceProvider::CreateBitmapProvider(
      IntSize(kMaxTextureSize - 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, CanvasResourceProvider::ShouldInitialize::kCallClear);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateBitmapProvider(
      IntSize(kMaxTextureSize, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, CanvasResourceProvider::ShouldInitialize::kCallClear);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateBitmapProvider(
      IntSize(kMaxTextureSize + 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, CanvasResourceProvider::ShouldInitialize::kCallClear);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_SharedImage) {
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      IntSize(kMaxTextureSize - 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSharedImageProvider(
      IntSize(kMaxTextureSize, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSharedImageProvider(
      IntSize(kMaxTextureSize + 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);
  // The CanvasResourceProvider for SharedImage should not be created or valid
  // if the texture size is greater than the maximum value
  EXPECT_TRUE(!provider || !provider->IsValid());
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_SwapChain) {
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);
  auto provider = CanvasResourceProvider::CreateSwapChainProvider(
      IntSize(kMaxTextureSize - 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, nullptr /* resource_dispatcher */,
      true /*is_origin_top_left*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSwapChainProvider(
      IntSize(kMaxTextureSize, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, nullptr /* resource_dispatcher */,
      true /*is_origin_top_left*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSwapChainProvider(
      IntSize(kMaxTextureSize + 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, nullptr /* resource_dispatcher */,
      true /*is_origin_top_left*/);

  // The CanvasResourceProvider for SwapChain should not be created or valid
  // if the texture size is greater than the maximum value
  EXPECT_TRUE(!provider || !provider->IsValid());
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_PassThrough) {
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);
  auto provider = CanvasResourceProvider::CreatePassThroughProvider(
      IntSize(kMaxTextureSize - 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, context_provider_wrapper_,
      nullptr /* resource_dispatcher */, true /*is_origin_top_left*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreatePassThroughProvider(
      IntSize(kMaxTextureSize, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, context_provider_wrapper_,
      nullptr /* resource_dispatcher */, true /*is_origin_top_left*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreatePassThroughProvider(
      IntSize(kMaxTextureSize + 1, kMaxTextureSize), kLow_SkFilterQuality,
      kColorParams, context_provider_wrapper_,
      nullptr /* resource_dispatcher */, true /*is_origin_top_left*/);
  // The CanvasResourceProvider for PassThrough should not be created or valid
  // if the texture size is greater than the maximum value
  EXPECT_TRUE(!provider || !provider->IsValid());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderDirect2DSwapChain) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  auto provider = CanvasResourceProvider::CreateSwapChainProvider(
      kSize, kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, nullptr /* resource_dispatcher */,
      true /*is_origin_top_left*/);

  ASSERT_TRUE(provider);
  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_TRUE(provider->IsSingleBuffered());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().GetSkColorType(),
            kColorParams.GetSkColorType());
  EXPECT_EQ(provider->ColorParams().GetSkAlphaType(),
            kColorParams.GetSkAlphaType());
}

TEST_F(CanvasResourceProviderTest, FlushForImage) {
  const IntSize kSize(10, 10);
  const CanvasResourceParams kColorParams(
      CanvasColorSpace::kSRGB, kN32_SkColorType, kPremul_SkAlphaType);

  auto src_provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);

  auto dst_provider = CanvasResourceProvider::CreateSharedImageProvider(
      kSize, kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);

  MemoryManagedPaintCanvas* dst_canvas =
      static_cast<MemoryManagedPaintCanvas*>(dst_provider->Canvas());

  PaintImage paint_image =
      src_provider->Snapshot()->PaintImageForCurrentFrame();
  PaintImage::ContentId src_content_id = paint_image.GetContentIdForFrame(0u);

  EXPECT_FALSE(dst_canvas->IsCachingImage(src_content_id));

  dst_canvas->drawImage(paint_image, 0, 0, SkSamplingOptions(), nullptr);

  EXPECT_TRUE(dst_canvas->IsCachingImage(src_content_id));

  src_provider->Canvas()->clear(
      SK_ColorWHITE);  // Modify the canvas to trigger OnFlushForImage
  src_provider
      ->ProduceCanvasResource();  // So that all the cached draws are executed

  // The paint canvas may have moved
  dst_canvas = static_cast<MemoryManagedPaintCanvas*>(dst_provider->Canvas());

  // TODO(aaronhk): The resource on the src_provider should be the same before
  // and after the draw. Something about the program flow within
  // this testing framework (but not in layout tests) makes a reference to
  // the src_resource stick around throughout the FlushForImage call so the
  // src_resource changes in this test. Things work as expected for actual
  // browser code like canvas_to_canvas_draw.html.

  // OnFlushForImage should detect the modification of the source resource and
  // clear the cache of the destination canvas to avoid a copy-on-write.
  EXPECT_FALSE(dst_canvas->IsCachingImage(src_content_id));
}

}  // namespace blink
