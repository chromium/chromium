// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/shared_bitmap_id_registrar.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/config/gpu_feature_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

// These unit tests are separate from DrawingBufferTests.cpp because they are
// built as a part of webkit_unittests instead blink_platform_unittests. This is
// because the software rendering mode has a dependency on the blink::Platform
// interface for buffer allocations.

namespace blink {
namespace {

class TestSharedBitmapIdRegistar : public cc::SharedBitmapIdRegistrar {
  cc::SharedBitmapIdRegistration RegisterSharedBitmapId(
      const viz::SharedBitmapId& id,
      scoped_refptr<cc::CrossThreadSharedBitmap> bitmap) override {
    return {};
  }
};

class DrawingBufferSoftwareCompositingTest : public testing::Test {
 protected:
  void SetUp() override {
    gfx::Size initial_size(kInitialWidth, kInitialHeight);
    auto gl = std::make_unique<GLES2InterfaceForTests>();
    auto provider =
        std::make_unique<WebGraphicsContext3DProviderForTests>(std::move(gl));
    GLES2InterfaceForTests* gl_ =
        static_cast<GLES2InterfaceForTests*>(provider->ContextGL());
    auto sii_provider_for_bitmap =
        TestWebGraphicsSharedImageInterfaceProvider::Create();
    Platform::GraphicsInfo graphics_info;
    graphics_info.using_gpu_compositing = false;

    drawing_buffer_ = DrawingBufferForTests::Create(
        std::move(provider), std::move(sii_provider_for_bitmap), graphics_info,
        gl_, initial_size, DrawingBuffer::kPreserve, kDisableMultisampling);
    CHECK(drawing_buffer_);
  }

  test::TaskEnvironment task_environment_;
  scoped_refptr<DrawingBufferForTests> drawing_buffer_;
  TestSharedBitmapIdRegistar test_shared_bitmap_id_registrar_;
};

TEST_F(DrawingBufferSoftwareCompositingTest, BitmapRecycling) {
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback1;
  viz::ReleaseCallback release_callback2;
  viz::ReleaseCallback release_callback3;
  gfx::Size initial_size(kInitialWidth, kInitialHeight);
  gfx::Size alternate_size(kInitialWidth, kAlternateHeight);

  drawing_buffer_->Resize(initial_size);
  drawing_buffer_->MarkContentsChanged();
  drawing_buffer_->PrepareTransferableResource(
      &test_shared_bitmap_id_registrar_, &resource,
      &release_callback1);  // create a bitmap.
  EXPECT_EQ(0, drawing_buffer_->RecycledBitmapCount());
  std::move(release_callback1)
      .Run(gpu::SyncToken(),
           false /* lostResource */);  // release bitmap to the recycling queue
  EXPECT_EQ(1, drawing_buffer_->RecycledBitmapCount());
  drawing_buffer_->MarkContentsChanged();
  drawing_buffer_->PrepareTransferableResource(
      &test_shared_bitmap_id_registrar_, &resource,
      &release_callback2);  // recycle a bitmap.
  EXPECT_EQ(0, drawing_buffer_->RecycledBitmapCount());
  std::move(release_callback2)
      .Run(gpu::SyncToken(),
           false /* lostResource */);  // release bitmap to the recycling queue
  EXPECT_EQ(1, drawing_buffer_->RecycledBitmapCount());
  drawing_buffer_->Resize(alternate_size);
  drawing_buffer_->MarkContentsChanged();
  // Regression test for crbug.com/647896 - Next line must not crash
  drawing_buffer_->PrepareTransferableResource(
      &test_shared_bitmap_id_registrar_, &resource,
      &release_callback3);  // cause recycling queue to be purged due to resize
  EXPECT_EQ(0, drawing_buffer_->RecycledBitmapCount());
  std::move(release_callback3).Run(gpu::SyncToken(), false /* lostResource */);
  EXPECT_EQ(1, drawing_buffer_->RecycledBitmapCount());

  drawing_buffer_->BeginDestruction();
}

TEST_F(DrawingBufferSoftwareCompositingTest, FramebufferBinding) {
  GLES2InterfaceForTests* gl_ = drawing_buffer_->ContextGLForTests();
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  gfx::Size initial_size(kInitialWidth, kInitialHeight);
  GLint drawBinding = 0, readBinding = 0;

  GLuint draw_framebuffer_binding = 0xbeef3;
  GLuint read_framebuffer_binding = 0xbeef4;
  gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer_binding);
  gl_->BindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer_binding);
  gl_->SaveState();
  drawing_buffer_->Resize(initial_size);
  drawing_buffer_->MarkContentsChanged();
  drawing_buffer_->PrepareTransferableResource(
      &test_shared_bitmap_id_registrar_, &resource, &release_callback);
  gl_->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawBinding);
  gl_->GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readBinding);
  EXPECT_EQ(static_cast<GLint>(draw_framebuffer_binding), drawBinding);
  EXPECT_EQ(static_cast<GLint>(read_framebuffer_binding), readBinding);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);

  drawing_buffer_->BeginDestruction();
}

}  // unnamed namespace
}  // blink
