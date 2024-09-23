// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include "base/command_line.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::StrEq;

namespace gpu {
namespace gles2 {

class GLES2DecoderTest1 : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest1() = default;
};

class GLES3DecoderTest1 : public GLES2DecoderTest1 {
 public:
  GLES3DecoderTest1() { shader_language_version_ = 300; }
 protected:
  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 3.0";
    init.bind_generates_resource = true;
    init.context_type = CONTEXT_TYPE_OPENGLES3;
    InitDecoder(init);
  }
};

INSTANTIATE_TEST_SUITE_P(Service, GLES2DecoderTest1, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(Service, GLES3DecoderTest1, ::testing::Bool());

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::GenerateMipmap, 0>(
    bool valid) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::CheckFramebufferStatus, 0>(
    bool /* valid */) {
  // Give it a valid framebuffer.
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1, GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Clear, 0>(bool valid) {
  if (valid) {
    SetupExpectationsForApplyingDefaultDirtyState();
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::ColorMask, 0>(
    bool /* valid */) {
  // We bind a framebuffer color the colormask test since the framebuffer
  // will be considered RGB.
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::CopyTexImage2D, 0>(
    bool valid) {
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::CopyTexSubImage2D, 0>(
    bool valid) {
  if (valid) {
    DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
    DoTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 16, 16, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset);
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::DetachShader, 0>(bool valid) {
  if (valid) {
    EXPECT_CALL(*gl_,
                AttachShader(kServiceProgramId, kServiceShaderId))
        .Times(1)
        .RetiresOnSaturation();
    cmds::AttachShader attach_cmd;
    attach_cmd.Init(client_program_id_, client_shader_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::FramebufferRenderbuffer, 0>(
    bool valid) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                     kServiceRenderbufferId);
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::FramebufferTextureLayer, 0>(
    bool valid) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<
    cmds::GetBufferParameteri64v, 0>(bool /* valid */) {
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<
    cmds::GetBufferParameteriv, 0>(bool /* valid */) {
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<
    cmds::GetFramebufferAttachmentParameteriv, 0>(bool /* valid */) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
}

template <>
void GLES2DecoderTestBase::
    SpecializedSetup<cmds::GenTransformFeedbacksImmediate, 0>(bool valid) {
  if (valid) {
    // Transform feedbacks are per-context, not per-group, so they get cleaned
    // up at the end of the test.
    EXPECT_CALL(*gl_, DeleteTransformFeedbacks(_, _))
        .Times(1)
        .RetiresOnSaturation();
  }
}

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_1_autogen.h"

}  // namespace gles2
}  // namespace gpu
