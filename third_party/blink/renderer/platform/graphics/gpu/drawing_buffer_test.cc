/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/gl/gpu_preference.h"
#include "v8/include/v8.h"

using testing::Test;
using testing::_;

namespace blink {

MATCHER_P(SyncTokenEq, token, "") {
  return *reinterpret_cast<const gpu::SyncToken*>(arg) == token;
}

class DrawingBufferTest : public Test {
 protected:
  void SetUp() override { Init(kDisableMultisampling); }

  void Init(UseMultisampling use_multisampling) {
    gfx::Size initial_size(kInitialWidth, kInitialHeight);
    auto gl = std::make_unique<GLES2InterfaceForTests>();
    auto provider =
        std::make_unique<WebGraphicsContext3DProviderForTests>(std::move(gl));
    GLES2InterfaceForTests* gl_ =
        static_cast<GLES2InterfaceForTests*>(provider->ContextGL());
    Platform::GraphicsInfo graphics_info;
    graphics_info.using_gpu_compositing = true;
    drawing_buffer_ = DrawingBufferForTests::Create(
        std::move(provider), /*sii_provider_for_bitmap=*/nullptr, graphics_info,
        gl_, initial_size, DrawingBuffer::kPreserve, use_multisampling);
    CHECK(drawing_buffer_);
    SetAndSaveRestoreState(false);
  }

  // Initialize GL state with unusual values, to verify that they are restored.
  // The |invert| parameter will reverse all boolean parameters, so that all
  // values are tested.
  void SetAndSaveRestoreState(bool invert) {
    GLES2InterfaceForTests* gl_ = drawing_buffer_->ContextGLForTests();
    GLboolean scissor_enabled = !invert;
    GLfloat clear_color[4] = {0.1, 0.2, 0.3, 0.4};
    GLfloat clear_depth = 0.8;
    GLint clear_stencil = 37;
    GLboolean color_mask[4] = {invert, !invert, !invert, invert};
    GLboolean depth_mask = invert;
    GLboolean stencil_mask = invert;
    GLint pack_alignment = 7;
    GLuint active_texture2d_binding = 0xbeef1;
    GLuint renderbuffer_binding = 0xbeef2;
    GLuint draw_framebuffer_binding = 0xbeef3;
    GLuint read_framebuffer_binding = 0xbeef4;
    GLuint pixel_unpack_buffer_binding = 0xbeef5;

    if (scissor_enabled)
      gl_->Enable(GL_SCISSOR_TEST);
    else
      gl_->Disable(GL_SCISSOR_TEST);

    gl_->ClearColor(clear_color[0], clear_color[1], clear_color[2],
                    clear_color[3]);
    gl_->ClearDepthf(clear_depth);
    gl_->ClearStencil(clear_stencil);
    gl_->ColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
    gl_->DepthMask(depth_mask);
    gl_->StencilMask(stencil_mask);
    gl_->PixelStorei(GL_PACK_ALIGNMENT, pack_alignment);
    gl_->BindTexture(GL_TEXTURE_2D, active_texture2d_binding);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, renderbuffer_binding);
    gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer_binding);
    gl_->BindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer_binding);
    gl_->BindBuffer(GL_PIXEL_UNPACK_BUFFER, pixel_unpack_buffer_binding);

    gl_->SaveState();
  }

  void VerifyStateWasRestored() {
    GLES2InterfaceForTests* gl_ = drawing_buffer_->ContextGLForTests();
    gl_->VerifyStateHasNotChangedSinceSave();
  }

  scoped_refptr<DrawingBufferForTests> drawing_buffer_;
};

class DrawingBufferTestMultisample : public DrawingBufferTest {
 protected:
  void SetUp() override { Init(kEnableMultisampling); }
};

TEST_F(DrawingBufferTestMultisample, verifyMultisampleResolve) {
  // Initial state: already marked changed, multisampled
  EXPECT_FALSE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->ExplicitResolveOfMultisampleData());

  // Resolve the multisample buffer
  EXPECT_TRUE(drawing_buffer_->ResolveAndBindForReadAndDraw());

  // After resolve, acknowledge new content
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  // No new content
  EXPECT_FALSE(drawing_buffer_->MarkContentsChanged());

  drawing_buffer_->BeginDestruction();
}

TEST_F(DrawingBufferTest, VerifyResizingProperlyAffectsResources) {
  gpu::TestSharedImageInterface* sii =
      drawing_buffer_->SharedImageInterfaceForTests();

  VerifyStateWasRestored();
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;

  gfx::Size initial_size(kInitialWidth, kInitialHeight);
  gfx::Size alternate_size(kInitialWidth, kAlternateHeight);

  // Produce one resource at size 100x100.
  EXPECT_FALSE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  VerifyStateWasRestored();
  EXPECT_EQ(initial_size, sii->MostRecentSize());

  // Resize to 100x50.
  drawing_buffer_->Resize(alternate_size);
  VerifyStateWasRestored();
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
  VerifyStateWasRestored();

  // Produce a resource at this size.
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  EXPECT_EQ(alternate_size, sii->MostRecentSize());
  VerifyStateWasRestored();

  // Reset to initial size.
  drawing_buffer_->Resize(initial_size);
  VerifyStateWasRestored();
  SetAndSaveRestoreState(true);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
  VerifyStateWasRestored();

  // Prepare another resource and verify that it's the correct size.
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  EXPECT_EQ(initial_size, sii->MostRecentSize());
  VerifyStateWasRestored();

  // Prepare one final resource and verify that it's the correct size.
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  VerifyStateWasRestored();
  EXPECT_EQ(initial_size, sii->MostRecentSize());
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
  drawing_buffer_->BeginDestruction();
}

TEST_F(DrawingBufferTest, VerifySharedImagesReleasedAfterReleaseCallback) {
  auto* sii = drawing_buffer_->SharedImageInterfaceForTests();

  viz::TransferableResource resource1;
  viz::ReleaseCallback release_callback1;
  viz::TransferableResource resource2;
  viz::ReleaseCallback release_callback2;
  viz::TransferableResource resource3;
  viz::ReleaseCallback release_callback3;

  // Produce resources.
  EXPECT_FALSE(drawing_buffer_->MarkContentsChanged());
  drawing_buffer_->ClearFramebuffers(GL_STENCIL_BUFFER_BIT);
  VerifyStateWasRestored();
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource1,
                                                           &release_callback1));
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  drawing_buffer_->ClearFramebuffers(GL_DEPTH_BUFFER_BIT);
  VerifyStateWasRestored();
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource2,
                                                           &release_callback2));
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  drawing_buffer_->ClearFramebuffers(GL_COLOR_BUFFER_BIT);
  VerifyStateWasRestored();
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource3,
                                                           &release_callback3));

  EXPECT_EQ(sii->shared_image_count(), 4u);

  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  std::move(release_callback1).Run(gpu::SyncToken(), true /* lostResource */);
  EXPECT_EQ(sii->shared_image_count(), 3u);

  std::move(release_callback2).Run(gpu::SyncToken(), true /* lostResource */);
  EXPECT_EQ(sii->shared_image_count(), 2u);

  // The resource is not marked lost so it's recycled after the callback.
  std::move(release_callback3).Run(gpu::SyncToken(), false /* lostResource */);
  EXPECT_EQ(sii->shared_image_count(), 2u);

  drawing_buffer_->BeginDestruction();
}

TEST_F(DrawingBufferTest, VerifyCachedRecycledResourcesAreKept) {
  const size_t kNumResources = DrawingBuffer::kDefaultColorBufferCacheLimit + 1;
  std::vector<viz::TransferableResource> resources(kNumResources);
  std::vector<viz::ReleaseCallback> release_callbacks(kNumResources);

  // Produce resources.
  for (size_t i = 0; i < kNumResources; ++i) {
    drawing_buffer_->MarkContentsChanged();
    EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(
        nullptr, &resources[i], &release_callbacks[i]));
  }

  // Release resources.
  for (auto& release_callback : release_callbacks) {
    drawing_buffer_->MarkContentsChanged();
    std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
  }

  std::vector<viz::ReleaseCallback> recycled_release_callbacks(
      DrawingBuffer::kDefaultColorBufferCacheLimit);

  // The first recycled resource must be from the cache
  for (size_t i = 0; i < DrawingBuffer::kDefaultColorBufferCacheLimit; ++i) {
    viz::TransferableResource recycled_resource;
    viz::ReleaseCallback recycled_release_callback;
    drawing_buffer_->MarkContentsChanged();
    EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(
        nullptr, &recycled_resource, &recycled_release_callbacks[i]));

    bool recycled = false;
    for (auto& resource : resources) {
      if (recycled_resource.mailbox() == resource.mailbox()) {
        recycled = true;
        break;
      }
    }
    EXPECT_TRUE(recycled);
  }

  // The next recycled resource must be a new resource.
  viz::TransferableResource next_recycled_resource;
  viz::ReleaseCallback next_recycled_release_callback;
  drawing_buffer_->MarkContentsChanged();
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(
      nullptr, &next_recycled_resource, &next_recycled_release_callback));
  for (auto& resource : resources) {
    EXPECT_NE(resource.mailbox(), next_recycled_resource.mailbox());
  }
  recycled_release_callbacks.push_back(
      std::move(next_recycled_release_callback));

  // Cleanup
  for (auto& release_cb : recycled_release_callbacks) {
    std::move(release_cb).Run(gpu::SyncToken(), false /* lostResource */);
  }
  drawing_buffer_->BeginDestruction();
}

TEST_F(DrawingBufferTest, verifyInsertAndWaitSyncTokenCorrectly) {
  GLES2InterfaceForTests* gl_ = drawing_buffer_->ContextGLForTests();
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;

  // Produce resources.
  EXPECT_FALSE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));

  // Pretend to release the resource. We should not wait for the sync token yet
  // because the returned buffer is not recycled.
  gpu::SyncToken wait_sync_token;
  gl_->GenSyncTokenCHROMIUM(wait_sync_token.GetData());
  EXPECT_CALL(*gl_, WaitSyncTokenCHROMIUMMock(SyncTokenEq(wait_sync_token)))
      .Times(0);
  std::move(release_callback).Run(wait_sync_token, false /* lostResource */);
  testing::Mock::VerifyAndClearExpectations(gl_);

  // The returned buffer will be recycled in PrepareTransferrableResource. Make
  // sure we wait for the sync token now.
  EXPECT_CALL(*gl_, WaitSyncTokenCHROMIUMMock(SyncTokenEq(wait_sync_token)))
      .Times(1);
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  testing::Mock::VerifyAndClearExpectations(gl_);

  // Release the resource and begin destruction. We will not wait on the sync
  // token because the buffer is not reused. SharedImageInterface, however, will
  // wait on the sync token before destruction.
  gl_->GenSyncTokenCHROMIUM(wait_sync_token.GetData());
  EXPECT_CALL(*gl_, WaitSyncTokenCHROMIUMMock(SyncTokenEq(wait_sync_token)))
      .Times(0);
  std::move(release_callback).Run(wait_sync_token, false /* lostResource */);
  drawing_buffer_->BeginDestruction();
  testing::Mock::VerifyAndClearExpectations(gl_);
}

class DrawingBufferImageChromiumTest : public DrawingBufferTest,
                                       private ScopedWebGLImageChromiumForTest {
 public:
  DrawingBufferImageChromiumTest() : ScopedWebGLImageChromiumForTest(true) {}

 protected:
  void SetUp() override {
    gfx::Size initial_size(kInitialWidth, kInitialHeight);
    auto gl = std::make_unique<GLES2InterfaceForTests>();
    auto provider =
        std::make_unique<WebGraphicsContext3DProviderForTests>(std::move(gl));

    provider->GetMutableGpuFeatureInfo()
        .status_values[gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] =
        gpu::kGpuFeatureStatusEnabled;

    // DrawingBuffer requests MappableSharedImages with usage SCANOUT, whereas
    // TestSII by default creates backing SharedMemory GMBs that don't support
    // this usage. Configure the TestSII to instead use test GMBs that have
    // relaxed usage validation.
    auto* sii = static_cast<gpu::TestSharedImageInterface*>(
        provider->SharedImageInterface());
    sii->UseTestGMBInSharedImageCreationWithBufferUsage();
    GLES2InterfaceForTests* gl_ =
        static_cast<GLES2InterfaceForTests*>(provider->ContextGL());
    EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUMMock(_)).Times(1);
    Platform::GraphicsInfo graphics_info;
    graphics_info.using_gpu_compositing = true;
    drawing_buffer_ = DrawingBufferForTests::Create(
        std::move(provider), /*sii_provider_for_bitmap=*/nullptr, graphics_info,
        gl_, initial_size, DrawingBuffer::kPreserve, kDisableMultisampling);
    CHECK(drawing_buffer_);
    SetAndSaveRestoreState(true);
    testing::Mock::VerifyAndClearExpectations(gl_);
  }

  GLuint image_id0_;
};

TEST_F(DrawingBufferImageChromiumTest, VerifyResizingReallocatesImages) {
  GLES2InterfaceForTests* gl_ = drawing_buffer_->ContextGLForTests();
  gpu::TestSharedImageInterface* sii =
      drawing_buffer_->SharedImageInterfaceForTests();

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;

  gfx::Size initial_size(kInitialWidth, kInitialHeight);
  gfx::Size alternate_size(kInitialWidth, kAlternateHeight);

  // There should be currently one back buffer and therefore one SharedImage.
  gpu::Mailbox mailbox1;
  mailbox1.SetName(gl_->last_imported_shared_image()->name);
  EXPECT_EQ(1u, sii->shared_image_count());
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox1));

  // Produce one resource at size 100x100. This should create another buffer and
  // therefore another SharedImage.
  EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUMMock(_)).Times(1);
  EXPECT_FALSE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  EXPECT_EQ(initial_size, sii->MostRecentSize());
  EXPECT_TRUE(resource.is_overlay_candidate);
  EXPECT_EQ(initial_size, resource.size);
  testing::Mock::VerifyAndClearExpectations(gl_);
  VerifyStateWasRestored();
  gpu::Mailbox mailbox2;
  mailbox2.SetName(gl_->last_imported_shared_image()->name);
  EXPECT_EQ(2u, sii->shared_image_count());
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox1));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox2));
  EXPECT_EQ(mailbox2, resource.mailbox());

  // Resize to 100x50. The current backbuffer must be destroyed. The exported
  // resource should stay alive. A new backbuffer must be created.
  EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUMMock(_)).Times(1);
  drawing_buffer_->Resize(alternate_size);
  VerifyStateWasRestored();
  gpu::Mailbox mailbox3;
  mailbox3.SetName(gl_->last_imported_shared_image()->name);
  EXPECT_EQ(2u, sii->shared_image_count());
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox1));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox2));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox3));
  testing::Mock::VerifyAndClearExpectations(gl_);

  // Return the exported resource. Now it should get destroyed too.
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
  VerifyStateWasRestored();
  EXPECT_EQ(1u, sii->shared_image_count());
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox1));
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox2));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox3));

  // Produce a resource at the new size.
  EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUMMock(_)).Times(1);
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  EXPECT_EQ(alternate_size, sii->MostRecentSize());
  EXPECT_TRUE(resource.is_overlay_candidate);
  EXPECT_EQ(alternate_size, resource.size);
  gpu::Mailbox mailbox4;
  mailbox4.SetName(gl_->last_imported_shared_image()->name);
  EXPECT_EQ(2u, sii->shared_image_count());
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox3));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox4));
  EXPECT_EQ(mailbox4, resource.mailbox());
  testing::Mock::VerifyAndClearExpectations(gl_);

  // Reset to initial size. The exported resource has to stay alive, but the
  // current back buffer must be destroyed and a new one with the right size
  // must be created.
  EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUMMock(_)).Times(1);
  drawing_buffer_->Resize(initial_size);
  VerifyStateWasRestored();
  gpu::Mailbox mailbox5;
  mailbox5.SetName(gl_->last_imported_shared_image()->name);
  EXPECT_EQ(2u, sii->shared_image_count());
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox3));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox4));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox5));
  testing::Mock::VerifyAndClearExpectations(gl_);

  // Return the exported resource. Now it will be destroyed too.
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
  VerifyStateWasRestored();
  EXPECT_EQ(1u, sii->shared_image_count());
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox3));
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox4));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox5));

  // Prepare another resource and verify that it's the correct size.
  EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUMMock(_)).Times(1);
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  EXPECT_EQ(initial_size, sii->MostRecentSize());
  EXPECT_TRUE(resource.is_overlay_candidate);
  EXPECT_EQ(initial_size, resource.size);
  testing::Mock::VerifyAndClearExpectations(gl_);
  gpu::Mailbox mailbox6;
  mailbox6.SetName(gl_->last_imported_shared_image()->name);
  EXPECT_EQ(2u, sii->shared_image_count());
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox5));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox6));

  // Prepare one final resource and verify that it's the correct size. We should
  // recycle the previously exported resource and avoid allocating a new
  // SharedImage.
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));
  EXPECT_EQ(initial_size, sii->MostRecentSize());
  EXPECT_TRUE(resource.is_overlay_candidate);
  EXPECT_EQ(initial_size, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
  EXPECT_EQ(2u, sii->shared_image_count());
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox5));
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox6));

  drawing_buffer_->BeginDestruction();
  testing::Mock::VerifyAndClearExpectations(sii);
  EXPECT_EQ(0u, sii->shared_image_count());
}

TEST_F(DrawingBufferImageChromiumTest, AllocationFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kDrawingBufferWithoutGpuMemoryBuffer);

  GLES2InterfaceForTests* gl_ = drawing_buffer_->ContextGLForTests();
  gpu::TestSharedImageInterface* sii =
      drawing_buffer_->SharedImageInterfaceForTests();

  viz::TransferableResource resource1;
  viz::ReleaseCallback release_callback1;
  viz::TransferableResource resource2;
  viz::ReleaseCallback release_callback2;
  viz::TransferableResource resource3;
  viz::ReleaseCallback release_callback3;

  // Request a resource. A SharedImage should already be created. Everything
  // works as expected.
  EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUMMock(_)).Times(1);
  EXPECT_FALSE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource1,
                                                           &release_callback1));
  EXPECT_TRUE(resource1.is_overlay_candidate);
  gpu::Mailbox mailbox1;
  mailbox1.SetName(gl_->last_imported_shared_image()->name);
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox1));
  testing::Mock::VerifyAndClearExpectations(gl_);
  VerifyStateWasRestored();

  // Force MappableSI creation failure. Request another resource. It should
  // still be provided, but this time with allowOverlay = false.
  EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUMMock(_)).Times(1);
  sii->SetFailSharedImageCreationWithBufferUsage(true);
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource2,
                                                           &release_callback2));
  EXPECT_FALSE(resource2.is_overlay_candidate);
  gpu::Mailbox mailbox2;
  mailbox2.SetName(gl_->last_imported_shared_image()->name);
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox2));
  VerifyStateWasRestored();

  // Check that if MappableSI creation starts working again, resources
  // are correctly created with allowOverlay = true.
  EXPECT_CALL(*gl_, CreateAndTexStorage2DSharedImageCHROMIUMMock(_)).Times(1);
  sii->SetFailSharedImageCreationWithBufferUsage(false);
  EXPECT_TRUE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource3,
                                                           &release_callback3));
  EXPECT_TRUE(resource3.is_overlay_candidate);
  gpu::Mailbox mailbox3;
  mailbox3.SetName(gl_->last_imported_shared_image()->name);
  EXPECT_TRUE(sii->CheckSharedImageExists(mailbox3));
  testing::Mock::VerifyAndClearExpectations(gl_);
  VerifyStateWasRestored();

  std::move(release_callback1).Run(gpu::SyncToken(), false /* lostResource */);
  std::move(release_callback2).Run(gpu::SyncToken(), false /* lostResource */);
  std::move(release_callback3).Run(gpu::SyncToken(), false /* lostResource */);

  drawing_buffer_->BeginDestruction();
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox1));
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox2));
  EXPECT_FALSE(sii->CheckSharedImageExists(mailbox3));
}

class DepthStencilTrackingGLES2Interface
    : public gpu::gles2::GLES2InterfaceStub {
 public:
  void FramebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               GLuint renderbuffer) override {
    switch (attachment) {
      case GL_DEPTH_ATTACHMENT:
        depth_attachment_ = renderbuffer;
        break;
      case GL_STENCIL_ATTACHMENT:
        stencil_attachment_ = renderbuffer;
        break;
      case GL_DEPTH_STENCIL_ATTACHMENT:
        depth_stencil_attachment_ = renderbuffer;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  GLenum CheckFramebufferStatus(GLenum target) override {
    return GL_FRAMEBUFFER_COMPLETE;
  }

  void GetIntegerv(GLenum ptype, GLint* value) override {
    switch (ptype) {
      case GL_MAX_TEXTURE_SIZE:
        *value = 1024;
        return;
    }
  }

  const GLubyte* GetString(GLenum type) override {
    if (type == GL_EXTENSIONS)
      return reinterpret_cast<const GLubyte*>("GL_OES_packed_depth_stencil");
    return reinterpret_cast<const GLubyte*>("");
  }

  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override {
    for (GLsizei i = 0; i < n; ++i)
      renderbuffers[i] = next_gen_renderbuffer_id_++;
  }

  GLuint StencilAttachment() const { return stencil_attachment_; }
  GLuint DepthAttachment() const { return depth_attachment_; }
  GLuint DepthStencilAttachment() const { return depth_stencil_attachment_; }
  size_t NumAllocatedRenderBuffer() const {
    return next_gen_renderbuffer_id_ - 1;
  }

 private:
  GLuint next_gen_renderbuffer_id_ = 1;
  GLuint depth_attachment_ = 0;
  GLuint stencil_attachment_ = 0;
  GLuint depth_stencil_attachment_ = 0;
};

struct DepthStencilTestCase {
  DepthStencilTestCase(bool request_stencil,
                       bool request_depth,
                       int expected_render_buffers,
                       const char* const test_case_name)
      : request_stencil(request_stencil),
        request_depth(request_depth),
        expected_render_buffers(expected_render_buffers),
        test_case_name(test_case_name) {}

  bool request_stencil;
  bool request_depth;
  size_t expected_render_buffers;
  const char* const test_case_name;
};

// This tests that, when the packed depth+stencil extension is supported, and
// either depth or stencil is requested, DrawingBuffer always allocates a single
// packed renderbuffer and properly computes the actual context attributes as
// defined by WebGL. We always allocate a packed buffer in this case since many
// desktop OpenGL drivers that support this extension do not consider a
// framebuffer with only a depth or a stencil buffer attached to be complete.
TEST(DrawingBufferDepthStencilTest, packedDepthStencilSupported) {
  DepthStencilTestCase cases[] = {
      DepthStencilTestCase(false, false, 0, "neither"),
      DepthStencilTestCase(true, false, 1, "stencil only"),
      DepthStencilTestCase(false, true, 1, "depth only"),
      DepthStencilTestCase(true, true, 1, "both"),
  };

  for (size_t i = 0; i < std::size(cases); i++) {
    SCOPED_TRACE(cases[i].test_case_name);
    auto gl = std::make_unique<DepthStencilTrackingGLES2Interface>();
    DepthStencilTrackingGLES2Interface* tracking_gl = gl.get();
    auto provider =
        std::make_unique<WebGraphicsContext3DProviderForTests>(std::move(gl));
    DrawingBuffer::PreserveDrawingBuffer preserve = DrawingBuffer::kPreserve;

    Platform::GraphicsInfo graphics_info;
    graphics_info.using_gpu_compositing = true;
    bool premultiplied_alpha = false;
    bool want_alpha_channel = true;
    bool want_depth_buffer = cases[i].request_depth;
    bool want_stencil_buffer = cases[i].request_stencil;
    bool want_antialiasing = false;
    bool using_swap_chain = false;
    bool desynchronized = false;
    scoped_refptr<DrawingBuffer> drawing_buffer = DrawingBuffer::Create(
        std::move(provider), graphics_info, using_swap_chain, nullptr,
        gfx::Size(10, 10), premultiplied_alpha, want_alpha_channel,
        want_depth_buffer, want_stencil_buffer, want_antialiasing,
        desynchronized, preserve, DrawingBuffer::kWebGL1,
        DrawingBuffer::kAllowChromiumImage, cc::PaintFlags::FilterQuality::kLow,
        PredefinedColorSpace::kSRGB, gl::GpuPreference::kHighPerformance);

    // When we request a depth or a stencil buffer, we will get both.
    EXPECT_EQ(cases[i].request_depth || cases[i].request_stencil,
              drawing_buffer->HasDepthBuffer());
    EXPECT_EQ(cases[i].request_depth || cases[i].request_stencil,
              drawing_buffer->HasStencilBuffer());
    EXPECT_EQ(cases[i].expected_render_buffers,
              tracking_gl->NumAllocatedRenderBuffer());
    if (cases[i].request_depth || cases[i].request_stencil) {
      EXPECT_NE(0u, tracking_gl->DepthStencilAttachment());
      EXPECT_EQ(0u, tracking_gl->DepthAttachment());
      EXPECT_EQ(0u, tracking_gl->StencilAttachment());
    } else {
      EXPECT_EQ(0u, tracking_gl->DepthStencilAttachment());
      EXPECT_EQ(0u, tracking_gl->DepthAttachment());
      EXPECT_EQ(0u, tracking_gl->StencilAttachment());
    }

    drawing_buffer->Resize(gfx::Size(10, 20));
    EXPECT_EQ(cases[i].request_depth || cases[i].request_stencil,
              drawing_buffer->HasDepthBuffer());
    EXPECT_EQ(cases[i].request_depth || cases[i].request_stencil,
              drawing_buffer->HasStencilBuffer());
    EXPECT_EQ(cases[i].expected_render_buffers,
              tracking_gl->NumAllocatedRenderBuffer());
    if (cases[i].request_depth || cases[i].request_stencil) {
      EXPECT_NE(0u, tracking_gl->DepthStencilAttachment());
      EXPECT_EQ(0u, tracking_gl->DepthAttachment());
      EXPECT_EQ(0u, tracking_gl->StencilAttachment());
    } else {
      EXPECT_EQ(0u, tracking_gl->DepthStencilAttachment());
      EXPECT_EQ(0u, tracking_gl->DepthAttachment());
      EXPECT_EQ(0u, tracking_gl->StencilAttachment());
    }

    drawing_buffer->BeginDestruction();
  }
}

TEST_F(DrawingBufferTest, VerifySetIsHiddenProperlyAffectsMailboxes) {
  GLES2InterfaceForTests* gl_ = drawing_buffer_->ContextGLForTests();
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;

  // Produce resources.
  EXPECT_FALSE(drawing_buffer_->MarkContentsChanged());
  EXPECT_TRUE(drawing_buffer_->PrepareTransferableResource(nullptr, &resource,
                                                           &release_callback));

  gpu::SyncToken wait_sync_token;
  gl_->GenSyncTokenCHROMIUM(wait_sync_token.GetData());
  drawing_buffer_->SetIsInHiddenPage(true);
  // m_drawingBuffer deletes resource immediately when hidden.
  EXPECT_CALL(*gl_, WaitSyncTokenCHROMIUMMock(SyncTokenEq(wait_sync_token)))
      .Times(0);
  std::move(release_callback).Run(wait_sync_token, false /* lostResource */);
  testing::Mock::VerifyAndClearExpectations(gl_);

  drawing_buffer_->BeginDestruction();
}

TEST_F(DrawingBufferTest,
       VerifyTooBigDrawingBufferExceedingV8MaxSizeFailsToCreate) {
  constexpr size_t kBytesPerPixel = 4;
  constexpr size_t kMaxSize = v8::TypedArray::kMaxByteLength / kBytesPerPixel;

  // Statically compute a width and height such that the product is above
  // kMaxSize.
  constexpr int kWidth = 1 << 30;
  constexpr int kHeight = (kMaxSize / kWidth) + 1;
  static_assert(size_t{kWidth} * (kHeight - 1) <= kMaxSize);
  static_assert(size_t{kWidth} * kHeight > kMaxSize);

  gfx::Size too_big_size(kWidth, kHeight);
  Platform::GraphicsInfo graphics_info;
  graphics_info.using_gpu_compositing = true;
  scoped_refptr<DrawingBuffer> too_big_drawing_buffer = DrawingBuffer::Create(
      nullptr, graphics_info, false /* using_swap_chain */, nullptr,
      too_big_size, false, false, false, false, false,
      /*desynchronized=*/false, DrawingBuffer::kDiscard, DrawingBuffer::kWebGL1,
      DrawingBuffer::kAllowChromiumImage, cc::PaintFlags::FilterQuality::kLow,
      PredefinedColorSpace::kSRGB, gl::GpuPreference::kHighPerformance);
  EXPECT_EQ(too_big_drawing_buffer, nullptr);
  drawing_buffer_->BeginDestruction();
}

}  // namespace blink
