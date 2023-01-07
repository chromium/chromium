// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_context_virtual.h"

#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"

namespace gpu {
namespace gles2 {
namespace {

using testing::AnyNumber;
using testing::Return;

class GLContextVirtualTest : public GpuServiceTest {
 public:
  GLContextVirtualTest()
      : decoder_(new MockGLES2Decoder(&client_,
                                      &command_buffer_service_,
                                      &outputter_)) {}
  ~GLContextVirtualTest() override = default;

 protected:
  FakeCommandBufferServiceBase command_buffer_service_;
  FakeDecoderClient client_;
  TraceOutputter outputter_;
  std::unique_ptr<MockGLES2Decoder> decoder_;
};

// Tests that destroying a GLContextVirtual doesn't leave stale state that
// prevents a new one from initializing.
TEST_F(GLContextVirtualTest, Reinitialize) {
  EXPECT_CALL(*gl_, GetError())
      .Times(AnyNumber())
      .WillRepeatedly(Return(GL_NO_ERROR));
  EXPECT_CALL(*gl_, GetString(GL_VERSION))
      .Times(AnyNumber())
      .WillRepeatedly(Return(reinterpret_cast<unsigned const char *>("2.0")));
  EXPECT_CALL(*gl_, GetString(GL_EXTENSIONS))
      .Times(AnyNumber())
      .WillRepeatedly(Return(reinterpret_cast<unsigned const char *>("")));
  {
    auto base_context = base::MakeRefCounted<gl::GLContextStub>();
    gl::GLShareGroup* share_group = base_context->share_group();
    share_group->SetSharedContext(base_context.get());
    auto context = base::MakeRefCounted<GLContextVirtual>(
        share_group, base_context.get(), decoder_->AsWeakPtr());
    EXPECT_TRUE(context->Initialize(GetGLSurface(), gl::GLContextAttribs()));
    EXPECT_TRUE(context->MakeCurrent(GetGLSurface()));
  }
  {
    auto base_context = base::MakeRefCounted<gl::GLContextStub>();
    gl::GLShareGroup* share_group = base_context->share_group();
    share_group->SetSharedContext(base_context.get());
    auto context = base::MakeRefCounted<GLContextVirtual>(
        share_group, base_context.get(), decoder_->AsWeakPtr());
    EXPECT_TRUE(context->Initialize(GetGLSurface(), gl::GLContextAttribs()));
    EXPECT_TRUE(context->MakeCurrent(GetGLSurface()));
  }
}

// Tests that CheckStickyGraphicsResetStatus gets the state from the real
// context, but "virtualizes" the guilty party (i.e. makes it unknown).
TEST_F(GLContextVirtualTest, CheckStickyGraphicsResetStatus) {
  EXPECT_CALL(*gl_, GetError())
      .Times(AnyNumber())
      .WillRepeatedly(Return(GL_NO_ERROR));
  auto base_context = base::MakeRefCounted<gl::GLContextStub>();
  const char gl_extensions[] = "GL_KHR_robustness";
  base_context->SetExtensionsString(gl_extensions);

  gl::GLShareGroup* share_group = base_context->share_group();
  share_group->SetSharedContext(base_context.get());
  auto context = base::MakeRefCounted<GLContextVirtual>(
      share_group, base_context.get(), decoder_->AsWeakPtr());
  EXPECT_TRUE(context->Initialize(GetGLSurface(), gl::GLContextAttribs()));
  EXPECT_TRUE(context->MakeCurrent(GetGLSurface()));

  // If no reset, GLContextVirtual should report no error.
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB()).WillOnce(Return(GL_NO_ERROR));
  EXPECT_EQ(unsigned{GL_NO_ERROR}, context->CheckStickyGraphicsResetStatus());

  // If reset, GLContextVirtual should report an error, but with an unknown
  // guilty context.
  EXPECT_CALL(*gl_, GetGraphicsResetStatusARB())
      .WillOnce(Return(GL_GUILTY_CONTEXT_RESET_ARB));
  EXPECT_EQ(unsigned{GL_UNKNOWN_CONTEXT_RESET_ARB},
            context->CheckStickyGraphicsResetStatus());
  // The underlying real context still knows, though.
  EXPECT_EQ(unsigned{GL_GUILTY_CONTEXT_RESET_ARB},
            base_context->CheckStickyGraphicsResetStatus());
}

}  // anonymous namespace
}  // namespace gles2
}  // namespace gpu
