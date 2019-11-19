// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkFilterQuality.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::Test;

namespace blink {

namespace {

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
    InitializeSharedGpuContext(test_context_provider_.get(),
                               &image_decode_cache_);
    context_provider_wrapper_ = SharedGpuContext::ContextProviderWrapper();
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }

  // Adds |buffer_format| to the context capabilities if it's not supported.
  void EnsureBufferFormatIsSupported(gfx::BufferFormat buffer_format) {
    auto* context_provider = context_provider_wrapper_->ContextProvider();
    if (context_provider->GetCapabilities().gpu_memory_buffer_formats.Has(
            buffer_format)) {
      return;
    }

    auto capabilities = context_provider->GetCapabilities();
    capabilities.gpu_memory_buffer_formats.Add(buffer_format);

    static_cast<FakeWebGraphicsContext3DProvider*>(context_provider)
        ->SetCapabilities(capabilities);
  }

  void EnsureOverlaysSupported() {
    auto* context_provider = context_provider_wrapper_->ContextProvider();
    auto capabilities = context_provider->GetCapabilities();
    capabilities.texture_storage_image = true;
    capabilities.max_texture_size = 1024;
    static_cast<FakeWebGraphicsContext3DProvider*>(context_provider)
        ->SetCapabilities(capabilities);
  }

  bool PlatformSupportsGMBs() {
#if defined(OS_ANDROID)
    return false;
#endif
    return true;
  }

 protected:
  cc::StubDecodeCache image_decode_cache_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform_;
};

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderAcceleratedOverlay) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());
  EnsureOverlaysSupported();

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect2DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_EQ(provider->SupportsSingleBuffering(), PlatformSupportsGMBs());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_EQ(provider->IsSingleBuffered(), PlatformSupportsGMBs());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderTexture) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::ResourceUsage::kAcceleratedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderUnacceleratedOverlay) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());
  EnsureOverlaysSupported();

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::
          kSoftwareCompositedDirect2DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_EQ(provider->SupportsDirectCompositing(), PlatformSupportsGMBs());
  EXPECT_EQ(provider->SupportsSingleBuffering(), PlatformSupportsGMBs());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageResourceRecycling) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::
          kAcceleratedCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_FALSE(provider->IsSingleBuffered());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

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
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  ASSERT_TRUE(resource->PrepareTransferableResource(
      &transferable_resource, &release_callback, kUnverifiedSyncToken));
  auto* resource_ptr = resource.get();
  resource = nullptr;
  release_callback->Run(sync_token, false);

  provider->Canvas()->clear(SK_ColorBLACK);
  auto resource_again = provider->ProduceCanvasResource();
  EXPECT_EQ(resource_ptr, resource_again);
  EXPECT_NE(sync_token, resource_again->GetSyncToken());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageStaticBitmapImage) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::
          kAcceleratedCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);
  ASSERT_TRUE(provider->IsValid());

  // Same resource returned until the canvas is updated.
  auto image = provider->Snapshot();
  ASSERT_TRUE(image);
  auto new_image = provider->Snapshot();
  EXPECT_EQ(image->GetMailbox(), new_image->GetMailbox());
  EXPECT_EQ(provider->ProduceCanvasResource()->GetOrCreateGpuMailbox(
                kOrderingBarrier),
            image->GetMailbox());

  // Resource updated after draw.
  provider->Canvas()->clear(SK_ColorWHITE);
  new_image = provider->Snapshot();
  EXPECT_NE(new_image->GetMailbox(), image->GetMailbox());

  // Resource recycled.
  auto original_mailbox = image->GetMailbox();
  image.reset();
  provider->Canvas()->clear(SK_ColorBLACK);
  EXPECT_EQ(original_mailbox, provider->Snapshot()->GetMailbox());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageCopyOnWriteDisabled) {
  auto* fake_context = static_cast<FakeWebGraphicsContext3DProvider*>(
      context_provider_wrapper_->ContextProvider());
  auto caps = fake_context->GetCapabilities();
  caps.disable_2d_canvas_copy_on_write = true;
  fake_context->SetCapabilities(caps);

  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::
          kAcceleratedCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kMedium_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);
  ASSERT_TRUE(provider->IsValid());

  // Disabling copy-on-write forces a copy each time the resource is queried.
  auto resource = provider->ProduceCanvasResource();
  EXPECT_NE(resource->GetOrCreateGpuMailbox(kOrderingBarrier),
            provider->ProduceCanvasResource()->GetOrCreateGpuMailbox(
                kOrderingBarrier));
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderBitmap) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);

  auto provider = CanvasResourceProvider::Create(
      kSize, CanvasResourceProvider::ResourceUsage::kSoftwareResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderSharedBitmap) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);

  MockCanvasResourceDispatcherClient client;
  CanvasResourceDispatcher resource_dispatcher(
      &client, 1 /* client_id */, 1 /* sink_id */,
      1 /* placeholder_canvas_id */, kSize);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kSoftwareCompositedResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kDefaultPresentationMode,
      resource_dispatcher.GetWeakPtr(), true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect2DGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());
  EnsureOverlaysSupported();

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect2DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_EQ(provider->SupportsSingleBuffering(), PlatformSupportsGMBs());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_EQ(provider->IsSingleBuffered(), PlatformSupportsGMBs());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect3DGpuMemoryBuffer) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect3DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kAllowImageChromiumPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_EQ(provider->SupportsSingleBuffering(), PlatformSupportsGMBs());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_EQ(provider->IsSingleBuffered(), PlatformSupportsGMBs());

  if (!PlatformSupportsGMBs())
    return;
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  scoped_refptr<ExternalCanvasResource> resource =
      ExternalCanvasResource::Create(
          mailbox, kSize, GL_TEXTURE_2D, kColorParams,
          SharedGpuContext::ContextProviderWrapper(), provider->CreateWeakPtr(),
          kMedium_SkFilterQuality);

  // NewOrRecycledResource() would return nullptr before an ImportResource().
  EXPECT_TRUE(provider->ImportResource(resource));
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
  // NewOrRecycledResource() will always return the same |resource|.
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
}

// Verifies that Accelerated Direct 3D resources are backed by SharedImages.
// https://crbug.com/985366
TEST_F(CanvasResourceProviderTest, CanvasResourceProviderDirect3D) {
  const IntSize kSize(10, 10);
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);

  auto provider = CanvasResourceProvider::Create(
      kSize,
      CanvasResourceProvider::ResourceUsage::kAcceleratedDirect3DResourceUsage,
      context_provider_wrapper_, 0 /* msaa_sample_count */,
      kLow_SkFilterQuality, kColorParams,
      CanvasResourceProvider::kDefaultPresentationMode,
      nullptr /* resource_dispatcher */, true /* is_origin_top_left */);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_EQ(provider->ColorParams().ColorSpace(), kColorParams.ColorSpace());
  EXPECT_EQ(provider->ColorParams().PixelFormat(), kColorParams.PixelFormat());
  EXPECT_EQ(provider->ColorParams().GetOpacityMode(),
            kColorParams.GetOpacityMode());

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_FALSE(provider->IsSingleBuffered());

  auto resource = provider->ProduceCanvasResource();
  viz::TransferableResource transferable_resource;
  std::unique_ptr<viz::SingleReleaseCallback> callback;
  resource->PrepareTransferableResource(&transferable_resource, &callback,
                                        kOrderingBarrier);
  EXPECT_TRUE(transferable_resource.mailbox_holder.mailbox.IsSharedImage());
  EXPECT_FALSE(transferable_resource.is_overlay_candidate);
  callback->Run(gpu::SyncToken(), true /* is_lost */);
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize) {
  const CanvasColorParams kColorParams(kSRGBCanvasColorSpace,
                                       kRGBA8CanvasPixelFormat, kNonOpaque,
                                       CanvasForceRGBA::kNotForced);
  const int max_texture_size = context_provider_wrapper_->ContextProvider()
                                   ->GetCapabilities()
                                   .max_texture_size;
  EnsureBufferFormatIsSupported(kColorParams.GetBufferFormat());
  EnsureOverlaysSupported();

  for (int i = 0;
       i < static_cast<int>(CanvasResourceProvider::ResourceUsage::kMaxValue);
       ++i) {
    SCOPED_TRACE(i);
    auto usage = static_cast<CanvasResourceProvider::ResourceUsage>(i);
    bool should_support_compositing = false;
    switch (usage) {
      case CanvasResourceProvider::ResourceUsage::kSoftwareResourceUsage:
        should_support_compositing = false;
        break;
      case CanvasResourceProvider::ResourceUsage::
          kSoftwareCompositedResourceUsage:
        FALLTHROUGH;
      case CanvasResourceProvider::ResourceUsage::
          kSoftwareCompositedDirect2DResourceUsage:
        should_support_compositing = PlatformSupportsGMBs();
        break;
      case CanvasResourceProvider::ResourceUsage::kAcceleratedResourceUsage:
        FALLTHROUGH;
      case CanvasResourceProvider::ResourceUsage::
          kAcceleratedCompositedResourceUsage:
        FALLTHROUGH;
      case CanvasResourceProvider::ResourceUsage::
          kAcceleratedDirect2DResourceUsage:
        FALLTHROUGH;
      case CanvasResourceProvider::ResourceUsage::
          kAcceleratedDirect3DResourceUsage:
        should_support_compositing = true;
        break;
    }

    auto provider = CanvasResourceProvider::Create(
        IntSize(max_texture_size - 1, max_texture_size), usage,
        context_provider_wrapper_, 0 /* msaa_sample_count */,
        kLow_SkFilterQuality, kColorParams,
        CanvasResourceProvider::kAllowImageChromiumPresentationMode,
        nullptr /* resource_dispatcher */, true /* is_origin_top_left */);
    EXPECT_EQ(provider->SupportsDirectCompositing(),
              should_support_compositing);

    provider = CanvasResourceProvider::Create(
        IntSize(max_texture_size, max_texture_size), usage,
        context_provider_wrapper_, 0 /* msaa_sample_count */,
        kLow_SkFilterQuality, kColorParams,
        CanvasResourceProvider::kAllowImageChromiumPresentationMode,
        nullptr /* resource_dispatcher */, true /* is_origin_top_left */);
    EXPECT_EQ(provider->SupportsDirectCompositing(),
              should_support_compositing);

    provider = CanvasResourceProvider::Create(
        IntSize(max_texture_size + 1, max_texture_size), usage,
        context_provider_wrapper_, 0 /* msaa_sample_count */,
        kLow_SkFilterQuality, kColorParams,
        CanvasResourceProvider::kAllowImageChromiumPresentationMode,
        nullptr /* resource_dispatcher */, true /* is_origin_top_left */);
    EXPECT_FALSE(provider->SupportsDirectCompositing());
  }
}

}  // namespace blink
