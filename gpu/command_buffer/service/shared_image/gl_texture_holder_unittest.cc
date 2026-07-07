// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_holder.h"

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gpu {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;

class GLTextureHolderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gl::SetGLGetProcAddressProc(gl::MockGLInterface::GetGLProcAddress);
    display_ = gl::GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_ = std::make_unique<NiceMock<gl::MockGLInterface>>();
    gl::MockGLInterface::SetGLInterface(gl_.get());

    surface_ = base::MakeRefCounted<gl::GLSurfaceStub>();
    context_ = base::MakeRefCounted<gl::GLContextStub>();
    context_->SetGLVersionString("OpenGL ES 2.0");
    context_->SetExtensionsString("");
    context_->Initialize(surface_.get(), {});
    context_->MakeCurrent(surface_.get());

    ON_CALL(*gl_, CheckFramebufferStatusEXT(_))
        .WillByDefault(Return(GL_FRAMEBUFFER_COMPLETE));
  }

  void TearDown() override {
    context_ = nullptr;
    surface_ = nullptr;
    gl::MockGLInterface::SetGLInterface(nullptr);
    gl_.reset();
    gl::GLSurfaceTestSupport::ShutdownGL(display_);
  }

  std::unique_ptr<NiceMock<gl::MockGLInterface>> gl_;
  scoped_refptr<gl::GLContextStub> context_;
  scoped_refptr<gl::GLSurfaceStub> surface_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

// ReadbackToMemory creates a temporary FBO for glReadPixels. The previous
// framebuffer binding must be restored before the temporary FBO is deleted so
// that the temporary FBO is never the previously bound FBO at the next bind
// transition. Some drivers retain an internal reference to the previously
// bound FBO across bind transitions; see the
// ensure_previous_framebuffer_not_deleted driver workaround.
TEST_F(GLTextureHolderTest, ReadbackToMemoryRestoresFramebufferBeforeDelete) {
  constexpr GLuint kTextureId = 11;
  constexpr GLuint kTempFboId = 22;
  constexpr GLint kPrevFboId = 33;
  constexpr gfx::Size kSize(4, 4);

  auto holder = base::MakeRefCounted<GLTextureHolder>(
      viz::SinglePlaneFormat::kRGBA_8888, kSize,
      /*is_passthrough=*/true,
      /*progress_reporter=*/nullptr);
  GLFormatDesc format_desc;
  format_desc.data_format = GL_RGBA;
  format_desc.data_type = GL_UNSIGNED_BYTE;
  format_desc.target = GL_TEXTURE_2D;
  auto texture = base::MakeRefCounted<gles2::TexturePassthrough>(kTextureId,
                                                                 GL_TEXTURE_2D);
  holder->InitializeWithTexture(format_desc, texture);

  ON_CALL(*gl_, GetIntegerv(GL_FRAMEBUFFER_BINDING, _))
      .WillByDefault(SetArgPointee<1>(kPrevFboId));

  {
    InSequence seq;
    EXPECT_CALL(*gl_, GenFramebuffersEXT(1, _))
        .WillOnce(SetArgPointee<1>(kTempFboId));
    EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER, kTempFboId));
    EXPECT_CALL(*gl_, ReadPixels(0, 0, kSize.width(), kSize.height(), GL_RGBA,
                                 GL_UNSIGNED_BYTE, _));
    // The previous framebuffer binding must be restored before the temporary
    // FBO is deleted.
    EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER, kPrevFboId));
    EXPECT_CALL(*gl_, DeleteFramebuffersEXT(1, Pointee(kTempFboId)));
  }

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(kSize.width(), kSize.height(),
                                       kRGBA_8888_SkColorType,
                                       kPremul_SkAlphaType));
  EXPECT_TRUE(holder->ReadbackToMemory(bitmap.pixmap()));

  texture->MarkContextLost();
}

}  // namespace
}  // namespace gpu
