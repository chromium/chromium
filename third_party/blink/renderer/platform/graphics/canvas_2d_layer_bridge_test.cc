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

#include <utility>

#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/test/skia_common.h"
#include "cc/test/stub_decode_cache.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/waitable_event.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

#include <memory>

using testing::AnyNumber;
using testing::AtLeast;
using testing::InSequence;
using testing::Pointee;
using testing::Return;
using testing::SetArgPointee;
using testing::Test;
using testing::_;

namespace blink {

namespace {

class MockGLES2InterfaceWithImageSupport : public FakeGLES2Interface {
 public:
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD4(CreateImageCHROMIUM,
               GLuint(ClientBuffer, GLsizei, GLsizei, GLenum));
  MOCK_METHOD1(DestroyImageCHROMIUM, void(GLuint));
  MOCK_METHOD2(GenTextures, void(GLsizei, GLuint*));
  MOCK_METHOD2(DeleteTextures, void(GLsizei, const GLuint*));
  // Fake
  void ProduceTextureDirectCHROMIUM(GLuint texture, GLbyte* mailbox) override {
    mailbox[0] = 1;  // Make non-zero mailbox names
  }
};

class FakePlatformSupport : public TestingPlatformSupport {
 public:
  void RunUntilStop() const { base::RunLoop().Run(); }

  void StopRunning() const { base::RunLoop().Quit(); }

 private:
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override {
    return &test_gpu_memory_buffer_manager_;
  }

  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;
};

class ImageTrackingDecodeCache : public cc::StubDecodeCache {
 public:
  ImageTrackingDecodeCache() = default;
  ~ImageTrackingDecodeCache() override { EXPECT_EQ(num_locked_images_, 0); }

  cc::DecodedDrawImage GetDecodedImageForDraw(
      const cc::DrawImage& image) override {
    EXPECT_FALSE(disallow_cache_use_);

    num_locked_images_++;
    decoded_images_.push_back(image);
    SkBitmap bitmap;
    bitmap.allocPixelsFlags(SkImageInfo::MakeN32Premul(10, 10),
                            SkBitmap::kZeroPixels_AllocFlag);
    sk_sp<SkImage> sk_image = SkImage::MakeFromBitmap(bitmap);
    return cc::DecodedDrawImage(sk_image, SkSize::Make(0, 0),
                                SkSize::Make(1, 1), kLow_SkFilterQuality,
                                !budget_exceeded_);
  }

  void set_budget_exceeded(bool exceeded) { budget_exceeded_ = exceeded; }
  void set_disallow_cache_use(bool disallow) { disallow_cache_use_ = disallow; }

  void DrawWithImageFinished(
      const cc::DrawImage& image,
      const cc::DecodedDrawImage& decoded_image) override {
    EXPECT_FALSE(disallow_cache_use_);
    num_locked_images_--;
  }

  const std::vector<cc::DrawImage>& decoded_images() const {
    return decoded_images_;
  }
  int num_locked_images() const { return num_locked_images_; }

 private:
  std::vector<cc::DrawImage> decoded_images_;
  int num_locked_images_ = 0;
  bool budget_exceeded_ = false;
  bool disallow_cache_use_ = false;
};

class MockCanvasResourceHost : public blink::FakeCanvasResourceHost {
 public:
  MockCanvasResourceHost(const IntSize& size) : FakeCanvasResourceHost(size) {}
  MOCK_CONST_METHOD1(RestoreCanvasMatrixClipStack, void(cc::PaintCanvas*));
};

}  // anonymous namespace

class Canvas2DLayerBridgeTest : public Test {
 public:
  std::unique_ptr<Canvas2DLayerBridge> MakeBridge(
      const IntSize& size,
      Canvas2DLayerBridge::AccelerationMode acceleration_mode,
      const CanvasColorParams& color_params) {
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        std::make_unique<Canvas2DLayerBridge>(size, acceleration_mode,
                                              color_params);
    bridge->DontUseIdleSchedulingForTesting();
    if (!host_)
      host_ = std::make_unique<MockCanvasResourceHost>(size);
    bridge->SetCanvasResourceHost(host_.get());
    return bridge;
  }

  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl, ImageTrackingDecodeCache* cache,
                      bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl, cache);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(WTF::BindRepeating(
        factory, WTF::Unretained(&gl_), WTF::Unretained(&image_decode_cache_)));
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }

  MockCanvasResourceHost* Host() {
    DCHECK(host_);
    return host_.get();
  }

 protected:
  MockGLES2InterfaceWithImageSupport gl_;
  ImageTrackingDecodeCache image_decode_cache_;
  std::unique_ptr<MockCanvasResourceHost> host_;
};

TEST_F(Canvas2DLayerBridgeTest, DisableAcceleration) {
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 150), Canvas2DLayerBridge::kDisableAcceleration,
                 CanvasColorParams());

  GrBackendTexture backend_texture =
      bridge->NewImageSnapshot(kPreferAcceleration)
          ->PaintImageForCurrentFrame()
          .GetSkImage()
          ->getBackendTexture(true);

  EXPECT_FALSE(backend_texture.isValid());
}

TEST_F(Canvas2DLayerBridgeTest, NoDrawOnContextLost) {
  std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
      IntSize(300, 150), Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams());
  EXPECT_TRUE(bridge->IsValid());
  PaintFlags flags;
  uint32_t gen_id = bridge->GetOrCreateResourceProvider()->ContentUniqueID();
  bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
  EXPECT_EQ(gen_id, bridge->GetOrCreateResourceProvider()->ContentUniqueID());
  gl_.SetIsContextLost(true);
  EXPECT_EQ(nullptr, bridge->GetOrCreateResourceProvider());
  // The following passes by not crashing
  bridge->NewImageSnapshot(kPreferAcceleration);
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhenContextIsLost) {
  std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
      IntSize(300, 150), Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams());

  EXPECT_TRUE(bridge->IsAccelerated());
  bridge->FinalizeFrame();  // Trigger the creation of a backing store
  // When the context is lost we are not sure if we should still be producing
  // GL frames for the compositor or not, so fail to generate frames.
  gl_.SetIsContextLost(true);

  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(bridge->PrepareTransferableResource(nullptr, &resource,
                                                   &release_callback));
}

TEST_F(Canvas2DLayerBridgeTest,
       PrepareMailboxWhenContextIsLostWithFailedRestore) {
  std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
      IntSize(300, 150), Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams());

  bridge->GetOrCreateResourceProvider();
  EXPECT_TRUE(bridge->IsValid());
  // When the context is lost we are not sure if we should still be producing
  // GL frames for the compositor or not, so fail to generate frames.
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(bridge->IsValid());

  // Restoration will fail because
  // Platform::createSharedOffscreenGraphicsContext3DProvider() is stubbed
  // in unit tests.  This simulates what would happen when attempting to
  // restore while the GPU process is down.
  bridge->Restore();

  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(bridge->PrepareTransferableResource(nullptr, &resource,
                                                   &release_callback));
}

TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxAndLoseResource) {
  // Prepare a mailbox, then report the resource as lost.
  // This test passes by not crashing and not triggering assertions.
  {
    std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
        IntSize(300, 150), Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams());
    bridge->FinalizeFrame();
    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    EXPECT_TRUE(bridge->PrepareTransferableResource(nullptr, &resource,
                                                    &release_callback));

    bool lost_resource = true;
    release_callback->Run(gpu::SyncToken(), lost_resource);
  }

  // Retry with mailbox released while bridge destruction is in progress.
  {
    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;

    {
      std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
          IntSize(300, 150), Canvas2DLayerBridge::kForceAccelerationForTesting,
          CanvasColorParams());
      bridge->FinalizeFrame();
      bridge->PrepareTransferableResource(nullptr, &resource,
                                          &release_callback);
      // |bridge| goes out of scope and would normally be destroyed, but
      // object is kept alive by self references.
    }

    // This should cause the bridge to be destroyed.
    bool lost_resource = true;
    // Before fixing crbug.com/411864, the following line would cause a memory
    // use after free that sometimes caused a crash in normal builds and
    // crashed consistently with ASAN.
    release_callback->Run(gpu::SyncToken(), lost_resource);
  }
}

TEST_F(Canvas2DLayerBridgeTest, ReleaseCallbackWithNullContextProviderWrapper) {
  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;

  {
    std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
        IntSize(300, 150), Canvas2DLayerBridge::kForceAccelerationForTesting,
        CanvasColorParams());
    bridge->FinalizeFrame();
    EXPECT_TRUE(bridge->PrepareTransferableResource(nullptr, &resource,
                                                    &release_callback));
  }

  bool lost_resource = true;
  gl_.SetIsContextLost(true);
  // Get a new context provider so that the WeakPtr to the old one is null.
  // This is the test to make sure that ReleaseMailboxImageResource() handles
  // null context_provider_wrapper properly.
  SharedGpuContext::ContextProviderWrapper();
  release_callback->Run(gpu::SyncToken(), lost_resource);
}

TEST_F(Canvas2DLayerBridgeTest, AccelerationHint) {
  {
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                   CanvasColorParams());
    PaintFlags flags;
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    scoped_refptr<StaticBitmapImage> image =
        bridge->NewImageSnapshot(kPreferAcceleration);
    EXPECT_TRUE(bridge->IsValid());
    EXPECT_TRUE(bridge->IsAccelerated());
  }

  {
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                   CanvasColorParams());
    PaintFlags flags;
    bridge->Canvas()->drawRect(SkRect::MakeXYWH(0, 0, 1, 1), flags);
    scoped_refptr<StaticBitmapImage> image =
        bridge->NewImageSnapshot(kPreferNoAcceleration);
    EXPECT_TRUE(bridge->IsValid());
    EXPECT_FALSE(bridge->IsAccelerated());
  }
}

TEST_F(Canvas2DLayerBridgeTest, FallbackToSoftwareIfContextLost) {
  gl_.SetIsContextLost(true);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 150), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  EXPECT_TRUE(bridge->IsValid());
  EXPECT_FALSE(bridge->IsAccelerated());
}

TEST_F(Canvas2DLayerBridgeTest, FallbackToSoftwareOnFailedTextureAlloc) {
  {
    // No fallback case.
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        MakeBridge(IntSize(300, 150), Canvas2DLayerBridge::kEnableAcceleration,
                   CanvasColorParams());
    EXPECT_TRUE(bridge->IsValid());
    EXPECT_TRUE(bridge->IsAccelerated());
    scoped_refptr<StaticBitmapImage> snapshot =
        bridge->NewImageSnapshot(kPreferAcceleration);
    EXPECT_TRUE(bridge->IsAccelerated());
    EXPECT_TRUE(snapshot->IsTextureBacked());
  }

  {
    // Fallback case.
    GrContext* gr = SharedGpuContext::ContextProviderWrapper()
                        ->ContextProvider()
                        ->GetGrContext();
    std::unique_ptr<Canvas2DLayerBridge> bridge =
        MakeBridge(IntSize(300, 150), Canvas2DLayerBridge::kEnableAcceleration,
                   CanvasColorParams());
    EXPECT_TRUE(bridge->IsValid());
    EXPECT_TRUE(bridge->IsAccelerated());  // We don't yet know that
                                           // allocation will fail.
    // This will cause SkSurface_Gpu creation to fail without
    // Canvas2DLayerBridge otherwise detecting that anything was disabled.
    gr->abandonContext();
    scoped_refptr<StaticBitmapImage> snapshot =
        bridge->NewImageSnapshot(kPreferAcceleration);
    EXPECT_FALSE(bridge->IsAccelerated());
    EXPECT_FALSE(snapshot->IsTextureBacked());
  }
}

class MockLogger : public Canvas2DLayerBridge::Logger {
 public:
  MOCK_METHOD1(ReportHibernationEvent,
               void(Canvas2DLayerBridge::HibernationEvent));
  MOCK_METHOD0(DidStartHibernating, void());
  ~MockLogger() override = default;
};

void DrawSomething(Canvas2DLayerBridge* bridge) {
  bridge->DidDraw(FloatRect(0, 0, 1, 1));
  bridge->FinalizeFrame();
  // Grabbing an image forces a flush
  bridge->NewImageSnapshot(kPreferAcceleration);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationLifeCycle)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationLifeCycle)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());
  EXPECT_TRUE(bridge->IsAccelerated());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);

  bridge->SetIsHidden(true);
  platform->RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));

  bridge->SetIsHidden(false);

  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationReEntry)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationReEntry)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  // Toggle visibility before the task that enters hibernation gets a
  // chance to run.
  bridge->SetIsHidden(false);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));

  bridge->SetIsHidden(false);

  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest,
       HibernationLifeCycleWithDeferredRenderingDisabled)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_HibernationLifeCycleWithDeferredRenderingDisabled)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;

  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  EXPECT_CALL(*Host(), RestoreCanvasMatrixClipStack(_)).Times(AnyNumber());

  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());
  bridge->DisableDeferral(kDisableDeferralReasonUnknown);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  testing::Mock::VerifyAndClearExpectations(host_.get());
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Test exiting hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally));
  EXPECT_CALL(*Host(), RestoreCanvasMatrixClipStack(_))
      .Times(AtLeast(1));  // Because deferred rendering is disabled
  bridge->SetIsHidden(false);
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  testing::Mock::VerifyAndClearExpectations(Host());
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, BackgroundRenderingWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_BackgroundRenderingWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  DrawSomething(bridge.get());
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Unhide
  bridge->SetIsHidden(false);
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest,
       BackgroundRenderingWhileHibernatingWithDeferredRenderingDisabled)
#else
TEST_F(
    Canvas2DLayerBridgeTest,
    DISABLED_BackgroundRenderingWhileHibernatingWithDeferredRenderingDisabled)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());
  EXPECT_CALL(*Host(), RestoreCanvasMatrixClipStack(_)).Times(AnyNumber());
  bridge->DisableDeferral(kDisableDeferralReasonUnknown);

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  testing::Mock::VerifyAndClearExpectations(Host());
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  EXPECT_CALL(*Host(), RestoreCanvasMatrixClipStack(_)).Times(AtLeast(1));
  DrawSomething(bridge.get());
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  testing::Mock::VerifyAndClearExpectations(Host());
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Unhide
  EXPECT_CALL(*Host(), RestoreCanvasMatrixClipStack(_)).Times(AtLeast(1));
  bridge->SetIsHidden(false);
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  testing::Mock::VerifyAndClearExpectations(Host());
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, DisableDeferredRenderingWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_DisableDeferredRenderingWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());

  EXPECT_CALL(*Host(), RestoreCanvasMatrixClipStack(_)).Times(AnyNumber());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  testing::Mock::VerifyAndClearExpectations(Host());
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Disable deferral while background rendering
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  EXPECT_CALL(*Host(), RestoreCanvasMatrixClipStack(_)).Times(AtLeast(1));
  bridge->DisableDeferral(kDisableDeferralReasonUnknown);
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  testing::Mock::VerifyAndClearExpectations(Host());
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Unhide
  EXPECT_CALL(*Host(), RestoreCanvasMatrixClipStack(_)).Times(AtLeast(1));
  bridge->SetIsHidden(false);
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  testing::Mock::VerifyAndClearExpectations(Host());
  EXPECT_TRUE(
      bridge->IsAccelerated());  // Becoming visible causes switch back to GPU
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  EXPECT_CALL(*Host(), RestoreCanvasMatrixClipStack(_)).Times(AnyNumber());
  bridge.reset();
  testing::Mock::VerifyAndClearExpectations(Host());
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_TeardownWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Tear down the bridge while hibernating
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::kHibernationEndedWithTeardown));
  bridge.reset();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, SnapshotWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_SnapshotWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Take a snapshot and verify that it is not accelerated due to hibernation
  scoped_refptr<StaticBitmapImage> image =
      bridge->NewImageSnapshot(kPreferAcceleration);
  EXPECT_FALSE(image->IsTextureBacked());
  image = nullptr;

  // Verify that taking a snapshot did not affect the state of bridge
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_TRUE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // End hibernation normally
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationEndedNormally))
      .Times(1);
  bridge->SetIsHidden(false);
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, TeardownWhileHibernationIsPending)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_TeardownWhileHibernationIsPending)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  bridge->SetIsHidden(true);
  bridge.reset();
  // In production, we would expect a
  // HibernationAbortedDueToDestructionWhileHibernatePending event to be
  // fired, but that signal is lost in the unit test due to no longer having
  // a bridge to hold the mockLogger.
  platform->RunUntilIdle();
  // This test passes by not crashing, which proves that the WeakPtr logic
  // is sound.
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToVisibilityChange)
#else
TEST_F(Canvas2DLayerBridgeTest,
       DISABLED_HibernationAbortedDueToVisibilityChange)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(
          Canvas2DLayerBridge::kHibernationAbortedDueToVisibilityChange))
      .Times(1);
  bridge->SetIsHidden(true);
  bridge->SetIsHidden(false);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, HibernationAbortedDueToLostContext)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_HibernationAbortedDueToLostContext)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  gl_.SetIsContextLost(true);

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::kHibernationAbortedDueGpuContextLoss))
      .Times(1);

  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsHibernating());
}

#if CANVAS2D_HIBERNATION_ENABLED
TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileHibernating)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_PrepareMailboxWhileHibernating)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  bridge->DontUseIdleSchedulingForTesting();
  DrawSomething(bridge.get());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);

  // Test PrepareTransferableResource() while hibernating
  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(bridge->PrepareTransferableResource(nullptr, &resource,
                                                   &release_callback));
  EXPECT_TRUE(bridge->IsValid());

  // Tear down the bridge on the thread so that 'bridge' can go out of scope
  // without crashing due to thread checks
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::kHibernationEndedWithTeardown));
  bridge.reset();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
}

#if CANVAS2D_HIBERNATION_ENABLED && CANVAS2D_BACKGROUND_RENDER_SWITCH_TO_CPU
TEST_F(Canvas2DLayerBridgeTest, PrepareMailboxWhileBackgroundRendering)
#else
TEST_F(Canvas2DLayerBridgeTest, DISABLED_PrepareMailboxWhileBackgroundRendering)
#endif
{
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 CanvasColorParams());
  DrawSomething(bridge.get());

  // Register an alternate Logger for tracking hibernation events
  std::unique_ptr<MockLogger> mock_logger = std::make_unique<MockLogger>();
  MockLogger* mock_logger_ptr = mock_logger.get();
  bridge->SetLoggerForTesting(std::move(mock_logger));

  // Test entering hibernation
  std::unique_ptr<WaitableEvent> hibernation_started_event =
      std::make_unique<WaitableEvent>();
  EXPECT_CALL(
      *mock_logger_ptr,
      ReportHibernationEvent(Canvas2DLayerBridge::kHibernationScheduled));
  EXPECT_CALL(*mock_logger_ptr, DidStartHibernating()).Times(1);
  bridge->SetIsHidden(true);
  platform->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);

  // Rendering in the background -> temp switch to SW
  EXPECT_CALL(*mock_logger_ptr,
              ReportHibernationEvent(
                  Canvas2DLayerBridge::
                      kHibernationEndedWithSwitchToBackgroundRendering));
  DrawSomething(bridge.get());
  testing::Mock::VerifyAndClearExpectations(mock_logger_ptr);
  EXPECT_FALSE(bridge->IsAccelerated());
  EXPECT_FALSE(bridge->IsHibernating());
  EXPECT_TRUE(bridge->IsValid());

  // Test prepareMailbox while background rendering
  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  EXPECT_FALSE(bridge->PrepareTransferableResource(nullptr, &resource,
                                                   &release_callback));
  EXPECT_TRUE(bridge->IsValid());
}

TEST_F(Canvas2DLayerBridgeTest, GpuMemoryBufferRecycling) {
  InSequence s;
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .texture_format_bgra8888 = true;

  viz::TransferableResource resource1;
  viz::TransferableResource resource2;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback1;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback2;
  constexpr GLuint texture_id1 = 1;
  constexpr GLuint texture_id2 = 2;
  constexpr GLuint image_id1 = 3;
  constexpr GLuint image_id2 = 4;
  const GLuint texture_target = gpu::GetPlatformSpecificTextureTarget();

  std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
      IntSize(300, 150), Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams());

  testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id1));
  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id1));
  if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    constexpr GLuint image_2d_id_for_copy = 17;
    EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _))
        .WillOnce(Return(image_2d_id_for_copy));
    EXPECT_CALL(gl_, GenTextures(1, _));
    EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_2d_id_for_copy));
  }
  DrawSomething(bridge.get());
  bridge->PrepareTransferableResource(nullptr, &resource1, &release_callback1);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id2));
  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id2));
  if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    constexpr GLuint image_2d_id_for_copy = 19;
    EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _))
        .WillOnce(Return(image_2d_id_for_copy));
    EXPECT_CALL(gl_, GenTextures(1, _));
    EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_2d_id_for_copy));
  }
  DrawSomething(bridge.get());
  bridge->PrepareTransferableResource(nullptr, &resource2, &release_callback2);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // Check that release resources does not result in destruction due
  // to recycling.
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(_)).Times(0);
  EXPECT_CALL(gl_, DeleteTextures(_, _)).Times(0);
  bool lost_resource = false;
  release_callback1->Run(gpu::SyncToken(), lost_resource);
  release_callback1 = nullptr;

  testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, DestroyImageCHROMIUM(_)).Times(0);
  EXPECT_CALL(gl_, DeleteTextures(_, _)).Times(0);
  release_callback2->Run(gpu::SyncToken(), lost_resource);
  release_callback2 = nullptr;

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // Destroying the bridge results in destruction of cached resources.
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id1)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id1))).Times(1);
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id2)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id2))).Times(1);
  bridge.reset();

  testing::Mock::VerifyAndClearExpectations(&gl_);
}

TEST_F(Canvas2DLayerBridgeTest, NoGpuMemoryBufferRecyclingWhenPageHidden) {
  InSequence s;
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .texture_format_bgra8888 = true;

  viz::TransferableResource resource1;
  viz::TransferableResource resource2;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback1;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback2;
  constexpr GLuint texture_id1 = 1;
  constexpr GLuint texture_id2 = 2;
  constexpr GLuint image_id1 = 3;
  constexpr GLuint image_id2 = 4;
  const GLuint texture_target = gpu::GetPlatformSpecificTextureTarget();

  std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
      IntSize(300, 150), Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams());

  testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id1));
  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id1));
  if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    constexpr GLuint image_2d_id_for_copy = 17;
    EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _))
        .WillOnce(Return(image_2d_id_for_copy));
    EXPECT_CALL(gl_, GenTextures(1, _));
    EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_2d_id_for_copy));
  }
  DrawSomething(bridge.get());
  bridge->PrepareTransferableResource(nullptr, &resource1, &release_callback1);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id2));
  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id2));
  if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    constexpr GLuint image_2d_id_for_copy = 19;
    EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _))
        .WillOnce(Return(image_2d_id_for_copy));
    EXPECT_CALL(gl_, GenTextures(1, _));
    EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_2d_id_for_copy));
  }
  DrawSomething(bridge.get());
  bridge->PrepareTransferableResource(nullptr, &resource2, &release_callback2);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // Release first frame to cache
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(_)).Times(0);
  EXPECT_CALL(gl_, DeleteTextures(_, _)).Times(0);
  bool lost_resource = false;
  release_callback1->Run(gpu::SyncToken(), lost_resource);
  release_callback1 = nullptr;

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // Switching to Hidden frees cached resources immediately
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id1)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id1))).Times(1);
  bridge->SetIsHidden(true);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // Release second frame and verify that its resource is destroyed immediately
  // due to the layer bridge being hidden
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id2)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id2))).Times(1);
  release_callback2->Run(gpu::SyncToken(), lost_resource);
  release_callback2 = nullptr;

  testing::Mock::VerifyAndClearExpectations(&gl_);
}

TEST_F(Canvas2DLayerBridgeTest, ReleaseGpuMemoryBufferAfterBridgeDestroyed) {
  InSequence s;
  ScopedCanvas2dImageChromiumForTest canvas_2d_image_chromium(true);
  ScopedTestingPlatformSupport<FakePlatformSupport> platform;
  const_cast<gpu::Capabilities&>(SharedGpuContext::ContextProviderWrapper()
                                     ->ContextProvider()
                                     ->GetCapabilities())
      .texture_format_bgra8888 = true;

  viz::TransferableResource resource;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;
  constexpr GLuint texture_id = 1;
  constexpr GLuint image_id = 2;
  const GLuint texture_target = gpu::GetPlatformSpecificTextureTarget();

  std::unique_ptr<Canvas2DLayerBridge> bridge = MakeBridge(
      IntSize(300, 150), Canvas2DLayerBridge::kForceAccelerationForTesting,
      CanvasColorParams());

  testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _)).WillOnce(Return(image_id));
  EXPECT_CALL(gl_, GenTextures(1, _)).WillOnce(SetArgPointee<1>(texture_id));

  if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    constexpr GLuint image_2d_id_for_copy = 17;
    EXPECT_CALL(gl_, CreateImageCHROMIUM(_, _, _, _))
        .WillOnce(Return(image_2d_id_for_copy));
    EXPECT_CALL(gl_, GenTextures(1, _));
    EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_2d_id_for_copy));
  }
  DrawSomething(bridge.get());
  bridge->PrepareTransferableResource(nullptr, &resource, &release_callback);

  testing::Mock::VerifyAndClearExpectations(&gl_);

  // Tearing down the bridge does not destroy unreleased resources.
  EXPECT_CALL(gl_, DestroyImageCHROMIUM(_)).Times(0);
  EXPECT_CALL(gl_, DeleteTextures(_, _)).Times(0);
  bridge.reset();

  testing::Mock::VerifyAndClearExpectations(&gl_);

  EXPECT_CALL(gl_, DestroyImageCHROMIUM(image_id)).Times(1);
  EXPECT_CALL(gl_, DeleteTextures(1, Pointee(texture_id))).Times(1);
  bool lost_resource = false;
  release_callback->Run(gpu::SyncToken(), lost_resource);
  release_callback = nullptr;

  testing::Mock::VerifyAndClearExpectations(&gl_);
}

TEST_F(Canvas2DLayerBridgeTest, EnsureCCImageCacheUse) {
  auto color_params =
      CanvasColorParams(kSRGBCanvasColorSpace, kF16CanvasPixelFormat, kOpaque);
  ASSERT_FALSE(color_params.NeedsSkColorSpaceXformCanvas());

  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 color_params);
  gfx::ColorSpace expected_color_space = gfx::ColorSpace::CreateSRGB();
  std::vector<cc::DrawImage> images = {
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(10, 10)),
                    SkIRect::MakeWH(10, 10), kNone_SkFilterQuality,
                    SkMatrix::I(), 0u, expected_color_space),
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(20, 20)),
                    SkIRect::MakeWH(5, 5), kNone_SkFilterQuality, SkMatrix::I(),
                    0u, expected_color_space)};

  bridge->Canvas()->drawImage(images[0].paint_image(), 0u, 0u, nullptr);
  bridge->Canvas()->drawImageRect(
      images[1].paint_image(), SkRect::MakeWH(5u, 5u), SkRect::MakeWH(5u, 5u),
      nullptr, cc::PaintCanvas::kFast_SrcRectConstraint);
  bridge->NewImageSnapshot(kPreferAcceleration);

  EXPECT_EQ(image_decode_cache_.decoded_images(), images);
}

TEST_F(Canvas2DLayerBridgeTest, EnsureCCImageCacheUseWithColorConversion) {
  auto color_params = CanvasColorParams(kSRGBCanvasColorSpace,
                                        kRGBA8CanvasPixelFormat, kOpaque);
  ASSERT_TRUE(color_params.NeedsSkColorSpaceXformCanvas());

  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 color_params);
  std::vector<cc::DrawImage> images = {
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(10, 10)),
                    SkIRect::MakeWH(10, 10), kNone_SkFilterQuality,
                    SkMatrix::I(), 0u, color_params.GetStorageGfxColorSpace()),
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(20, 20)),
                    SkIRect::MakeWH(5, 5), kNone_SkFilterQuality, SkMatrix::I(),
                    0u, color_params.GetStorageGfxColorSpace())};

  bridge->Canvas()->drawImage(images[0].paint_image(), 0u, 0u, nullptr);
  bridge->Canvas()->drawImageRect(
      images[1].paint_image(), SkRect::MakeWH(5u, 5u), SkRect::MakeWH(5u, 5u),
      nullptr, cc::PaintCanvas::kFast_SrcRectConstraint);
  bridge->NewImageSnapshot(kPreferAcceleration);

  EXPECT_EQ(image_decode_cache_.decoded_images(), images);
}

TEST_F(Canvas2DLayerBridgeTest, ImagesLockedUntilCacheLimit) {
  // Disable deferral so we can inspect the cache state as we use the canvas.
  auto color_params =
      CanvasColorParams(kSRGBCanvasColorSpace, kF16CanvasPixelFormat, kOpaque);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 color_params);
  bridge->DisableDeferral(DisableDeferralReason::kDisableDeferralReasonUnknown);

  std::vector<cc::DrawImage> images = {
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(10, 10)),
                    SkIRect::MakeWH(10, 10), kNone_SkFilterQuality,
                    SkMatrix::I(), 0u, color_params.GetStorageGfxColorSpace()),
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(20, 20)),
                    SkIRect::MakeWH(5, 5), kNone_SkFilterQuality, SkMatrix::I(),
                    0u, color_params.GetStorageGfxColorSpace()),
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(20, 20)),
                    SkIRect::MakeWH(5, 5), kNone_SkFilterQuality, SkMatrix::I(),
                    0u, color_params.GetStorageGfxColorSpace())};

  // First 2 images are budgeted, they should remain locked after the op.
  bridge->Canvas()->drawImage(images[0].paint_image(), 0u, 0u, nullptr);
  bridge->Canvas()->drawImage(images[1].paint_image(), 0u, 0u, nullptr);
  EXPECT_EQ(image_decode_cache_.num_locked_images(), 2);

  // Next image is not budgeted, we should unlock all images other than the last
  // image.
  image_decode_cache_.set_budget_exceeded(true);
  bridge->Canvas()->drawImage(images[2].paint_image(), 0u, 0u, nullptr);
  EXPECT_EQ(image_decode_cache_.num_locked_images(), 1);

  // Ask the provider to release everything, no locked images should remain.
  bridge->GetOrCreateResourceProvider()->ReleaseLockedImages();
  EXPECT_EQ(image_decode_cache_.num_locked_images(), 0);
}

TEST_F(Canvas2DLayerBridgeTest, QueuesCleanupTaskForLockedImages) {
  // Disable deferral so we can inspect the cache state as we use the canvas.
  auto color_params =
      CanvasColorParams(kSRGBCanvasColorSpace, kF16CanvasPixelFormat, kOpaque);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 color_params);
  bridge->DisableDeferral(DisableDeferralReason::kDisableDeferralReasonUnknown);

  auto image =
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(10, 10)),
                    SkIRect::MakeWH(10, 10), kNone_SkFilterQuality,
                    SkMatrix::I(), 0u, color_params.GetStorageGfxColorSpace());
  bridge->Canvas()->drawImage(image.paint_image(), 0u, 0u, nullptr);
  EXPECT_EQ(image_decode_cache_.num_locked_images(), 1);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(image_decode_cache_.num_locked_images(), 0);
}

TEST_F(Canvas2DLayerBridgeTest, ImageCacheOnContextLost) {
  // Disable deferral so we use the raster canvas directly.
  auto color_params =
      CanvasColorParams(kSRGBCanvasColorSpace, kF16CanvasPixelFormat, kOpaque);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      MakeBridge(IntSize(300, 300), Canvas2DLayerBridge::kEnableAcceleration,
                 color_params);
  bridge->DisableDeferral(DisableDeferralReason::kDisableDeferralReasonUnknown);

  PaintFlags flags;
  std::vector<cc::DrawImage> images = {
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(10, 10)),
                    SkIRect::MakeWH(10, 10), kNone_SkFilterQuality,
                    SkMatrix::I(), 0u, color_params.GetStorageGfxColorSpace()),
      cc::DrawImage(cc::CreateDiscardablePaintImage(gfx::Size(20, 20)),
                    SkIRect::MakeWH(5, 5), kNone_SkFilterQuality, SkMatrix::I(),
                    0u, color_params.GetStorageGfxColorSpace())};
  bridge->Canvas()->drawImage(images[0].paint_image(), 0u, 0u, nullptr);

  // Lose the context and ensure that the image provider is not used.
  bridge->ResourceProvider()->OnContextDestroyed();
  // We should unref all images on the cache when the context is destroyed.
  EXPECT_EQ(image_decode_cache_.num_locked_images(), 0);
  image_decode_cache_.set_disallow_cache_use(true);
  bridge->Canvas()->drawImage(images[1].paint_image(), 0u, 0u, &flags);
}

}  // namespace blink
