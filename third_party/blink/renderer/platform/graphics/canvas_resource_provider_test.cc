// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "build/build_config.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
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
  MOCK_METHOD1(SetFilterQualityInResource, void(cc::PaintFlags::FilterQuality));
};

}  // anonymous namespace

class CanvasResourceProviderTest : public Test {
 public:
  void SetUp() override {
    test_context_provider_ = viz::TestContextProvider::Create();
    auto* test_gl = test_context_provider_->UnboundTestContextGL();
    test_gl->set_max_texture_size(kMaxTextureSize);
    test_gl->set_supports_scanout_shared_images(true);
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
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kMedium,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to RGBA8, or BGRA8 on MacOS
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kBGRA_8888_SkColorType));
#else
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kRGBA_8888_SkColorType));
#endif

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderTexture) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to kRGBA8
  EXPECT_EQ(provider->GetSkImageInfo(),
            kInfo.makeColorType(kRGBA_8888_SkColorType));

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderUnacceleratedOverlay) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kCPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());

  // We do not support single buffering for unaccelerated low latency canvas.
  EXPECT_FALSE(provider->SupportsSingleBuffering());

  EXPECT_EQ(provider->GetSkImageInfo(), kInfo);

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageResourceRecycling) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kMedium,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_FALSE(provider->IsSingleBuffered());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
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
  auto resource = provider->ProduceCanvasResource();
  auto sync_token = resource->GetSyncToken();
  ASSERT_TRUE(resource);
  EXPECT_EQ(resource, provider->ProduceCanvasResource());
  EXPECT_EQ(sync_token, resource->GetSyncToken());

  // Resource updated after draw.
  provider->Canvas()->clear(SkColors::kWhite);
  auto new_resource = provider->ProduceCanvasResource();
  EXPECT_NE(resource, new_resource);
  EXPECT_NE(sync_token, new_resource->GetSyncToken());

  // Resource recycled.
  viz::TransferableResource transferable_resource;
  CanvasResource::ReleaseCallback release_callback;
  ASSERT_TRUE(resource->PrepareTransferableResource(
      &transferable_resource, &release_callback, kUnverifiedSyncToken));
  auto* resource_ptr = resource.get();
  std::move(release_callback).Run(std::move(resource), sync_token, false);

  provider->Canvas()->clear(SkColors::kBlack);
  auto resource_again = provider->ProduceCanvasResource();
  EXPECT_EQ(resource_ptr, resource_again);
  EXPECT_NE(sync_token, resource_again->GetSyncToken());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageStaticBitmapImage) {
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kMedium,
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
  provider->Canvas()->clear(SkColors::kWhite);
  provider->FlushCanvas();
  new_image = provider->Snapshot();
  EXPECT_NE(new_image->GetMailboxHolder().mailbox,
            image->GetMailboxHolder().mailbox);

  // Resource recycled.
  auto original_mailbox = image->GetMailboxHolder().mailbox;
  image.reset();
  provider->Canvas()->clear(SkColors::kBlack);
  provider->FlushCanvas();
  EXPECT_EQ(original_mailbox, provider->Snapshot()->GetMailboxHolder().mailbox);
}

TEST_F(CanvasResourceProviderTest, NoRecycleIfLastRefCallback) {
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kMedium,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  ASSERT_TRUE(provider->IsValid());

  scoped_refptr<StaticBitmapImage> snapshot1 = provider->Snapshot();
  ASSERT_TRUE(snapshot1);

  // Set up a LastUnrefCallback that recycles the resource asynchronously,
  // similarly to what OffscreenCanvasPlaceholder would do.
  provider->ProduceCanvasResource()->SetLastUnrefCallback(
      base::BindOnce([](scoped_refptr<CanvasResource> resource) {}));

  // Resource updated after draw.
  provider->Canvas()->clear(SkColors::kWhite);
  provider->FlushCanvas();
  scoped_refptr<StaticBitmapImage> snapshot2 = provider->Snapshot();
  EXPECT_NE(snapshot2->GetMailboxHolder().mailbox,
            snapshot1->GetMailboxHolder().mailbox);

  auto snapshot1_mailbox = snapshot1->GetMailboxHolder().mailbox;
  snapshot1.reset();  // resource not recycled due to LastUnrefCallback
  provider->Canvas()->clear(SkColors::kBlack);
  provider->FlushCanvas();
  scoped_refptr<StaticBitmapImage> snapshot3 = provider->Snapshot();
  // confirm resource is not recycled.
  EXPECT_NE(snapshot3->GetMailboxHolder().mailbox, snapshot1_mailbox);
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderSharedImageCopyOnWriteDisabled) {
  auto* fake_context = static_cast<FakeWebGraphicsContext3DProvider*>(
      context_provider_wrapper_->ContextProvider());
  auto caps = fake_context->GetCapabilities();
  caps.disable_2d_canvas_copy_on_write = true;
  fake_context->SetCapabilities(caps);

  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kMedium,
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
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  auto provider = CanvasResourceProvider::CreateBitmapProvider(
      kInfo, cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_TRUE(provider->GetSkImageInfo() == kInfo);

  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderSharedBitmap) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  MockCanvasResourceDispatcherClient client;
  CanvasResourceDispatcher resource_dispatcher(
      &client, scheduler::GetSingleThreadTaskRunnerForTesting(),
      scheduler::GetSingleThreadTaskRunnerForTesting(), 1 /* client_id */,
      1 /* sink_id */, 1 /* placeholder_canvas_id */, kSize);

  auto provider = CanvasResourceProvider::CreateSharedBitmapProvider(
      kInfo, cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      resource_dispatcher.GetWeakPtr());

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_FALSE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_FALSE(provider->SupportsSingleBuffering());
  EXPECT_TRUE(provider->GetSkImageInfo() == kInfo);

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_FALSE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect2DGpuMemoryBuffer) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  const uint32_t shared_image_usage_flags =
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT |
      gpu::SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE;

  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kMedium,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      shared_image_usage_flags);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  // As it is an CanvasResourceProviderSharedImage and an accelerated canvas, it
  // will internally force it to RGBA8, or BGRA8 on MacOS
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kBGRA_8888_SkColorType));
#else
  EXPECT_TRUE(provider->GetSkImageInfo() ==
              kInfo.makeColorType(kRGBA_8888_SkColorType));
#endif

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());
}

TEST_F(CanvasResourceProviderTest,
       CanvasResourceProviderDirect3DGpuMemoryBuffer) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  auto provider = CanvasResourceProvider::CreatePassThroughProvider(
      kInfo, cc::PaintFlags::FilterQuality::kLow, context_provider_wrapper_,
      nullptr /*resource_dispatcher */, true /*is_origin_top_left*/);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider->IsValid());
  EXPECT_TRUE(provider->IsAccelerated());
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  EXPECT_TRUE(provider->SupportsSingleBuffering());
  EXPECT_TRUE(provider->GetSkImageInfo() == kInfo);

  EXPECT_FALSE(provider->IsSingleBuffered());
  provider->TryEnableSingleBuffering();
  EXPECT_TRUE(provider->IsSingleBuffered());

  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  scoped_refptr<ExternalCanvasResource> resource =
      ExternalCanvasResource::Create(
          mailbox, viz::ReleaseCallback(), gpu::SyncToken(), kInfo,
          GL_TEXTURE_2D, SharedGpuContext::ContextProviderWrapper(),
          provider->CreateWeakPtr(), cc::PaintFlags::FilterQuality::kMedium,
          true /*is_origin_top_left*/, true /*is_overlay_candidate*/);

  // NewOrRecycledResource() would return nullptr before an ImportResource().
  EXPECT_TRUE(provider->ImportResource(resource));
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
  // NewOrRecycledResource() will always return the same |resource|.
  EXPECT_EQ(provider->NewOrRecycledResource(), resource);
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_Bitmap) {
  auto provider = CanvasResourceProvider::CreateBitmapProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize - 1, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateBitmapProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateBitmapProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize + 1, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear);
  EXPECT_FALSE(provider->SupportsDirectCompositing());
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_SharedImage) {
  auto provider = CanvasResourceProvider::CreateSharedImageProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize - 1, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSharedImageProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSharedImageProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize + 1, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);
  // The CanvasResourceProvider for SharedImage should not be created or valid
  // if the texture size is greater than the maximum value
  EXPECT_TRUE(!provider || !provider->IsValid());
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_SwapChain) {
  auto provider = CanvasResourceProvider::CreateSwapChainProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize - 1, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, nullptr /* resource_dispatcher */,
      true /*is_origin_top_left*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSwapChainProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, nullptr /* resource_dispatcher */,
      true /*is_origin_top_left*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreateSwapChainProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize + 1, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, nullptr /* resource_dispatcher */,
      true /*is_origin_top_left*/);

  // The CanvasResourceProvider for SwapChain should not be created or valid
  // if the texture size is greater than the maximum value
  EXPECT_TRUE(!provider || !provider->IsValid());
}

TEST_F(CanvasResourceProviderTest, DimensionsExceedMaxTextureSize_PassThrough) {
  auto provider = CanvasResourceProvider::CreatePassThroughProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize - 1, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow, context_provider_wrapper_,
      nullptr /* resource_dispatcher */, true /*is_origin_top_left*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreatePassThroughProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow, context_provider_wrapper_,
      nullptr /* resource_dispatcher */, true /*is_origin_top_left*/);
  EXPECT_TRUE(provider->SupportsDirectCompositing());
  provider = CanvasResourceProvider::CreatePassThroughProvider(
      SkImageInfo::MakeN32Premul(kMaxTextureSize + 1, kMaxTextureSize),
      cc::PaintFlags::FilterQuality::kLow, context_provider_wrapper_,
      nullptr /* resource_dispatcher */, true /*is_origin_top_left*/);
  // The CanvasResourceProvider for PassThrough should not be created or valid
  // if the texture size is greater than the maximum value
  EXPECT_TRUE(!provider || !provider->IsValid());
}

TEST_F(CanvasResourceProviderTest, CanvasResourceProviderDirect2DSwapChain) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  auto provider = CanvasResourceProvider::CreateSwapChainProvider(
      kInfo, cc::PaintFlags::FilterQuality::kLow,
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
  EXPECT_EQ(provider->GetSkImageInfo(), kInfo);
}

TEST_F(CanvasResourceProviderTest, FlushForImage) {
  const SkImageInfo kInfo = SkImageInfo::MakeN32Premul(10, 10);

  auto src_provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kMedium,
      CanvasResourceProvider::ShouldInitialize::kCallClear,
      context_provider_wrapper_, RasterMode::kGPU, true /*is_origin_top_left*/,
      0u /*shared_image_usage_flags*/);

  auto dst_provider = CanvasResourceProvider::CreateSharedImageProvider(
      kInfo, cc::PaintFlags::FilterQuality::kMedium,
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

  // Modify the canvas to trigger OnFlushForImage
  src_provider->Canvas()->clear(SkColors::kWhite);
  // So that all the cached draws are executed
  src_provider->ProduceCanvasResource();

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
