/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"

#include <list>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"
#include "cc/layers/texture_layer.h"
#include "cc/paint/paint_flags.h"
#include "cc/test/skia_common.h"
#include "cc/test/stub_decode_cache.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

namespace blink {

namespace {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;

class AcceleratedCompositingTestPlatform
    : public blink::TestingPlatformSupport {
 public:
  bool IsGpuCompositingDisabled() const override { return false; }
};

}  // namespace

class Canvas2DLayerBridgeTest : public Test {
 public:
  std::unique_ptr<Canvas2DLayerBridge> MakeBridge(
      const gfx::Size& size,
      RasterModeHint raster_mode,
      OpacityMode opacity_mode,
      std::unique_ptr<FakeCanvasResourceHost> custom_host = nullptr) {
    if (custom_host)
      host_ = std::move(custom_host);
    if (!host_)
      host_ = std::make_unique<FakeCanvasResourceHost>(size);
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        std::make_unique<Canvas2DLayerBridge>(host_.get());
    host_->SetPreferred2DRasterMode(raster_mode);
    host_->AlwaysEnableRasterTimersForTesting();
    host_->SetOpacityMode(opacity_mode);
    host_->GetOrCreateCanvasResourceProvider(raster_mode);
    host_->GetOrCreateCcLayerIfNeeded();
    return bridge;
  }

  void SetUp() override {
    accelerated_compositing_scope_ = std::make_unique<
        ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>();
    test_context_provider_ = viz::TestContextProvider::Create();
    InitializeSharedGpuContextGLES2(test_context_provider_.get(),
                                    &image_decode_cache_);
  }

  virtual bool NeedsMockGL() { return false; }

  void TearDown() override {
    SharedGpuContext::Reset();
    test_context_provider_.reset();
    accelerated_compositing_scope_ = nullptr;
  }

  FakeCanvasResourceHost* Host() {
    DCHECK(host_);
    return host_.get();
  }

  cc::PaintCanvas& Canvas() { return Host()->ResourceProvider()->Canvas(); }

  RasterMode GetRasterMode(Canvas2DLayerBridge* bridge) {
    // TODO(crbug.com/1476964): Remove this when done refactoring.
    // Temporary bootstrap. In non-test code HTMLCanvasElement overrides
    // IsHibernating to propagate the value from Canvas2DLayerBridge, but
    // FakeCanvasResourceHost does not do this. This can be removed once
    // hibernation management is removed from Canvas2DLayerBridge.
    host_->SetIsHibernating(bridge->GetHibernationHandler().IsHibernating());

    return Host()->GetRasterMode();
  }

 protected:
  test::TaskEnvironment task_environment_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  cc::StubDecodeCache image_decode_cache_;
  std::unique_ptr<FakeCanvasResourceHost> host_;
  std::unique_ptr<
      ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>
      accelerated_compositing_scope_;
};

TEST_F(Canvas2DLayerBridgeTest, NoRecreationOfResourceProviderAfterDraw) {
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);
  EXPECT_TRUE(Host()->IsResourceValid());
  cc::PaintFlags flags;
  uint32_t gen_id = bridge->GetOrCreateResourceProvider()->ContentUniqueID();
  Canvas().drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
  EXPECT_EQ(gen_id, bridge->GetOrCreateResourceProvider()->ContentUniqueID());
}

TEST_F(Canvas2DLayerBridgeTest, GetResourceProviderWhenContextIsLost) {
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);
  EXPECT_TRUE(Host()->IsResourceValid());
  cc::PaintFlags flags;
  EXPECT_TRUE(bridge->GetOrCreateResourceProvider());
  test_context_provider_->TestContextGL()->set_context_lost(true);
  EXPECT_EQ(nullptr, bridge->GetOrCreateResourceProvider());
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhenContextIsLost) {
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);

  EXPECT_TRUE(GetRasterMode(bridge.get()) == RasterMode::kGPU);

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  EXPECT_TRUE(Host()->PrepareTransferableResource(nullptr, &resource,
                                                  &release_callback));

  // When the context is lost we are not sure if we should still be producing
  // GL frames for the compositor or not, so fail to generate frames.
  test_context_provider_->TestContextGL()->set_context_lost(true);
  EXPECT_FALSE(Host()->PrepareTransferableResource(nullptr, &resource,
                                                   &release_callback));
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxAndLoseResource) {
  // Prepare a mailbox, then report the resource as lost.
  // This test passes by not crashing and not triggering assertions.
  {
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);
    viz::TransferableResource resource;
    viz::ReleaseCallback release_callback;
    EXPECT_TRUE(Host()->PrepareTransferableResource(nullptr, &resource,
                                                    &release_callback));

    bool lost_resource = true;
    std::move(release_callback).Run(gpu::SyncToken(), lost_resource);
  }

  // Retry with mailbox released while bridge destruction is in progress.
  {
    viz::TransferableResource resource;
    viz::ReleaseCallback release_callback;

    {
      std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
          gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);
      Host()->PrepareTransferableResource(nullptr, &resource,
                                          &release_callback);
      // |bridge| goes out of scope and would normally be destroyed, but
      // object is kept alive by self references.
    }

    // This should cause the bridge to be destroyed.
    bool lost_resource = true;
    // Before fixing crbug.com/411864, the following line would cause a memory
    // use after free that sometimes caused a crash in normal builds and
    // crashed consistently with ASAN.
    std::move(release_callback).Run(gpu::SyncToken(), lost_resource);
  }
}

TEST_F(Canvas2DLayerBridgeTest, ReleaseCallbackWithNullContextProviderWrapper) {
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;

  {
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);
    EXPECT_TRUE(Host()->PrepareTransferableResource(nullptr, &resource,
                                                    &release_callback));
  }

  bool lost_resource = true;
  test_context_provider_->TestContextGL()->set_context_lost(true);
  // Get a new context provider so that the WeakPtr to the old one is null.
  // This is the test to make sure that ReleaseMailboxImageResource() handles
  // null context_provider_wrapper properly.
  SharedGpuContext::ContextProviderWrapper();
  std::move(release_callback).Run(gpu::SyncToken(), lost_resource);
}

TEST_F(Canvas2DLayerBridgeTest, FallbackToSoftwareIfContextLost) {
  test_context_provider_->TestContextGL()->set_context_lost(true);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);
  EXPECT_EQ(GetRasterMode(bridge.get()), RasterMode::kCPU);
  EXPECT_TRUE(Host()->IsResourceValid());
}

void DrawSomething(Canvas2DLayerBridge* bridge) {
  CanvasResourceProvider* provider = bridge->GetOrCreateResourceProvider();
  provider->Canvas().drawLine(0, 0, 2, 2, cc::PaintFlags());
  provider->FlushCanvas(FlushReason::kTesting);
}

TEST_F(Canvas2DLayerBridgeTest, ResourceRecycling) {
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .gpu_memory_buffer_formats.Put(gfx::BufferFormat::BGRA_8888);

  viz::TransferableResource resources[3];
  viz::ReleaseCallback callbacks[3];
  cc::PaintFlags flags;

  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);
  Canvas().drawLine(0, 0, 2, 2, flags);
  DrawSomething(bridge.get());
  ASSERT_TRUE(Host()->PrepareTransferableResource(nullptr, &resources[0],
                                                  &callbacks[0]));

  Canvas().drawLine(0, 0, 2, 2, flags);
  DrawSomething(bridge.get());
  ASSERT_TRUE(Host()->PrepareTransferableResource(nullptr, &resources[1],
                                                  &callbacks[1]));
  EXPECT_NE(resources[0].mailbox(), resources[1].mailbox());

  // Now release the first resource and draw again. It should be reused due to
  // recycling.
  std::move(callbacks[0]).Run(gpu::SyncToken(), false);
  Canvas().drawLine(0, 0, 2, 2, flags);
  DrawSomething(bridge.get());
  ASSERT_TRUE(Host()->PrepareTransferableResource(nullptr, &resources[2],
                                                  &callbacks[2]));
  EXPECT_EQ(resources[0].mailbox(), resources[2].mailbox());

  std::move(callbacks[1]).Run(gpu::SyncToken(), false);
  std::move(callbacks[2]).Run(gpu::SyncToken(), false);
}

TEST_F(Canvas2DLayerBridgeTest, NoResourceRecyclingWhenPageHidden) {
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .gpu_memory_buffer_formats.Put(gfx::BufferFormat::BGRA_8888);

  viz::TransferableResource resources[2];
  viz::ReleaseCallback callbacks[2];
  cc::PaintFlags flags;

  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);
  Canvas().drawLine(0, 0, 2, 2, flags);
  DrawSomething(bridge.get());
  ASSERT_TRUE(Host()->PrepareTransferableResource(nullptr, &resources[0],
                                                  &callbacks[0]));
  Canvas().drawLine(0, 0, 2, 2, flags);
  DrawSomething(bridge.get());
  ASSERT_TRUE(Host()->PrepareTransferableResource(nullptr, &resources[1],
                                                  &callbacks[1]));
  EXPECT_NE(resources[0].mailbox(), resources[1].mailbox());

  // Now release the first resource and mark the page hidden. The recycled
  // resource should be dropped.
  std::move(callbacks[0]).Run(gpu::SyncToken(), false);
  EXPECT_EQ(test_context_provider_->TestContextGL()->NumTextures(), 2u);
  Host()->SetPageVisible(false);

  // TODO(crbug.com/1476964): Remove this when done refactoring.
  bridge->PageVisibilityChanged();

  EXPECT_EQ(test_context_provider_->TestContextGL()->NumTextures(), 1u);

  // Release second frame, this resource is not released because its the current
  // render target for the canvas. It should only be released if the canvas is
  // hibernated.
  std::move(callbacks[1]).Run(gpu::SyncToken(), false);
  EXPECT_EQ(test_context_provider_->TestContextGL()->NumTextures(), 1u);
}

TEST_F(Canvas2DLayerBridgeTest,
       PrepareTransferableResourceTracksCanvasChanges) {
  gfx::Size size(300, 300);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(size, RasterModeHint::kPreferGPU, kNonOpaque);

  Canvas().clear(SkColors::kRed);
  DrawSomething(bridge.get());
  ASSERT_TRUE(!!Host()->CcLayer());

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  EXPECT_TRUE(Host()->PrepareTransferableResource(nullptr, &resource,
                                                  &release_callback));
  Host()->CcLayer()->SetTransferableResource(resource,
                                             std::move(release_callback));

  viz::ReleaseCallback release_callback2;
  EXPECT_FALSE(Host()->PrepareTransferableResource(nullptr, &resource,
                                                   &release_callback2));
  EXPECT_FALSE(release_callback2);
}

TEST_F(Canvas2DLayerBridgeTest, SoftwareCanvasIsCompositedIfImageChromium) {
  ScopedTestingPlatformSupport<GpuMemoryBufferTestPlatform> platform;
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .gpu_memory_buffer_formats.Put(gfx::BufferFormat::BGRA_8888);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferCPU, kNonOpaque);
  EXPECT_TRUE(Host()->IsResourceValid());
  DrawSomething(bridge.get());
  EXPECT_TRUE(Host()->IsComposited());
  EXPECT_EQ(GetRasterMode(bridge.get()), RasterMode::kCPU);
}

TEST_F(Canvas2DLayerBridgeTest, SoftwareCanvasNotCompositedIfNotImageChromium) {
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(false);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferCPU, kNonOpaque);
  EXPECT_TRUE(Host()->IsResourceValid());
  DrawSomething(bridge.get());
  EXPECT_FALSE(Host()->IsComposited());
  EXPECT_EQ(GetRasterMode(bridge.get()), RasterMode::kCPU);
}

TEST_F(Canvas2DLayerBridgeTest, PushPropertiesAfterVisibilityChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({::features::kClearCanvasResourcesInBackground},
                                {features::kCanvas2DHibernation});

  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .gpu_memory_buffer_formats.Put(gfx::BufferFormat::BGRA_8888);

  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(gfx::Size(300, 150), RasterModeHint::kPreferGPU, kNonOpaque);
  cc::PaintFlags flags;
  Canvas().drawLine(0, 0, 2, 2, flags);
  DrawSomething(bridge.get());

  Host()->SetPageVisible(false);
  // TODO(crbug.com/1476964): Remove this when done refactoring.
  bridge->PageVisibilityChanged();
  EXPECT_FALSE(Host()->CcLayer()->needs_set_resource_for_testing());

  Host()->SetPageVisible(true);
  bridge->PageVisibilityChanged();
  EXPECT_TRUE(Host()->CcLayer()->needs_set_resource_for_testing());
}

}  // namespace blink
