// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_surface_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"

#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MatcherCast;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArrayArgument;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

class GLES2DecoderTestWithExtensionsOnGLES2 : public GLES2DecoderTest {
 public:
  GLES2DecoderTestWithExtensionsOnGLES2() = default;

  void SetUp() override {}
  void Init(const char* extensions) {
    InitState init;
    init.extensions = extensions;
    init.gl_version = "OpenGL ES 2.0";
    init.has_alpha = true;
    init.has_depth = true;
    init.request_alpha = true;
    init.request_depth = true;
    InitDecoder(init);
  }
};

TEST_P(GLES2DecoderTest, CheckFramebufferStatusWithNoBoundTarget) {
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(_)).Times(0);
  auto* result = static_cast<cmds::CheckFramebufferStatus::Result*>(
      shared_memory_address_);
  *result = 0;
  cmds::CheckFramebufferStatus cmd;
  cmd.Init(GL_FRAMEBUFFER, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE), *result);
}

TEST_P(GLES2DecoderWithShaderTest, BindAndDeleteFramebuffer) {
  SetupTexture();
  SetupExpectationsForApplyingDefaultDirtyState();
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoDeleteFramebuffer(client_framebuffer_id_,
                      kServiceFramebufferId,
                      true,
                      GL_FRAMEBUFFER,
                      0,
                      true,
                      GL_FRAMEBUFFER,
                      0);
  EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
      .Times(1)
      .RetiresOnSaturation();
  cmds::DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, FramebufferRenderbufferWithNoBoundTarget) {
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(_, _, _, _)).Times(0);
  cmds::FramebufferRenderbuffer cmd;
  cmd.Init(GL_FRAMEBUFFER,
           GL_COLOR_ATTACHMENT0,
           GL_RENDERBUFFER,
           client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, FramebufferTexture2DWithNoBoundTarget) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER,
           GL_COLOR_ATTACHMENT0,
           GL_TEXTURE_2D,
           client_texture_id_,
           0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTexture2DWithNoBoundTarget) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER,
           GL_COLOR_ATTACHMENT0,
           GL_TEXTURE_2D,
           client_texture_id_,
           1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, FramebufferTexture2DValidArgs) {
  EXPECT_CALL(*gl_,
              FramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, kServiceTextureId, 0));
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
           client_texture_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTexture2DValidArgs) {
  EXPECT_CALL(*gl_,
              FramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, kServiceTextureId, 1));
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
           client_texture_id_, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTexture2DDepthStencil) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              FramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_TEXTURE_2D, kServiceTextureId, 4))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              FramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                      GL_TEXTURE_2D, kServiceTextureId, 4))
      .Times(1)
      .RetiresOnSaturation();
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
           client_texture_id_, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  Framebuffer* framebuffer = GetFramebuffer(client_framebuffer_id_);
  ASSERT_TRUE(framebuffer);
  ASSERT_FALSE(framebuffer->GetAttachment(GL_DEPTH_STENCIL_ATTACHMENT));
  ASSERT_TRUE(framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT));
  ASSERT_TRUE(framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT));
}

TEST_P(GLES2DecoderTest, FramebufferTexture2DInvalidArgs0_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_RENDERBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
           client_texture_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTexture2DInvalidArgs0_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_RENDERBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
           client_texture_id_, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, FramebufferTexture2DInvalidArgs1_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR, GL_TEXTURE_2D, client_texture_id_, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTexture2DInvalidArgs1_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR, GL_TEXTURE_2D, client_texture_id_, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, FramebufferTexture2DInvalidArgs2_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_PROXY_TEXTURE_CUBE_MAP,
           client_texture_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTexture2DInvalidArgs2_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_PROXY_TEXTURE_CUBE_MAP,
           client_texture_id_, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, FramebufferTexture2DInvalidArgs4_0) {
  EXPECT_CALL(*gl_, FramebufferTexture2DEXT(_, _, _, _, _)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
           client_texture_id_, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTexture2DValidArgs4_0) {
  EXPECT_CALL(*gl_,
              FramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D, kServiceTextureId, 0));
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
           client_texture_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, FramebufferTexture2DMismatchedTexture1) {
  EXPECT_CALL(*gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _))
      .Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
           client_texture_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, FramebufferTexture2DMismatchedTexture2) {
  EXPECT_CALL(*gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _))
      .Times(0);
  DoBindTexture(GL_TEXTURE_CUBE_MAP, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferTexture2D cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
           client_texture_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, GetFramebufferAttachmentParameterivWithNoBoundTarget) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetFramebufferAttachmentParameterivEXT(_, _, _, _))
      .Times(0);
  cmds::GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(GL_FRAMEBUFFER,
           GL_COLOR_ATTACHMENT0,
           GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, GetFramebufferAttachmentParameterivWithRenderbuffer) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                     kServiceRenderbufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillRepeatedly(Return(GL_NO_ERROR));
  EXPECT_CALL(*gl_,
              FramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                                         GL_COLOR_ATTACHMENT0,
                                         GL_RENDERBUFFER,
                                         kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  auto* result =
      static_cast<cmds::GetFramebufferAttachmentParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  const GLint* result_value = result->GetData();
  cmds::FramebufferRenderbuffer fbrb_cmd;
  cmds::GetFramebufferAttachmentParameteriv cmd;
  fbrb_cmd.Init(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                GL_RENDERBUFFER,
                client_renderbuffer_id_);
  cmd.Init(GL_FRAMEBUFFER,
           GL_COLOR_ATTACHMENT0,
           GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbrb_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(client_renderbuffer_id_, static_cast<GLuint>(*result_value));
}

TEST_P(GLES2DecoderTest, GetFramebufferAttachmentParameterivWithTexture) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillRepeatedly(Return(GL_NO_ERROR));
  EXPECT_CALL(*gl_,
              FramebufferTexture2DEXT(GL_FRAMEBUFFER,
                                      GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D,
                                      kServiceTextureId,
                                      0))
      .Times(1)
      .RetiresOnSaturation();
  auto* result =
      static_cast<cmds::GetFramebufferAttachmentParameteriv::Result*>(
          shared_memory_address_);
  result->SetNumResults(0);
  const GLint* result_value = result->GetData();
  cmds::FramebufferTexture2D fbtex_cmd;
  cmds::GetFramebufferAttachmentParameteriv cmd;
  fbtex_cmd.Init(GL_FRAMEBUFFER,
                 GL_COLOR_ATTACHMENT0,
                 GL_TEXTURE_2D,
                 client_texture_id_,
                 0);
  cmd.Init(GL_FRAMEBUFFER,
           GL_COLOR_ATTACHMENT0,
           GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbtex_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(client_texture_id_, static_cast<GLuint>(*result_value));
}

TEST_P(GLES2DecoderWithShaderTest,
       GetRenderbufferParameterivRebindRenderbuffer) {
  SetupTexture();
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1, GL_NO_ERROR);

  cmds::GetRenderbufferParameteriv cmd;
  cmd.Init(GL_RENDERBUFFER,
           GL_RENDERBUFFER_RED_SIZE,
           shared_memory_id_,
           shared_memory_offset_);

  RestoreRenderbufferBindings();
  EnsureRenderbufferBound(true);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              GetRenderbufferParameterivEXT(
                  GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE, _));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, GetRenderbufferParameterivWithNoBoundTarget) {
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _)).Times(0);
  cmds::GetRenderbufferParameteriv cmd;
  cmd.Init(GL_RENDERBUFFER,
           GL_RENDERBUFFER_WIDTH,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, RenderbufferStorageRebindRenderbuffer) {
  SetupTexture();
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  RestoreRenderbufferBindings();
  EnsureRenderbufferBound(true);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1, GL_NO_ERROR);
}

TEST_P(GLES2DecoderTest, RenderbufferStorageWithNoBoundTarget) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

namespace {

// A class to emulate glReadPixels
class ReadPixelsEmulator {
 public:
  // pack_alignment is the alignment you want ReadPixels to use
  // when copying. The actual data passed in pixels should be contiguous.
  ReadPixelsEmulator(GLsizei width,
                     GLsizei height,
                     GLint bytes_per_pixel,
                     const void* src_pixels,
                     const void* expected_pixels,
                     GLint pack_alignment)
      : width_(width),
        height_(height),
        pack_alignment_(pack_alignment),
        bytes_per_pixel_(bytes_per_pixel),
        src_pixels_(reinterpret_cast<const int8_t*>(src_pixels)),
        expected_pixels_(reinterpret_cast<const int8_t*>(expected_pixels)) {}

  void ReadPixels(GLint x,
                  GLint y,
                  GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  void* pixels) const {
    DCHECK_GE(x, 0);
    DCHECK_GE(y, 0);
    DCHECK_LE(x + width, width_);
    DCHECK_LE(y + height, height_);
    for (GLint yy = 0; yy < height; ++yy) {
      const int8_t* src = GetPixelAddress(src_pixels_, x, y + yy);
      const void* dst = ComputePackAlignmentAddress(0, yy, width, pixels);
      memcpy(const_cast<void*>(dst), src, width * bytes_per_pixel_);
    }
  }

  bool CompareRowSegment(GLint x,
                         GLint y,
                         GLsizei width,
                         const void* data) const {
    DCHECK(x + width <= width_ || width == 0);
    return memcmp(data,
                  GetPixelAddress(expected_pixels_, x, y),
                  width * bytes_per_pixel_) == 0;
  }

  // Helper to compute address of pixel in pack aligned data.
  const void* ComputePackAlignmentAddress(GLint x,
                                          GLint y,
                                          GLsizei width,
                                          const void* address) const {
    GLint unpadded_row_size = ComputeImageDataSize(width, 1);
    GLint two_rows_size = ComputeImageDataSize(width, 2);
    GLsizei padded_row_size = two_rows_size - unpadded_row_size;
    GLint offset = y * padded_row_size + x * bytes_per_pixel_;
    return static_cast<const int8_t*>(address) + offset;
  }

  GLint ComputeImageDataSize(GLint width, GLint height) const {
    GLint row_size = width * bytes_per_pixel_;
    if (height > 1) {
      GLint temp = row_size + pack_alignment_ - 1;
      GLint padded_row_size = (temp / pack_alignment_) * pack_alignment_;
      GLint size_of_all_but_last_row = (height - 1) * padded_row_size;
      return size_of_all_but_last_row + row_size;
    } else {
      return height * row_size;
    }
  }

 private:
  const int8_t* GetPixelAddress(const int8_t* base, GLint x, GLint y) const {
    return base + (width_ * y + x) * bytes_per_pixel_;
  }

  GLsizei width_;
  GLsizei height_;
  GLint pack_alignment_;
  GLint bytes_per_pixel_;
  raw_ptr<const int8_t> src_pixels_;
  raw_ptr<const int8_t> expected_pixels_;
};

}  // anonymous namespace

void GLES2DecoderTest::CheckReadPixelsOutOfRange(GLint in_read_x,
                                                 GLint in_read_y,
                                                 GLsizei in_read_width,
                                                 GLsizei in_read_height,
                                                 bool init) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  const GLint kPackAlignment = 4;
  const GLenum kFormat = GL_RGBA;
  static const uint8_t kSrcPixels[kWidth * kHeight * kBytesPerPixel] = {
      12, 13, 14, 255, 18, 19, 18, 255, 19, 12, 13, 255, 14, 18, 19, 255,
      18, 19, 13, 255, 29, 28, 23, 255, 22, 21, 22, 255, 21, 29, 28, 255,
      23, 22, 21, 255, 22, 21, 28, 255, 31, 34, 39, 255, 37, 32, 37, 255,
      32, 31, 34, 255, 39, 37, 32, 255, 37, 32, 34, 255};

  ClearSharedMemory();

  // We need to setup an FBO so we can know the max size that ReadPixels will
  // access
  if (init) {
    DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
    DoTexImage2D(GL_TEXTURE_2D, 0, kFormat, kWidth, kHeight, 0, kFormat,
                 GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset);
    DoBindFramebuffer(
        GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
    DoFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           client_texture_id_,
                           kServiceTextureId,
                           0,
                           GL_NO_ERROR);
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
  }

  ReadPixelsEmulator emu(
      kWidth, kHeight, kBytesPerPixel, kSrcPixels, kSrcPixels, kPackAlignment);
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  void* dest = &result[1];

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  // ReadPixels will be called for valid size only even though the command
  // is requesting a larger size.
  GLint read_x = std::max(0, in_read_x);
  GLint read_y = std::max(0, in_read_y);
  GLint read_end_x = std::clamp(in_read_x + in_read_width, 0, kWidth);
  GLint read_end_y = std::clamp(in_read_y + in_read_height, 0, kHeight);
  GLint read_width = read_end_x - read_x;
  GLint read_height = read_end_y - read_y;
  if (read_width > 0 && read_height > 0) {
    for (GLint yy = read_y; yy < read_end_y; ++yy) {
      EXPECT_CALL(
          *gl_,
          ReadPixels(read_x, yy, read_width, 1, kFormat, GL_UNSIGNED_BYTE, _))
          .WillOnce(Invoke(&emu, &ReadPixelsEmulator::ReadPixels))
          .RetiresOnSaturation();
    }
  }
  cmds::ReadPixels cmd;
  cmd.Init(in_read_x,
           in_read_y,
           in_read_width,
           in_read_height,
           kFormat,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  GLint unpadded_row_size = emu.ComputeImageDataSize(in_read_width, 1);
  auto zero = base::HeapArray<int8_t>::Uninit(unpadded_row_size);
  auto pack = base::HeapArray<int8_t>::Uninit(kPackAlignment);
  memset(zero.data(), kInitialMemoryValue, unpadded_row_size);
  memset(pack.data(), kInitialMemoryValue, kPackAlignment);
  for (GLint yy = 0; yy < in_read_height; ++yy) {
    const int8_t* row = static_cast<const int8_t*>(
        emu.ComputePackAlignmentAddress(0, yy, in_read_width, dest));
    GLint y = in_read_y + yy;
    if (y < 0 || y >= kHeight) {
      EXPECT_EQ(0, memcmp(zero.data(), row, unpadded_row_size));
    } else {
      // check off left.
      GLint num_left_pixels = std::max(-in_read_x, 0);
      GLint num_left_bytes = num_left_pixels * kBytesPerPixel;
      EXPECT_EQ(0, memcmp(zero.data(), row, num_left_bytes));

      // check off right.
      GLint num_right_pixels = std::max(in_read_x + in_read_width - kWidth, 0);
      GLint num_right_bytes = num_right_pixels * kBytesPerPixel;
      EXPECT_EQ(0,
                memcmp(zero.data(), row + unpadded_row_size - num_right_bytes,
                       num_right_bytes));

      // check middle.
      GLint x = std::max(in_read_x, 0);
      GLint num_middle_pixels =
          std::max(in_read_width - num_left_pixels - num_right_pixels, 0);
      EXPECT_TRUE(
          emu.CompareRowSegment(x, y, num_middle_pixels, row + num_left_bytes));
    }

    // check padding
    if (yy != in_read_height - 1) {
      GLint temp = unpadded_row_size + kPackAlignment - 1;
      GLint padded_row_size = (temp / kPackAlignment ) * kPackAlignment;
      GLint num_padding_bytes = padded_row_size - unpadded_row_size;
      if (num_padding_bytes) {
        EXPECT_EQ(
            0, memcmp(pack.data(), row + unpadded_row_size, num_padding_bytes));
      }
    }
  }
}

TEST_P(GLES2DecoderTest, ReadPixels) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  const GLint kPackAlignment = 4;
  static const uint8_t kSrcPixels[kWidth * kHeight * kBytesPerPixel] = {
      12, 13, 14, 255, 18, 19, 18, 255, 19, 12, 13, 255, 14, 18, 19, 255,
      18, 19, 13, 255, 29, 28, 23, 255, 22, 21, 22, 255, 21, 29, 28, 255,
      23, 22, 21, 255, 22, 21, 28, 255, 31, 34, 39, 255, 37, 32, 37, 255,
      32, 31, 34, 255, 39, 37, 32, 255, 37, 32, 34, 255};

  surface_->SetSize(gfx::Size(INT_MAX, INT_MAX));

  ReadPixelsEmulator emu(
      kWidth, kHeight, kBytesPerPixel, kSrcPixels, kSrcPixels, kPackAlignment);
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  void* dest = &result[1];
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              ReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, _))
      .WillOnce(Invoke(&emu, &ReadPixelsEmulator::ReadPixels));
  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           kWidth,
           kHeight,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 1;
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  for (GLint yy = 0; yy < kHeight; ++yy) {
    EXPECT_TRUE(emu.CompareRowSegment(
        0, yy, kWidth, emu.ComputePackAlignmentAddress(0, yy, kWidth, dest)));
  }
}

TEST_P(GLES3DecoderTest, ReadPixels2PixelPackBufferNoBufferBound) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  EXPECT_CALL(*gl_, ReadPixels(_, _, _, _, _, _, _)).Times(0);

  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           kWidth,
           kHeight,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           0,
           0,
           0,
           0,
           false);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, ReadPixelsBufferBound) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  GLint size = kWidth * kHeight * kBytesPerPixel;
  EXPECT_CALL(*gl_, ReadPixels(_, _, _, _, _, _, _)).Times(0);
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size);

  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           kWidth,
           kHeight,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, ReadPixels2PixelPackBuffer) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  GLint size = kWidth * kHeight * kBytesPerPixel;

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              ReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, _));
  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           kWidth,
           kHeight,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           0,
           0,
           0,
           0,
           false);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, ReadPixelsPixelPackBufferMapped) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  GLint size = kWidth * kHeight * kBytesPerPixel;

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size);

  std::vector<int8_t> mapped_data(size);

  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  // uint32_t is Result for both MapBufferRange and UnmapBuffer commands.
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(uint32_t);
  EXPECT_CALL(*gl_,
              MapBufferRange(GL_PIXEL_PACK_BUFFER, 0, size, GL_MAP_READ_BIT))
        .WillOnce(Return(mapped_data.data()))
        .RetiresOnSaturation();
  cmds::MapBufferRange map_buffer_range;
  map_buffer_range.Init(GL_PIXEL_PACK_BUFFER, 0, size, GL_MAP_READ_BIT,
                        data_shm_id, data_shm_offset,
                        result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(map_buffer_range));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, ReadPixels(_, _, _, _, _, _, _)).Times(0);
  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           kWidth,
           kHeight,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           0,
           0,
           0,
           0,
           false);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, ReadPixelsPixelPackBufferIsNotLargeEnough) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  GLint size = kWidth * kHeight * kBytesPerPixel;

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);

  DoBufferData(GL_PIXEL_PACK_BUFFER, size - 4);
  EXPECT_CALL(*gl_, ReadPixels(_, _, _, _, _, _, _)).Times(0);

  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           kWidth,
           kHeight,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           0,
           0,
           0,
           0,
           false);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, ReadPixels2RowLengthWorkaround) {
  gpu::GpuDriverBugWorkarounds workarounds;
  workarounds.pack_parameters_workaround_with_pack_buffer = true;
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoderWithWorkarounds(init, workarounds);

  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kRowLength = 4;
  GLint size = (kRowLength * (kHeight - 1) + kWidth) * kBytesPerPixel;

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size);

  DoPixelStorei(GL_PACK_ROW_LENGTH, kRowLength);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  for (GLint ii = 0; ii < kHeight; ++ii) {
    if (ii + 1 == kHeight) {
      EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ROW_LENGTH, kWidth))
          .Times(1)
          .RetiresOnSaturation();
    }
    void* offset = reinterpret_cast<void*>(ii * kRowLength * kBytesPerPixel);
    EXPECT_CALL(*gl_, ReadPixels(0, ii, kWidth, 1, kFormat, kType, offset))
        .Times(1)
        .RetiresOnSaturation();
    if (ii + 1 == kHeight) {
      EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ROW_LENGTH, kRowLength))
          .Times(1)
          .RetiresOnSaturation();
    }
  }

  cmds::ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight,
           kFormat, kType,
           0, 0, 0, 0,
           false);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, ReadPixels2AlignmentWorkaround) {
  gpu::GpuDriverBugWorkarounds workarounds;
  workarounds.pack_parameters_workaround_with_pack_buffer = true;
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoderWithWorkarounds(init, workarounds);

  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kAlignment = 8;
  const GLint kPadding = 4;
  GLint size = kWidth * kBytesPerPixel * kHeight + kPadding * (kHeight - 1);

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size);

  DoPixelStorei(GL_PACK_ALIGNMENT, kAlignment);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  uint8_t* offset = reinterpret_cast<uint8_t*>(0);
  EXPECT_CALL(*gl_,
              ReadPixels(0, 0, kWidth, kHeight - 1, kFormat, kType, offset))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ALIGNMENT, 1))
      .Times(1)
      .RetiresOnSaturation();
  offset += (kWidth * kBytesPerPixel + kPadding) * (kHeight - 1);
  EXPECT_CALL(*gl_,
              ReadPixels(0, kHeight - 1, kWidth, 1, kFormat, kType, offset))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ALIGNMENT, kAlignment))
      .Times(1)
      .RetiresOnSaturation();

  cmds::ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight,
           kFormat, kType,
           0, 0, 0, 0,
           false);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest,
       ReadPixels2RowLengthAndAlignmentWorkarounds) {
  gpu::GpuDriverBugWorkarounds workarounds;
  workarounds.pack_parameters_workaround_with_pack_buffer = true;
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoderWithWorkarounds(init, workarounds);

  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const GLint kAlignment = 8;
  const GLint kRowLength = 3;
  const GLint kPadding = 4;
  GLint padded_row_size = kRowLength * kBytesPerPixel + kPadding;
  GLint unpadded_row_size = kWidth * kBytesPerPixel;
  GLint size = padded_row_size * (kHeight - 1) + unpadded_row_size;

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size);

  DoPixelStorei(GL_PACK_ALIGNMENT, kAlignment);
  DoPixelStorei(GL_PACK_ROW_LENGTH, kRowLength);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  for (GLint ii = 0; ii < kHeight - 1; ++ii) {
    void* offset = reinterpret_cast<void*>(ii * padded_row_size);
    EXPECT_CALL(*gl_, ReadPixels(0, ii, kWidth, 1, kFormat, kType, offset))
        .Times(1)
        .RetiresOnSaturation();
  }
  EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ALIGNMENT, 1))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ROW_LENGTH, kWidth))
      .Times(1)
      .RetiresOnSaturation();
  void* offset = reinterpret_cast<void*>((kHeight - 1) * padded_row_size);
  EXPECT_CALL(*gl_,
              ReadPixels(0, kHeight - 1, kWidth, 1, kFormat, kType, offset))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ALIGNMENT, kAlignment))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ROW_LENGTH, kRowLength))
      .Times(1)
      .RetiresOnSaturation();

  cmds::ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight,
           kFormat, kType,
           0, 0, 0, 0,
           false);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderRGBBackbufferTest, ReadPixelsNoAlphaBackbuffer) {
  const GLsizei kWidth = 3;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  const GLint kPackAlignment = 4;
  static const uint8_t kSrcPixels[kWidth * kHeight * kBytesPerPixel] = {
      12, 13, 14, 18, 19, 18, 19, 12, 13, 14, 18, 19, 29, 28, 23, 22, 21, 22,
      21, 29, 28, 23, 22, 21, 31, 34, 39, 37, 32, 37, 32, 31, 34, 39, 37, 32,
  };

  surface_->SetSize(gfx::Size(INT_MAX, INT_MAX));

  ReadPixelsEmulator emu(kWidth, kHeight, kBytesPerPixel, kSrcPixels,
                         kSrcPixels, kPackAlignment);
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  void* dest = &result[1];
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              ReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, _))
      .WillOnce(Invoke(&emu, &ReadPixelsEmulator::ReadPixels));
  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           kWidth,
           kHeight,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  for (GLint yy = 0; yy < kHeight; ++yy) {
    EXPECT_TRUE(emu.CompareRowSegment(
        0, yy, kWidth, emu.ComputePackAlignmentAddress(0, yy, kWidth, dest)));
  }
}

TEST_P(GLES2DecoderTest, ReadPixelsOutOfRange) {
  static GLint tests[][4] = {
      {
       -2, -1, 9, 5,
      },  // out of range on all sides
      {
       2, 1, 9, 5,
      },  // out of range on right, bottom
      {
       -7, -4, 9, 5,
      },  // out of range on left, top
      {
       0, -5, 9, 5,
      },  // completely off top
      {
       0, 3, 9, 5,
      },  // completely off bottom
      {
       -9, 0, 9, 5,
      },  // completely off left
      {
       5, 0, 9, 5,
      },  // completely off right
  };

  for (size_t tt = 0; tt < std::size(tests); ++tt) {
    CheckReadPixelsOutOfRange(
        tests[tt][0], tests[tt][1], tests[tt][2], tests[tt][3], tt == 0);
  }
}

TEST_P(GLES2DecoderTest, ReadPixelsInvalidArgs) {
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  EXPECT_CALL(*gl_, ReadPixels(_, _, _, _, _, _, _)).Times(0);
  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           -1,
           1,
           GL_RGB,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(0,
           0,
           1,
           -1,
           GL_RGB,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(0,
           0,
           1,
           1,
           GL_RGB,
           GL_INT,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(0,
           0,
           1,
           1,
           GL_RGB,
           GL_UNSIGNED_BYTE,
           kInvalidSharedMemoryId,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(0,
           0,
           1,
           1,
           GL_RGB,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           kInvalidSharedMemoryOffset,
           result_shm_id,
           result_shm_offset,
           false);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(0,
           0,
           1,
           1,
           GL_RGB,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           kInvalidSharedMemoryId,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(0,
           0,
           1,
           1,
           GL_RGB,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           kInvalidSharedMemoryOffset,
           false);
  result->success = 0;
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderManualInitTest, ReadPixelsAsyncError) {
  InitState init;
  init.extensions = "GL_ARB_sync";
  init.gl_version = "OpenGL ES 3.0";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);

  EXPECT_CALL(*gl_, GetError())
      // first error check must pass to get to the test
      .WillOnce(Return(GL_NO_ERROR))
      // second check is after BufferData, simulate fail here
      .WillOnce(Return(GL_INVALID_OPERATION))
      // third error check is fall-through call to sync ReadPixels
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_,
              ReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, _))
      .Times(1);
  EXPECT_CALL(*gl_, GenBuffersARB(1, _)).Times(1);
  EXPECT_CALL(*gl_, DeleteBuffersARB(1, _)).Times(1);
  EXPECT_CALL(*gl_, BindBuffer(GL_PIXEL_PACK_BUFFER_ARB, _)).Times(2);
  EXPECT_CALL(*gl_,
              BufferData(GL_PIXEL_PACK_BUFFER_ARB, _, nullptr, GL_STREAM_READ))
      .Times(1);

  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           kWidth,
           kHeight,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           true);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

class GLES2ReadPixelsAsyncTest : public GLES2DecoderManualInitTest {
 public:
  void SetUp() override {
    InitState init;
    init.extensions = "GL_ARB_sync";
    init.gl_version = "OpenGL ES 3.0";
    init.has_alpha = true;
    init.request_alpha = true;
    init.bind_generates_resource = true;
    InitDecoder(init);
  }

  void SetupReadPixelsAsyncExpectation(GLsizei width, GLsizei height) {
    const size_t kBufferSize = width * height * 4;
    EXPECT_CALL(*gl_, GetError())
        .WillRepeatedly(Return(GL_NO_ERROR))
        .RetiresOnSaturation();

    EXPECT_CALL(*gl_,
                ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, _))
        .Times(1);
    EXPECT_CALL(*gl_, GenBuffersARB(1, _))
        .WillOnce(SetArgPointee<1>(kServiceBufferId))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindBuffer(GL_PIXEL_PACK_BUFFER_ARB, kServiceBufferId))
        .Times(1);
    EXPECT_CALL(*gl_, BindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0)).Times(1);
    EXPECT_CALL(*gl_, BufferData(GL_PIXEL_PACK_BUFFER_ARB, kBufferSize, nullptr,
                                 GL_STREAM_READ))
        .Times(1);
    GLsync sync = reinterpret_cast<GLsync>(kServiceSyncId);
    EXPECT_CALL(*gl_, FenceSync(0x9117, 0)).WillOnce(Return(sync));
    EXPECT_CALL(*gl_, IsSync(sync)).WillRepeatedly(Return(GL_TRUE));
    EXPECT_CALL(*gl_, Flush()).Times(1);
  }

  void FinishReadPixelsAndCheckResult(GLsizei width,
                                      GLsizei height,
                                      char* pixels) {
    EXPECT_TRUE(decoder_->HasMoreIdleWork());

    const size_t kBufferSize = width * height * 4;
    auto buffer = std::make_unique<char[]>(kBufferSize);
    for (size_t i = 0; i < kBufferSize; ++i)
      buffer[i] = i;

    GLsync sync = reinterpret_cast<GLsync>(kServiceSyncId);
    EXPECT_CALL(*gl_, ClientWaitSync(sync, 0, 0))
        .WillOnce(Return(GL_CONDITION_SATISFIED));
    EXPECT_CALL(*gl_, BindBuffer(GL_PIXEL_PACK_BUFFER_ARB, kServiceBufferId))
        .Times(1);
    EXPECT_CALL(*gl_, MapBufferRange(GL_PIXEL_PACK_BUFFER_ARB, 0, kBufferSize,
                                     GL_MAP_READ_BIT))
        .WillOnce(Return(buffer.get()))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, UnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB)).Times(1);
    EXPECT_CALL(*gl_, BindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0)).Times(1);
    EXPECT_CALL(*gl_, DeleteBuffersARB(1, _)).Times(1);
    EXPECT_CALL(*gl_, DeleteSync(sync)).Times(1);
    decoder_->PerformIdleWork();
    EXPECT_FALSE(decoder_->HasMoreIdleWork());
    EXPECT_EQ(0, memcmp(pixels, buffer.get(), kBufferSize));
  }
};

TEST_P(GLES2ReadPixelsAsyncTest, ReadPixelsAsync) {
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  char* pixels = reinterpret_cast<char*>(result + 1);

  SetupReadPixelsAsyncExpectation(kWidth, kHeight);

  cmds::ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels_shm_id,
           pixels_shm_offset, result_shm_id, result_shm_offset, true);
  result->success = 0;

  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_TRUE(decoder_->HasMoreIdleWork());

  GLsync sync = reinterpret_cast<GLsync>(kServiceSyncId);
  EXPECT_CALL(*gl_, ClientWaitSync(sync, 0, 0))
      .WillOnce(Return(GL_TIMEOUT_EXPIRED));
  decoder_->PerformIdleWork();

  FinishReadPixelsAndCheckResult(kWidth, kHeight, pixels);
}

TEST_P(GLES2ReadPixelsAsyncTest, ReadPixelsAsyncModifyCommand) {
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  char* pixels = reinterpret_cast<char*>(result + 1);

  SetupReadPixelsAsyncExpectation(kWidth, kHeight);

  cmds::ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels_shm_id,
           pixels_shm_offset, result_shm_id, result_shm_offset, true);
  result->success = 0;

  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_TRUE(decoder_->HasMoreIdleWork());

  // Change command after ReadPixels issued, but before we finish the read,
  // should have no impact.
  cmd.Init(1, 2, 6, 7, GL_RGB, GL_UNSIGNED_SHORT, pixels_shm_id,
           pixels_shm_offset, result_shm_id, result_shm_offset, false);

  FinishReadPixelsAndCheckResult(kWidth, kHeight, pixels);
}

TEST_P(GLES2ReadPixelsAsyncTest, ReadPixelsAsyncChangePackAlignment) {
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  const GLsizei kWidth = 1;
  const GLsizei kHeight = 4;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  char* pixels = reinterpret_cast<char*>(result + 1);

  SetupReadPixelsAsyncExpectation(kWidth, kHeight);

  {
    cmds::ReadPixels cmd;
    cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels_shm_id,
             pixels_shm_offset, result_shm_id, result_shm_offset, true);
    result->success = 0;

    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  EXPECT_TRUE(decoder_->HasMoreIdleWork());

  // Changing the pack alignment after the ReadPixels is issued but before we
  // finish the read should have no impact.
  {
    cmds::PixelStorei cmd;
    cmd.Init(GL_PACK_ALIGNMENT, 8);

    EXPECT_CALL(*gl_, PixelStorei(GL_PACK_ALIGNMENT, 8)).Times(1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  FinishReadPixelsAndCheckResult(kWidth, kHeight, pixels);
}

INSTANTIATE_TEST_SUITE_P(Service, GLES2ReadPixelsAsyncTest, ::testing::Bool());

// Check that if a renderbuffer is attached and GL returns
// GL_FRAMEBUFFER_COMPLETE that the buffer is cleared and state is restored.
TEST_P(GLES2DecoderTest, FramebufferRenderbufferClearColor) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                     kServiceRenderbufferId);
  cmds::ClearColor color_cmd;
  cmds::ColorMask color_mask_cmd;
  cmds::Enable enable_cmd;
  cmds::FramebufferRenderbuffer cmd;
  color_cmd.Init(0.1f, 0.2f, 0.3f, 0.4f);
  color_mask_cmd.Init(0, 1, 0, 1);
  enable_cmd.Init(GL_SCISSOR_TEST);
  cmd.Init(GL_FRAMEBUFFER,
           GL_COLOR_ATTACHMENT0,
           GL_RENDERBUFFER,
           client_renderbuffer_id_);
  InSequence sequence;
  EXPECT_CALL(*gl_, ClearColor(0.1f, 0.2f, 0.3f, 0.4f))
      .Times(1)
      .RetiresOnSaturation();
  SetupExpectationsForEnableDisable(GL_SCISSOR_TEST, true);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              FramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                                         GL_COLOR_ATTACHMENT0,
                                         GL_RENDERBUFFER,
                                         kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(color_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(color_mask_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(enable_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, FramebufferRenderbufferClearDepth) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                     kServiceRenderbufferId);
  cmds::ClearDepthf depth_cmd;
  cmds::DepthMask depth_mask_cmd;
  cmds::FramebufferRenderbuffer cmd;
  depth_cmd.Init(0.5f);
  depth_mask_cmd.Init(false);
  cmd.Init(GL_FRAMEBUFFER,
           GL_DEPTH_ATTACHMENT,
           GL_RENDERBUFFER,
           client_renderbuffer_id_);
  InSequence sequence;
  EXPECT_CALL(*gl_, ClearDepth(0.5f)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              FramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                                         GL_DEPTH_ATTACHMENT,
                                         GL_RENDERBUFFER,
                                         kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(depth_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(depth_mask_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, FramebufferRenderbufferClearStencil) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                     kServiceRenderbufferId);
  cmds::ClearStencil stencil_cmd;
  cmds::StencilMaskSeparate stencil_mask_separate_cmd;
  cmds::FramebufferRenderbuffer cmd;
  stencil_cmd.Init(123);
  stencil_mask_separate_cmd.Init(GL_BACK, 0x1234u);
  cmd.Init(GL_FRAMEBUFFER,
           GL_STENCIL_ATTACHMENT,
           GL_RENDERBUFFER,
           client_renderbuffer_id_);
  InSequence sequence;
  EXPECT_CALL(*gl_, ClearStencil(123)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              FramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                                         GL_STENCIL_ATTACHMENT,
                                         GL_RENDERBUFFER,
                                         kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(stencil_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(stencil_mask_separate_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest, FramebufferRenderbufferClearDepthStencil) {
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                     kServiceRenderbufferId);
  cmds::ClearDepthf depth_cmd;
  cmds::ClearStencil stencil_cmd;
  cmds::FramebufferRenderbuffer cmd;
  depth_cmd.Init(0.5f);
  stencil_cmd.Init(123);
  cmd.Init(
      GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_);
  InSequence sequence;
  EXPECT_CALL(*gl_, ClearDepth(0.5f))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ClearStencil(123))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, FramebufferRenderbufferEXT(
      GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(depth_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(stencil_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  Framebuffer* framebuffer = GetFramebuffer(client_framebuffer_id_);
  ASSERT_TRUE(framebuffer);
  ASSERT_FALSE(framebuffer->GetAttachment(GL_DEPTH_STENCIL_ATTACHMENT));
  ASSERT_TRUE(framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT));
  ASSERT_TRUE(framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT));
}

TEST_P(GLES2DecoderManualInitTest, ActualAlphaMatchesRequestedAlpha) {
  InitState init;
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  auto* result =
      static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetIntegerv cmd2;
  cmd2.Init(GL_ALPHA_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_ALPHA_BITS),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(8, result->GetData()[0]);
}

TEST_P(GLES2DecoderManualInitTest, PackedDepthStencilReportsCorrectValues) {
  InitState init;
  init.extensions = "GL_OES_packed_depth_stencil";
  init.gl_version = "OpenGL ES 2.0";
  init.has_depth = true;
  init.has_stencil = true;
  init.request_depth = true;
  init.request_stencil = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  auto* result =
      static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgPointee<1>(8))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_BITS),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(8, result->GetData()[0]);
  result->size = 0;
  cmd2.Init(GL_DEPTH_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgPointee<1>(24))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_BITS),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(24, result->GetData()[0]);
}

TEST_P(GLES2DecoderManualInitTest, PackedDepthStencilRenderbufferDepth) {
  InitState init;
  init.extensions = "GL_OES_packed_depth_stencil";
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);

  EnsureRenderbufferBound(false);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))  // for RenderbufferStoage
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for FramebufferRenderbuffer
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for GetIntegerv
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for GetIntegerv
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  EXPECT_CALL(
      *gl_,
      RenderbufferStorageEXT(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 100, 50))
      .Times(1)
      .RetiresOnSaturation();
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 100, 50);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_CALL(*gl_,
              FramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                                         GL_DEPTH_ATTACHMENT,
                                         GL_RENDERBUFFER,
                                         kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  cmds::FramebufferRenderbuffer fbrb_cmd;
  fbrb_cmd.Init(GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER,
                client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbrb_cmd));

  auto* result =
      static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgPointee<1>(8))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_BITS),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0, result->GetData()[0]);
  result->size = 0;
  cmd2.Init(GL_DEPTH_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgPointee<1>(24))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_BITS),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(24, result->GetData()[0]);
}

TEST_P(GLES2DecoderManualInitTest, PackedDepthStencilRenderbufferStencil) {
  InitState init;
  init.extensions = "GL_OES_packed_depth_stencil";
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);

  EnsureRenderbufferBound(false);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))  // for RenderbufferStoage
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for FramebufferRenderbuffer
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for GetIntegerv
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))  // for GetIntegerv
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  EXPECT_CALL(
      *gl_,
      RenderbufferStorageEXT(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 100, 50))
      .Times(1)
      .RetiresOnSaturation();
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 100, 50);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_CALL(*gl_,
              FramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                                         GL_STENCIL_ATTACHMENT,
                                         GL_RENDERBUFFER,
                                         kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  cmds::FramebufferRenderbuffer fbrb_cmd;
  fbrb_cmd.Init(GL_FRAMEBUFFER,
                GL_STENCIL_ATTACHMENT,
                GL_RENDERBUFFER,
                client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbrb_cmd));

  auto* result =
      static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetIntegerv cmd2;
  cmd2.Init(GL_STENCIL_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_STENCIL_BITS, _))
      .WillOnce(SetArgPointee<1>(8))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_STENCIL_BITS),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(8, result->GetData()[0]);
  result->size = 0;
  cmd2.Init(GL_DEPTH_BITS, shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_, GetIntegerv(GL_DEPTH_BITS, _))
      .WillOnce(SetArgPointee<1>(24))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_DEPTH_BITS),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0, result->GetData()[0]);
}

TEST_P(GLES2DecoderTest, FramebufferRenderbufferGLError) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  cmds::FramebufferRenderbuffer cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
           client_renderbuffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                     kServiceRenderbufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              FramebufferRenderbufferEXT(GL_FRAMEBUFFER,
                                         GL_COLOR_ATTACHMENT0,
                                         GL_RENDERBUFFER,
                                         kServiceRenderbufferId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_P(GLES2DecoderTest, FramebufferTexture2DGLError) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLenum kFormat = GL_RGB;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D,
               0,
               kFormat,
               kWidth,
               kHeight,
               0,
               kFormat,
               GL_UNSIGNED_BYTE,
               0,
               0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              FramebufferTexture2DEXT(GL_FRAMEBUFFER,
                                      GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D,
                                      kServiceTextureId,
                                      0))
      .Times(1)
      .RetiresOnSaturation();
  cmds::FramebufferTexture2D fbtex_cmd;
  fbtex_cmd.Init(GL_FRAMEBUFFER,
                 GL_COLOR_ATTACHMENT0,
                 GL_TEXTURE_2D,
                 client_texture_id_,
                 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbtex_cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_P(GLES2DecoderTest, RenderbufferStorageGLError) {
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  EnsureRenderbufferBound(false);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(GL_RENDERBUFFER, GL_RGBA4, 100, 50))
      .Times(1)
      .RetiresOnSaturation();
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 100, 50);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_P(GLES2DecoderTest, RenderbufferStorageBadArgs) {
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, TestHelper::kMaxRenderbufferSize + 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 1, TestHelper::kMaxRenderbufferSize + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderTest, ClearBufferivImmediateValidArgs) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8I, 1, 1, GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);

  // TODO(zmo): Set up expectations for the path where the attachment isn't
  // marked as cleared.
  Framebuffer* framebuffer = GetFramebuffer(client_framebuffer_id_);
  framebuffer->MarkAttachmentAsCleared(
      group().renderbuffer_manager(), nullptr, GL_COLOR_ATTACHMENT0, true);

  auto& cmd = *GetImmediateAs<cmds::ClearBufferivImmediate>();
  GLint temp[4] = { 0 };
  cmd.Init(GL_COLOR, 0, &temp[0]);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  SetupExpectationsForApplyingDirtyState(
      false, false, false, 0x1111, false, false, 0, 0, false);
  EXPECT_CALL(*gl_, ClearBufferiv(GL_COLOR, 0, PointsToArray(temp, 4)));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, ClearBufferuivImmediateValidArgs) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8UI, 1, 1, GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);

  // TODO(zmo): Set up expectations for the path where the attachment isn't
  // marked as cleared.
  Framebuffer* framebuffer = GetFramebuffer(client_framebuffer_id_);
  framebuffer->MarkAttachmentAsCleared(
      group().renderbuffer_manager(), nullptr, GL_COLOR_ATTACHMENT0, true);

  auto& cmd = *GetImmediateAs<cmds::ClearBufferuivImmediate>();
  GLuint temp[4] = { 0u };
  cmd.Init(GL_COLOR, 0, &temp[0]);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  SetupExpectationsForApplyingDirtyState(
      false, false, false, 0x1111, false, false, 0, 0, false);
  EXPECT_CALL(*gl_, ClearBufferuiv(GL_COLOR, 0, PointsToArray(temp, 4)));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, ClearBufferfvImmediateValidArgs) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, 1, 1,
                        GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);

  // TODO(zmo): Set up expectations for the path where the attachment isn't
  // marked as cleared.
  Framebuffer* framebuffer = GetFramebuffer(client_framebuffer_id_);
  framebuffer->MarkAttachmentAsCleared(
      group().renderbuffer_manager(), nullptr, GL_DEPTH_ATTACHMENT, true);

  cmds::Enable cmd_enable;
  cmd_enable.Init(GL_DEPTH_TEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd_enable));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  auto& cmd = *GetImmediateAs<cmds::ClearBufferfvImmediate>();
  GLfloat temp[4] = { 1.0f };
  cmd.Init(GL_DEPTH, 0, &temp[0]);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  SetupExpectationsForApplyingDirtyState(
      true, true, false, 0x1110, true, true, 0, 0, false);
  EXPECT_CALL(*gl_, ClearBufferfv(GL_DEPTH, 0, PointsToArray(temp, 4)));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, ClearBufferfiValidArgs) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1,
                        GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);

  // TODO(zmo): Set up expectations for the path where the attachment isn't
  // marked as cleared.
  Framebuffer* framebuffer = GetFramebuffer(client_framebuffer_id_);
  framebuffer->MarkAttachmentAsCleared(group().renderbuffer_manager(), nullptr,
                                       GL_DEPTH_ATTACHMENT, true);
  framebuffer->MarkAttachmentAsCleared(group().renderbuffer_manager(), nullptr,
                                       GL_STENCIL_ATTACHMENT, true);

  cmds::Enable cmd_enable;
  cmd_enable.Init(GL_STENCIL_TEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd_enable));
  cmd_enable.Init(GL_DEPTH_TEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd_enable));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::ClearBufferfi cmd;
  cmd.Init(GL_DEPTH_STENCIL, 0, 1.0f, 0);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  SetupExpectationsForApplyingDirtyState(
      true, true, true, 0x1110, true, true,
      GLES2Decoder::kDefaultStencilMask, GLES2Decoder::kDefaultStencilMask,
      true);
  EXPECT_CALL(*gl_, ClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest,
       RenderbufferStorageMultisampleCHROMIUMGLError) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  EnsureRenderbufferBound(false);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  SetupExpectationsForInternalFormatSampleCountsHelper(
      GL_RENDERBUFFER, GL_RGBA4, 1, TestHelper::kMaxSamples);
  EXPECT_CALL(*gl_, RenderbufferStorageMultisample(GL_RENDERBUFFER, 1, GL_RGBA4,
                                                   100, 50))
      .Times(1)
      .RetiresOnSaturation();
  cmds::RenderbufferStorageMultisampleCHROMIUM cmd;
  cmd.Init(GL_RENDERBUFFER, 1, GL_RGBA4, 100, 50);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest,
       RenderbufferStorageMultisampleCHROMIUMBadArgs) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  EXPECT_CALL(*gl_, RenderbufferStorageMultisample(_, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  cmds::RenderbufferStorageMultisampleCHROMIUM cmd;
  cmd.Init(GL_RENDERBUFFER,
           TestHelper::kMaxSamples + 1,
           GL_RGBA4,
           TestHelper::kMaxRenderbufferSize,
           1);
  SetupExpectationsForInternalFormatSampleCountsHelper(
      GL_RENDERBUFFER, GL_RGBA4, 1, TestHelper::kMaxSamples);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  cmd.Init(GL_RENDERBUFFER,
           TestHelper::kMaxSamples,
           GL_RGBA4,
           TestHelper::kMaxRenderbufferSize + 1,
           1);
  SetupExpectationsForInternalFormatSampleCountsHelper(
      GL_RENDERBUFFER, GL_RGBA4, 1, TestHelper::kMaxSamples);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  cmd.Init(GL_RENDERBUFFER,
           TestHelper::kMaxSamples,
           GL_RGBA4,
           1,
           TestHelper::kMaxRenderbufferSize + 1);
  SetupExpectationsForInternalFormatSampleCountsHelper(
      GL_RENDERBUFFER, GL_RGBA4, 1, TestHelper::kMaxSamples);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, RenderbufferStorageMultisampleCHROMIUM) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  InitDecoder(init);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  InSequence sequence;
  DoRenderbufferStorageMultisampleCHROMIUM(
      GL_RENDERBUFFER, TestHelper::kMaxSamples, GL_RGBA4,
      TestHelper::kMaxRenderbufferSize, 1, false);
}

TEST_P(GLES2DecoderManualInitTest,
       RenderbufferStorageMultisampleCHROMIUMRebindRenderbuffer) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  InitDecoder(init);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  RestoreRenderbufferBindings();
  InSequence sequence;
  DoRenderbufferStorageMultisampleCHROMIUM(
      GL_RENDERBUFFER, TestHelper::kMaxSamples, GL_RGBA4,
      TestHelper::kMaxRenderbufferSize, 1, true);
}

TEST_P(GLES2DecoderManualInitTest,
       RenderbufferStorageMultisampleEXTNotSupported) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  InSequence sequence;
  // GL_CHROMIUM_framebuffer_multisample uses
  // RenderbufferStorageMultisampleCHROMIUM.
  cmds::RenderbufferStorageMultisampleEXT cmd;
  cmd.Init(GL_RENDERBUFFER,
           TestHelper::kMaxSamples,
           GL_RGBA4,
           TestHelper::kMaxRenderbufferSize,
           1);
  EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));
}

class GLES2DecoderMultisampledRenderToTextureTest
    : public GLES2DecoderTestWithExtensionsOnGLES2 {
 public:
  void TestNotCompatibleWithRenderbufferStorageMultisampleCHROMIUM() {
    DoBindRenderbuffer(
        GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
    cmds::RenderbufferStorageMultisampleCHROMIUM cmd;
    cmd.Init(GL_RENDERBUFFER,
             TestHelper::kMaxSamples,
             GL_RGBA4,
             TestHelper::kMaxRenderbufferSize,
             1);
    EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));
  }

  void TestRenderbufferStorageMultisampleEXT(const char* extension,
                                             bool rb_rebind) {
    DoBindRenderbuffer(
        GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
    InSequence sequence;

    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    if (rb_rebind) {
      RestoreRenderbufferBindings();
      EnsureRenderbufferBound(true);
    } else {
      EnsureRenderbufferBound(false);
    }
    EXPECT_CALL(*gl_, RenderbufferStorageMultisampleEXT(
                          GL_RENDERBUFFER, TestHelper::kMaxSamples, GL_RGBA4,
                          TestHelper::kMaxRenderbufferSize, 1))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    cmds::RenderbufferStorageMultisampleEXT cmd;
    cmd.Init(GL_RENDERBUFFER,
             TestHelper::kMaxSamples,
             GL_RGBA4,
             TestHelper::kMaxRenderbufferSize,
             1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderMultisampledRenderToTextureTest,
                         ::testing::Bool());

TEST_P(GLES2DecoderMultisampledRenderToTextureTest,
       NotCompatibleWithRenderbufferStorageMultisampleCHROMIUM_EXT) {
  Init("GL_EXT_multisampled_render_to_texture");
  TestNotCompatibleWithRenderbufferStorageMultisampleCHROMIUM();
}

TEST_P(GLES2DecoderMultisampledRenderToTextureTest,
       NotCompatibleWithRenderbufferStorageMultisampleCHROMIUM_IMG) {
  Init("GL_IMG_multisampled_render_to_texture");
  TestNotCompatibleWithRenderbufferStorageMultisampleCHROMIUM();
}

TEST_P(GLES2DecoderMultisampledRenderToTextureTest,
       RenderbufferStorageMultisampleEXT_EXT) {
  Init("GL_EXT_multisampled_render_to_texture");
  TestRenderbufferStorageMultisampleEXT("GL_EXT_multisampled_render_to_texture",
                                        false);
}

TEST_P(GLES2DecoderMultisampledRenderToTextureTest,
       RenderbufferStorageMultisampleEXT_IMG) {
  Init("GL_IMG_multisampled_render_to_texture");
  TestRenderbufferStorageMultisampleEXT("GL_IMG_multisampled_render_to_texture",
                                        false);
}

TEST_P(GLES2DecoderMultisampledRenderToTextureTest,
       RenderbufferStorageMultisampleEXT_EXT_RebindRenderbuffer) {
  Init("GL_EXT_multisampled_render_to_texture");
  TestRenderbufferStorageMultisampleEXT("GL_EXT_multisampled_render_to_texture",
                                        true);
}

TEST_P(GLES2DecoderMultisampledRenderToTextureTest,
       RenderbufferStorageMultisampleEXT_IMG_RebindRenderbuffer) {
  Init("GL_IMG_multisampled_render_to_texture");
  TestRenderbufferStorageMultisampleEXT("GL_IMG_multisampled_render_to_texture",
                                        true);
}

TEST_P(GLES2DecoderTest, ReadPixelsGLError) {
  GLenum kFormat = GL_RGBA;
  GLint x = 0;
  GLint y = 0;
  GLsizei width = 2;
  GLsizei height = 4;
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              ReadPixels(x, y, width, height, kFormat, GL_UNSIGNED_BYTE, _))
      .Times(1)
      .RetiresOnSaturation();
  cmds::ReadPixels cmd;
  cmd.Init(x,
           y,
           width,
           height,
           kFormat,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, UnClearedAttachmentsGetClearedOnClear) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  // Set scissor rect and enable GL_SCISSOR_TEST to make sure we re-enable it
  // and restore the rect again after the clear.
  DoEnableDisable(GL_SCISSOR_TEST, true);
  DoScissor(0, 0, 64, 64);

  // Setup "render from" texture.
  SetupTexture();

  SetupExpectationsForFramebufferClearing(GL_FRAMEBUFFER,       // target
                                          GL_COLOR_BUFFER_BIT,  // clear bits
                                          0, 0, 0,
                                          0,      // color
                                          0,      // stencil
                                          1.0f,   // depth
                                          true,  // scissor test
                                          0, 0, 64, 64);
  SetupExpectationsForApplyingDirtyState(false,    // Framebuffer is RGB
                                         false,    // Framebuffer has depth
                                         false,    // Framebuffer has stencil
                                         0x1111,   // color bits
                                         false,    // depth mask
                                         false,    // depth enabled
                                         0,        // front stencil mask
                                         0,        // back stencil mask
                                         false);   // stencil enabled

  EXPECT_CALL(*gl_, Clear(GL_COLOR_BUFFER_BIT)).Times(1).RetiresOnSaturation();

  cmds::Clear cmd;
  cmd.Init(GL_COLOR_BUFFER_BIT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, UnClearedAttachmentsGetClearedOnReadPixels) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  DoEnableDisable(GL_SCISSOR_TEST, false);
  DoScissor(0, 0, 1, 1);

  // Setup "render from" texture.
  SetupTexture();

  SetupExpectationsForFramebufferClearing(GL_FRAMEBUFFER,       // target
                                          GL_COLOR_BUFFER_BIT,  // clear bits
                                          0, 0, 0,
                                          0,      // color
                                          0,      // stencil
                                          1.0f,   // depth
                                          false,  // scissor test
                                          0, 0, 1, 1);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, _))
      .Times(1)
      .RetiresOnSaturation();
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           1,
           1,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexImage2DValidInternalFormat) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RG32I;
  GLenum format = GL_RG_INTEGER;
  GLenum type = GL_INT;
  GLsizei width = 16;
  GLsizei height = 8;
  GLint border = 0;

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, level, internal_format, width, height, 0, format,
               type, shared_memory_id_, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  target, level, internal_format, 0, 0, width, height, border))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderManualInitTest, CopyTexImage2DValidInternalFormat_FloatEXT) {
  InitState init;
  init.extensions = "GL_EXT_color_buffer_float";
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoder(init);

  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RG16F;
  GLenum format = GL_RGBA;
  GLenum type = GL_HALF_FLOAT;
  GLsizei width = 16;
  GLsizei height = 8;
  GLint border = 0;

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, level, GL_RGBA16F, width, height, 0, format, type,
               shared_memory_id_, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  target, level, internal_format, 0, 0, width, height, border))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderManualInitTest,
    CopyTexImage2DInvalidInternalFormat_FloatEXT) {
  InitState init;
  init.extensions = "GL_EXT_color_buffer_float";
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoder(init);

  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RG16F;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  GLsizei width = 16;
  GLsizei height = 8;
  GLint border = 0;

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, width, height, 0, format, type,
               shared_memory_id_, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  target, level, internal_format, 0, 0, width, height, border))
      .Times(0);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexImage2DInvalidInternalFormat) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RG_INTEGER;
  GLenum format = GL_RG;
  GLenum type = GL_UNSIGNED_BYTE;
  GLsizei width = 16;
  GLsizei height = 8;
  GLint border = 0;

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, level, GL_RG8, width, height, 0, format, type,
               shared_memory_id_, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  target, level, internal_format, 0, 0, width, height, border))
      .Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexImage2DInvalidInternalFormat_Float) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RG16F;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  GLsizei width = 16;
  GLsizei height = 8;
  GLint border = 0;

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, width, height, 0, format, type,
               shared_memory_id_, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  target, level, internal_format, 0, 0, width, height, border))
      .Times(0);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexImage2DInvalidInternalFormat_Integer) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RG8I;
  GLenum format = GL_RG_INTEGER;
  GLenum type = GL_UNSIGNED_BYTE;
  GLsizei width = 16;
  GLsizei height = 8;
  GLint border = 0;

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, level, GL_RG8UI, width, height, 0, format, type,
               shared_memory_id_, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  target, level, internal_format, 0, 0, width, height, border))
      .Times(0);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexImage2DInvalidInternalFormat_sRGB) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_SRGB8;
  GLenum format = GL_RGB;
  GLenum type = GL_UNSIGNED_BYTE;
  GLsizei width = 16;
  GLsizei height = 8;
  GLint border = 0;

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, level, GL_RGB, width, height, 0, format, type,
               shared_memory_id_, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  target, level, internal_format, 0, 0, width, height, border))
      .Times(0);
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest,
       UnClearedAttachmentsGetClearedOnReadPixelsAndDrawBufferGetsRestored) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render from" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);

  // Set scissor rect and disable GL_SCISSOR_TEST to make sure we enable it in
  // the clear, then disable it and restore the rect again.
  DoScissor(0, 0, 32, 32);
  DoEnableDisable(GL_SCISSOR_TEST, false);

  SetupExpectationsForFramebufferClearingMulti(
      kServiceFramebufferId,  // read framebuffer service id
      0,                      // backbuffer service id
      GL_READ_FRAMEBUFFER,    // target
      GL_COLOR_BUFFER_BIT,    // clear bits
      0, 0, 0,
      0,      // color
      0,      // stencil
      1.0f,   // depth
      false,  // scissor test
      0, 0, 32, 32);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, _))
      .Times(1)
      .RetiresOnSaturation();
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
  cmds::ReadPixels cmd;
  cmd.Init(0,
           0,
           1,
           1,
           GL_RGBA,
           GL_UNSIGNED_BYTE,
           pixels_shm_id,
           pixels_shm_offset,
           result_shm_id,
           result_shm_offset,
           false);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, CopyTexImageWithInCompleteFBOFails) {
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  SetupTexture();
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 0, 0, GL_NO_ERROR);
  DoFramebufferRenderbuffer(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER,
                            client_renderbuffer_id_,
                            kServiceRenderbufferId,
                            GL_NO_ERROR);

  EXPECT_CALL(*gl_, CopyTexImage2D(_, _, _, _, _, _, _, _))
      .Times(0)
      .RetiresOnSaturation();
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_FRAMEBUFFER_OPERATION, GetGLError());
}

void GLES2DecoderWithShaderTest::CheckRenderbufferChangesMarkFBOAsNotComplete(
    bool bound_fbo) {
  FramebufferManager* framebuffer_manager = GetFramebufferManager();
  SetupTexture();
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1, GL_NO_ERROR);
  DoFramebufferRenderbuffer(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER,
                            client_renderbuffer_id_,
                            kServiceRenderbufferId,
                            GL_NO_ERROR);

  if (!bound_fbo) {
    DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);
  }

  Framebuffer* framebuffer =
      framebuffer_manager->GetFramebuffer(client_framebuffer_id_);
  ASSERT_TRUE(framebuffer != nullptr);
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));

  // Test that renderbufferStorage marks fbo as not complete.
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, 1, 1, GL_NO_ERROR);
  EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));

  // Test deleting renderbuffer marks fbo as not complete.
  DoDeleteRenderbuffer(client_renderbuffer_id_, kServiceRenderbufferId);
  if (bound_fbo) {
    EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));
  } else {
    EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));
  }
  // Cleanup
  DoDeleteFramebuffer(client_framebuffer_id_,
                      kServiceFramebufferId,
                      bound_fbo,
                      GL_FRAMEBUFFER,
                      0,
                      bound_fbo,
                      GL_FRAMEBUFFER,
                      0);
}

TEST_P(GLES2DecoderWithShaderTest,
       RenderbufferChangesMarkFBOAsNotCompleteBoundFBO) {
  CheckRenderbufferChangesMarkFBOAsNotComplete(true);
}

TEST_P(GLES2DecoderWithShaderTest,
       RenderbufferChangesMarkFBOAsNotCompleteUnboundFBO) {
  CheckRenderbufferChangesMarkFBOAsNotComplete(false);
}

void GLES2DecoderWithShaderTest::CheckTextureChangesMarkFBOAsNotComplete(
    bool bound_fbo) {
  FramebufferManager* framebuffer_manager = GetFramebufferManager();
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  SetupTexture();

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);

  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER,
                        GL_DEPTH_COMPONENT16,
                        1,
                        1,
                        GL_NO_ERROR);
  DoFramebufferRenderbuffer(GL_FRAMEBUFFER,
                            GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER,
                            client_renderbuffer_id_,
                            kServiceRenderbufferId,
                            GL_NO_ERROR);

  if (!bound_fbo) {
    DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);
  }

  Framebuffer* framebuffer =
      framebuffer_manager->GetFramebuffer(client_framebuffer_id_);
  ASSERT_TRUE(framebuffer != nullptr);
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));

  // Test TexImage2D marks fbo as not complete.
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0, 0);
  EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));

  // Test CopyImage2D marks fbo as not complete.
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1, 1, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::CopyTexImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1, 1);
  // Unbind fbo and bind again after CopyTexImage2D tp avoid feedback loops.
  if (bound_fbo) {
    DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);
  }
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  if (bound_fbo) {
    DoBindFramebuffer(
        GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  }
  EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));

  // Test deleting texture marks fbo as not complete.
  framebuffer_manager->MarkAsComplete(framebuffer);
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));
  DoDeleteTexture(kFBOClientTextureId, kFBOServiceTextureId);

  if (bound_fbo) {
    EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));
  } else {
    EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));
  }
  // Cleanup
  DoDeleteFramebuffer(client_framebuffer_id_,
                      kServiceFramebufferId,
                      bound_fbo,
                      GL_FRAMEBUFFER,
                      0,
                      bound_fbo,
                      GL_FRAMEBUFFER,
                      0);
}

TEST_P(GLES2DecoderWithShaderTest, TextureChangesMarkFBOAsNotCompleteBoundFBO) {
  CheckTextureChangesMarkFBOAsNotComplete(true);
}

TEST_P(GLES2DecoderWithShaderTest,
       TextureChangesMarkFBOAsNotCompleteUnboundFBO) {
  CheckTextureChangesMarkFBOAsNotComplete(false);
}

TEST_P(GLES2DecoderTest, CanChangeSurface) {
  scoped_refptr<GLSurfaceMock> other_surface(new GLSurfaceMock);
  EXPECT_CALL(*other_surface.get(), GetBackingFramebufferObject())
      .WillOnce(Return(7));
  EXPECT_CALL(*gl_, BindFramebufferEXT(GL_FRAMEBUFFER_EXT, 7));

  decoder_->SetSurface(other_surface);
}

TEST_P(GLES3DecoderTest, DrawBuffersEXTImmediateSucceeds) {
  const GLsizei count = 1;
  const GLenum bufs[] = {GL_COLOR_ATTACHMENT0};
  auto& cmd = *GetImmediateAs<cmds::DrawBuffersEXTImmediate>();
  cmd.Init(count, bufs);

  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_CALL(*gl_, DrawBuffersARB(count, _)).Times(1).RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(bufs)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, DrawBuffersEXTImmediateFails) {
  const GLsizei count = 1;
  const GLenum bufs[] = {GL_COLOR_ATTACHMENT1_EXT};
  auto& cmd = *GetImmediateAs<cmds::DrawBuffersEXTImmediate>();
  cmd.Init(count, bufs);

  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(bufs)));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, DrawBuffersEXTImmediateBackbuffer) {
  const GLsizei count = 1;
  const GLenum bufs[] = {GL_BACK};
  auto& cmd = *GetImmediateAs<cmds::DrawBuffersEXTImmediate>();
  cmd.Init(count, bufs);

  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(bufs)));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);  // unbind

  EXPECT_CALL(*gl_, DrawBuffersARB(count, _)).Times(1).RetiresOnSaturation();

  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(bufs)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, DrawBuffersEXTMainFramebuffer) {
  auto& cmd = *GetImmediateAs<cmds::DrawBuffersEXTImmediate>();
  {
    const GLenum bufs[] = {GL_BACK};
    const GLsizei count = std::size(bufs);
    cmd.Init(count, bufs);

    EXPECT_CALL(*gl_, DrawBuffersARB(count, Pointee(GL_BACK)))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(bufs)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
  {
    const GLsizei count = 0;
    cmd.Init(count, nullptr);

    EXPECT_CALL(*gl_, DrawBuffersARB(_, _))
        .Times(0)
        .RetiresOnSaturation();
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, 0));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }
  {
    const GLenum bufs[] = {GL_BACK, GL_NONE};
    const GLsizei count = std::size(bufs);
    cmd.Init(count, bufs);

    EXPECT_CALL(*gl_, DrawBuffersARB(_, _)).Times(0).RetiresOnSaturation();
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(bufs)));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }
}

TEST_P(GLES2DecoderManualInitTest, InvalidateFramebufferBinding) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  InitDecoder(init);

  // EXPECT_EQ can't be used to compare function pointers
  EXPECT_TRUE(
      gl::MockGLInterface::GetGLProcAddress("glInvalidateFramebuffer") !=
      reinterpret_cast<gl::GLFunctionPointerType>(
          gl::g_current_gl_driver->fn.glDiscardFramebufferEXTFn));
  EXPECT_TRUE(
      gl::MockGLInterface::GetGLProcAddress("glInvalidateFramebuffer") !=
      gl::MockGLInterface::GetGLProcAddress("glDiscardFramebufferEXT"));
}

TEST_P(GLES2DecoderTest, ClearBackbufferBitsOnFlipSwap) {
  surface_->set_buffers_flipped(true);

  EXPECT_EQ(0u, GetAndClearBackbufferClearBitsForTest());

  auto& cmd = *GetImmediateAs<cmds::SwapBuffers>();
  cmd.Init(1, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<uint32_t>(GL_COLOR_BUFFER_BIT),
            GetAndClearBackbufferClearBitsForTest());

  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0u, GetAndClearBackbufferClearBitsForTest());

  EXPECT_CALL(*gl_, Finish()).Times(AnyNumber());
  auto& resize_cmd = *GetImmediateAs<cmds::ResizeCHROMIUM>();
  resize_cmd.Init(1, 1, 1.0f, GL_TRUE, 0, 0, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(resize_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<uint32_t>(GL_COLOR_BUFFER_BIT),
            GetAndClearBackbufferClearBitsForTest());

  cmd.Init(1, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<uint32_t>(GL_COLOR_BUFFER_BIT),
            GetAndClearBackbufferClearBitsForTest());

  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0u, GetAndClearBackbufferClearBitsForTest());
}

TEST_P(GLES2DecoderManualInitTest, DiscardFramebufferEXT) {
  InitState init;
  init.extensions = "GL_EXT_discard_framebuffer";
  init.gl_version = "OpenGL ES 2.0";
  InitDecoder(init);

  // EXPECT_EQ can't be used to compare function pointers
  EXPECT_TRUE(
      gl::MockGLInterface::GetGLProcAddress("glDiscardFramebufferEXT") ==
      reinterpret_cast<gl::GLFunctionPointerType>(
          gl::g_current_gl_driver->fn.glDiscardFramebufferEXTFn));

  const GLenum target = GL_FRAMEBUFFER;
  const GLsizei count = 1;
  const GLenum attachments[] = {GL_COLOR_ATTACHMENT0};

  SetupTexture();
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         client_texture_id_,
                         kServiceTextureId,
                         0,
                         GL_NO_ERROR);
  FramebufferManager* framebuffer_manager = GetFramebufferManager();
  Framebuffer* framebuffer =
      framebuffer_manager->GetFramebuffer(client_framebuffer_id_);
  EXPECT_TRUE(framebuffer->IsCleared());

  EXPECT_CALL(*gl_, DiscardFramebufferEXT(target, count, _))
      .Times(1)
      .RetiresOnSaturation();
  auto& cmd = *GetImmediateAs<cmds::DiscardFramebufferEXTImmediate>();
  cmd.Init(target, count, attachments);

  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(attachments)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(framebuffer->IsCleared());
}

TEST_P(GLES2DecoderManualInitTest, ClearBackbufferBitsOnDiscardFramebufferEXT) {
  InitState init;
  init.extensions = "GL_EXT_discard_framebuffer";
  init.gl_version = "OpenGL ES 2.0";
  InitDecoder(init);

  // EXPECT_EQ can't be used to compare function pointers.
  EXPECT_TRUE(
      gl::MockGLInterface::GetGLProcAddress("glDiscardFramebufferEXT") ==
      reinterpret_cast<gl::GLFunctionPointerType>(
          gl::g_current_gl_driver->fn.glDiscardFramebufferEXTFn));

  const GLenum target = GL_FRAMEBUFFER;
  const GLsizei count = 1;
  GLenum attachments[] = {GL_COLOR_EXT};

  EXPECT_CALL(*gl_, DiscardFramebufferEXT(target, count, _))
      .Times(1)
      .RetiresOnSaturation();
  auto& cmd = *GetImmediateAs<cmds::DiscardFramebufferEXTImmediate>();
  cmd.Init(target, count, attachments);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(attachments)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<uint32_t>(GL_COLOR_BUFFER_BIT),
            GetAndClearBackbufferClearBitsForTest());

  attachments[0] = GL_DEPTH_EXT;
  EXPECT_CALL(*gl_, DiscardFramebufferEXT(target, count, _))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(target, count, attachments);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(attachments)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<uint32_t>(GL_DEPTH_BUFFER_BIT),
            GetAndClearBackbufferClearBitsForTest());

  attachments[0] = GL_STENCIL_EXT;
  EXPECT_CALL(*gl_, DiscardFramebufferEXT(target, count, _))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(target, count, attachments);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(attachments)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<uint32_t>(GL_STENCIL_BUFFER_BIT),
            GetAndClearBackbufferClearBitsForTest());

  const GLsizei count0 = 3;
  const GLenum attachments0[] = {GL_COLOR_EXT, GL_DEPTH_EXT, GL_STENCIL_EXT};
  EXPECT_CALL(*gl_, DiscardFramebufferEXT(target, count0, _))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(target, count0, attachments0);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(attachments0)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<uint32_t>(GL_COLOR_BUFFER_BIT |
                                  GL_DEPTH_BUFFER_BIT |
                                  GL_STENCIL_BUFFER_BIT),
            GetAndClearBackbufferClearBitsForTest());
}

TEST_P(GLES2DecoderTest, DiscardFramebufferEXTUnsupported) {
  const GLenum target = GL_FRAMEBUFFER;
  const GLsizei count = 1;
  const GLenum attachments[] = {GL_COLOR_EXT};
  auto& cmd = *GetImmediateAs<cmds::DiscardFramebufferEXTImmediate>();
  cmd.Init(target, count, attachments);

  // Should not result into a call into GL.
  EXPECT_EQ(error::kUnknownCommand,
            ExecuteImmediateCmd(cmd, sizeof(attachments)));
}

TEST_P(GLES3DecoderTest, DiscardFramebufferEXTInvalidTarget) {
  const GLenum target = GL_RED;  // Invalid
  const GLsizei count = 1;
  const GLenum attachments[] = {GL_COLOR_ATTACHMENT0};

  SetupTexture();
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         client_texture_id_,
                         kServiceTextureId,
                         0,
                         GL_NO_ERROR);
  FramebufferManager* framebuffer_manager = GetFramebufferManager();
  Framebuffer* framebuffer =
      framebuffer_manager->GetFramebuffer(client_framebuffer_id_);
  EXPECT_TRUE(framebuffer->IsCleared());

  EXPECT_CALL(*gl_, InvalidateFramebuffer(target, count, _)).Times(0);
  auto& cmd = *GetImmediateAs<cmds::DiscardFramebufferEXTImmediate>();
  cmd.Init(target, count, attachments);

  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(attachments)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  EXPECT_TRUE(framebuffer->IsCleared());
}

TEST_P(GLES3DecoderTest, DiscardFramebufferEXTUseCorrectTarget) {
  const GLenum target = GL_READ_FRAMEBUFFER;
  const GLsizei count = 1;
  const GLenum attachments[] = {GL_COLOR_ATTACHMENT0};

  SetupTexture();
  DoBindFramebuffer(
      GL_READ_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         client_texture_id_,
                         kServiceTextureId,
                         0,
                         GL_NO_ERROR);

  EXPECT_CALL(*gl_, GenFramebuffersEXT(_, _))
      .WillOnce(SetArgPointee<1>(kServiceFramebufferId + 1))
      .RetiresOnSaturation();
  DoBindFramebuffer(GL_DRAW_FRAMEBUFFER, client_framebuffer_id_ + 1,
                    kServiceFramebufferId + 1);
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kServiceTextureId + 1))
      .RetiresOnSaturation();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_ + 1, kServiceTextureId + 1);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
  DoFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         client_texture_id_ + 1,
                         kServiceTextureId + 1,
                         0,
                         GL_NO_ERROR);

  FramebufferManager* framebuffer_manager = GetFramebufferManager();
  Framebuffer* framebuffer =
      framebuffer_manager->GetFramebuffer(client_framebuffer_id_);
  EXPECT_TRUE(framebuffer->IsCleared());
  Framebuffer* other_framebuffer =
      framebuffer_manager->GetFramebuffer(client_framebuffer_id_ + 1);
  EXPECT_TRUE(other_framebuffer->IsCleared());

  EXPECT_CALL(*gl_, InvalidateFramebuffer(target, count, _))
      .Times(1)
      .RetiresOnSaturation();
  auto& cmd = *GetImmediateAs<cmds::DiscardFramebufferEXTImmediate>();
  cmd.Init(target, count, attachments);

  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(attachments)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(framebuffer->IsCleared());
  EXPECT_TRUE(other_framebuffer->IsCleared());
}

TEST_P(GLES2DecoderManualInitTest,
       DiscardedAttachmentsEXTMarksFramebufferIncomplete) {
  InitState init;
  init.extensions = "GL_EXT_discard_framebuffer";
  init.gl_version = "OpenGL ES 2.0";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;

  // Register a texture id.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);

  // Setup "render to" texture.
  DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         kFBOClientTextureId,
                         kFBOServiceTextureId,
                         0,
                         GL_NO_ERROR);
  DoEnableDisable(GL_SCISSOR_TEST, false);
  DoScissor(0, 0, 1, 1);

  // Setup "render from" texture.
  SetupTexture();

  SetupExpectationsForFramebufferClearing(GL_FRAMEBUFFER,       // target
                                          GL_COLOR_BUFFER_BIT,  // clear bits
                                          0, 0, 0,
                                          0,      // color
                                          0,      // stencil
                                          1.0f,   // depth
                                          false,  // scissor test
                                          0, 0, 1, 1);
  SetupExpectationsForApplyingDirtyState(false,    // Framebuffer is RGB
                                         false,    // Framebuffer has depth
                                         false,    // Framebuffer has stencil
                                         0x1111,   // color bits
                                         false,    // depth mask
                                         false,    // depth enabled
                                         0,        // front stencil mask
                                         0,        // back stencil mask
                                         false);   // stencil enabled

  EXPECT_CALL(*gl_, Clear(GL_COLOR_BUFFER_BIT)).Times(1).RetiresOnSaturation();

  cmds::Clear clear_cmd;
  clear_cmd.Init(GL_COLOR_BUFFER_BIT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(clear_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Check that framebuffer is cleared and complete.
  FramebufferManager* framebuffer_manager = GetFramebufferManager();
  Framebuffer* framebuffer =
      framebuffer_manager->GetFramebuffer(client_framebuffer_id_);
  EXPECT_TRUE(framebuffer->IsCleared());
  EXPECT_TRUE(framebuffer_manager->IsComplete(framebuffer));

  // Check that Discard GL_COLOR_ATTACHMENT0, sets the attachment as uncleared
  // and the framebuffer as incomplete.
  EXPECT_TRUE(
      gl::MockGLInterface::GetGLProcAddress("glDiscardFramebufferEXT") ==
      reinterpret_cast<gl::GLFunctionPointerType>(
          gl::g_current_gl_driver->fn.glDiscardFramebufferEXTFn));

  const GLenum target = GL_FRAMEBUFFER;
  const GLsizei count = 1;
  const GLenum attachments[] = {GL_COLOR_ATTACHMENT0};

  auto& discard_cmd = *GetImmediateAs<cmds::DiscardFramebufferEXTImmediate>();
  discard_cmd.Init(target, count, attachments);

  EXPECT_CALL(*gl_, DiscardFramebufferEXT(target, count, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(discard_cmd, sizeof(attachments)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(framebuffer->IsCleared());
  EXPECT_FALSE(framebuffer_manager->IsComplete(framebuffer));
}

TEST_P(GLES3DecoderTest, ImplementationReadColorFormatAndType) {
  ClearSharedMemory();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D,
                         client_texture_id_,
                         kServiceTextureId,
                         0,
                         GL_NO_ERROR);

  auto* result =
      static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
  cmds::GetIntegerv cmd;

  result->size = 0;
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, _))
      .WillOnce(SetArgPointee<1>(GL_RGBA))
      .RetiresOnSaturation();
  cmd.Init(GL_IMPLEMENTATION_COLOR_READ_FORMAT,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  result->size = 0;
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, _))
      .WillOnce(SetArgPointee<1>(GL_UNSIGNED_BYTE))
      .RetiresOnSaturation();
  cmd.Init(GL_IMPLEMENTATION_COLOR_READ_TYPE,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTextureLayerNoBoundFramebuffer) {
  DoBindTexture(GL_TEXTURE_3D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, FramebufferTextureLayer(_, _, _, _, _)).Times(0);
  cmds::FramebufferTextureLayer cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, client_texture_id_, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTextureLayerInvalidTextureTarget) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, FramebufferTextureLayer(_, _, _, _, _)).Times(0);
  cmds::FramebufferTextureLayer cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, client_texture_id_, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTextureLayerValidArgs) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindTexture(GL_TEXTURE_3D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, FramebufferTextureLayer(GL_FRAMEBUFFER,
                                            GL_COLOR_ATTACHMENT0,
                                            kServiceTextureId, 4, 5))
      .Times(1)
      .RetiresOnSaturation();
  cmds::FramebufferTextureLayer cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, client_texture_id_, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, FramebufferTextureLayerDepthStencil) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindTexture(GL_TEXTURE_2D_ARRAY, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, FramebufferTextureLayer(GL_FRAMEBUFFER,
                                            GL_DEPTH_STENCIL_ATTACHMENT,
                                            kServiceTextureId, 4, 5))
      .Times(1)
      .RetiresOnSaturation();
  cmds::FramebufferTextureLayer cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, client_texture_id_, 4,
           5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  Framebuffer* framebuffer = GetFramebuffer(client_framebuffer_id_);
  ASSERT_TRUE(framebuffer);
  ASSERT_FALSE(framebuffer->GetAttachment(GL_DEPTH_STENCIL_ATTACHMENT));
  ASSERT_TRUE(framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT));
  ASSERT_TRUE(framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT));
}

TEST_P(GLES3DecoderTest, InvalidateFramebufferDepthStencilAttachment) {
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1,
                        GL_NO_ERROR);
  DoFramebufferRenderbuffer(
      GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
      client_renderbuffer_id_, kServiceRenderbufferId, GL_NO_ERROR);

  Framebuffer* framebuffer = GetFramebuffer(client_framebuffer_id_);
  ASSERT_TRUE(framebuffer);
  ASSERT_FALSE(framebuffer->GetAttachment(GL_DEPTH_STENCIL_ATTACHMENT));
  ASSERT_TRUE(framebuffer->GetAttachment(GL_DEPTH_ATTACHMENT));
  ASSERT_TRUE(framebuffer->GetAttachment(GL_STENCIL_ATTACHMENT));
  framebuffer->MarkAttachmentAsCleared(group().renderbuffer_manager(), nullptr,
                                       GL_DEPTH_ATTACHMENT, true);
  framebuffer->MarkAttachmentAsCleared(group().renderbuffer_manager(), nullptr,
                                       GL_STENCIL_ATTACHMENT, true);
  EXPECT_TRUE(framebuffer->IsCleared());

  const GLenum target = GL_FRAMEBUFFER;
  const GLsizei count = 1;
  GLenum attachments[] = {GL_DEPTH_ATTACHMENT};
  EXPECT_CALL(*gl_, InvalidateFramebuffer(target, 0, _))
      .Times(1)
      .RetiresOnSaturation();
  auto& cmd = *GetImmediateAs<cmds::InvalidateFramebufferImmediate>();
  cmd.Init(target, count, attachments);

  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(attachments)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Invalidating part of DEPTH_STENCIL attachment doesn't change framebuffer
  // clearance status.
  EXPECT_TRUE(framebuffer->IsCleared());
  EXPECT_FALSE(framebuffer->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_FALSE(framebuffer->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));

  attachments[0] = GL_DEPTH_STENCIL_ATTACHMENT;
  EXPECT_CALL(*gl_, InvalidateFramebuffer(target, 2, _))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(target, count, attachments);

  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(attachments)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Invalidating DEPTH_STENCIL attachment should make framebuffer uncleared.
  EXPECT_FALSE(framebuffer->IsCleared());
  EXPECT_TRUE(framebuffer->HasUnclearedAttachment(GL_DEPTH_ATTACHMENT));
  EXPECT_TRUE(framebuffer->HasUnclearedAttachment(GL_STENCIL_ATTACHMENT));
}

TEST_P(GLES3DecoderTest, BlitFramebufferFeedbackLoopDefaultFramebuffer) {
  // Run BlitFramebufferCHROMIUM targetting the default framebuffer for both
  // read and draw, should result in a feedback loop.
  cmds::BlitFramebufferCHROMIUM cmd;
  cmd.Init(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  cmd.Init(0, 0, 1, 1, 0, 0, 1, 1, GL_DEPTH_BUFFER_BIT, GL_LINEAR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  cmd.Init(0, 0, 1, 1, 0, 0, 1, 1, GL_STENCIL_BUFFER_BIT, GL_LINEAR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, BlitFramebufferDisabledReadBuffer) {
  // Run BlitFramebufferCHROMIUM from a disabled read buffer. Even though the
  // read and draw framebuffer use the same attachment, since the read buffer is
  // disabled, no feedback loop happens.
  DoBindFramebuffer(GL_DRAW_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                     kServiceRenderbufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1, GL_NO_ERROR);
  DoFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, client_renderbuffer_id_,
                            kServiceRenderbufferId, GL_NO_ERROR);

  EXPECT_CALL(*gl_, GenFramebuffersEXT(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  GLuint read_fbo = client_framebuffer_id_ + 1;
  DoBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo, kNewServiceId);
  DoFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, client_renderbuffer_id_,
                            kServiceRenderbufferId, GL_NO_ERROR);
  {
    EXPECT_CALL(*gl_, ReadBuffer(GL_NONE))
        .Times(1)
        .RetiresOnSaturation();
    cmds::ReadBuffer cmd;
    cmd.Init(GL_NONE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  {
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
    SetupExpectationsForFramebufferClearing(GL_DRAW_FRAMEBUFFER,  // target
                                            GL_COLOR_BUFFER_BIT,  // clear bits
                                            0, 0, 0, 0,           // color
                                            0,                    // stencil
                                            1.0f,                 // depth
                                            false,  // scissor test
                                            0, 0, 128, 64);
    cmds::BlitFramebufferCHROMIUM cmd;
    cmd.Init(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    // Generate INVALID_OPERATION because of missing read buffer image.
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }
}

TEST_P(GLES3DecoderTest, BlitFramebufferMissingDepthOrStencil) {
  // Run BlitFramebufferCHROMIUM with depth or stencil bits, from/to a read/draw
  // framebuffer that doesn't have depth/stencil. It should generate
  // INVALID_OPERATION.
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                     kServiceRenderbufferId);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1,
                        GL_NO_ERROR);
  GLuint color_renderbuffer = client_renderbuffer_id_ + 1;
  GLuint color_renderbuffer_service = kServiceRenderbufferId + 1;
  EXPECT_CALL(*gl_, GenRenderbuffersEXT(1, _))
      .WillOnce(SetArgPointee<1>(color_renderbuffer_service))
      .RetiresOnSaturation();
  DoBindRenderbuffer(GL_RENDERBUFFER, color_renderbuffer,
                     color_renderbuffer_service);
  DoRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1, GL_NO_ERROR);
  DoBindFramebuffer(GL_DRAW_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, client_renderbuffer_id_,
                            kServiceRenderbufferId, GL_NO_ERROR);
  DoFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, client_renderbuffer_id_,
                            kServiceRenderbufferId, GL_NO_ERROR);
  DoFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, color_renderbuffer,
                            color_renderbuffer_service, GL_NO_ERROR);

  EXPECT_CALL(*gl_, GenFramebuffersEXT(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  GLuint color_fbo = client_framebuffer_id_ + 1;
  DoBindFramebuffer(GL_READ_FRAMEBUFFER, color_fbo, kNewServiceId);
  DoFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, color_renderbuffer,
                            color_renderbuffer_service, GL_NO_ERROR);

  {
    SetupExpectationsForFramebufferClearing(
        GL_DRAW_FRAMEBUFFER,  // target
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT,  // clear bits
        0,
        0, 0, 0,  // color
        0,        // stencil
        1.0f,     // depth
        false,    // scissor test
        0, 0, 128, 64);
    EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER))
        .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
        .RetiresOnSaturation();
    cmds::BlitFramebufferCHROMIUM cmd;
    cmd.Init(0, 0, 1, 1, 0, 0, 1, 1, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
    cmd.Init(0, 0, 1, 1, 0, 0, 1, 1, GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }

  // Switch FBOs and try the same.
  DoBindFramebuffer(GL_READ_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  DoBindFramebuffer(GL_DRAW_FRAMEBUFFER, color_fbo, kNewServiceId);
  {
    EXPECT_CALL(*gl_, BlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, _, _)).Times(0);
    cmds::BlitFramebufferCHROMIUM cmd;
    cmd.Init(0, 0, 1, 1, 0, 0, 1, 1, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    cmd.Init(0, 0, 1, 1, 0, 0, 1, 1, GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
}

TEST_P(GLES2DecoderManualInitTest, MESAFramebufferFlipYExtensionEnabled) {
  InitState init;
  init.gl_version = "OpenGL ES 3.1";
  init.context_type = CONTEXT_TYPE_WEBGL1;
  init.extensions = "GL_MESA_framebuffer_flip_y";
  InitDecoder(init);

  EXPECT_TRUE(feature_info()->validators()->framebuffer_parameter.IsValid(
      GL_FRAMEBUFFER_FLIP_Y_MESA));

  EXPECT_CALL(*gl_, FramebufferParameteri(_, _, _))
      .Times(1)
      .RetiresOnSaturation();

  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  cmds::FramebufferParameteri cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_FRAMEBUFFER_FLIP_Y_MESA, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, MESAFramebufferFlipYExtensionDisabled) {
  InitState init;
  init.gl_version = "OpenGL ES 3.1";
  init.context_type = CONTEXT_TYPE_WEBGL1;
  InitDecoder(init);

  EXPECT_FALSE(feature_info()->validators()->framebuffer_parameter.IsValid(
      GL_FRAMEBUFFER_FLIP_Y_MESA));

  EXPECT_CALL(*gl_, FramebufferParameteri(_, _, _))
      .Times(0)
      .RetiresOnSaturation();

  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);
  cmds::FramebufferParameteri cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_FRAMEBUFFER_FLIP_Y_MESA, 1);
  EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

// TODO(gman): PixelStorei

// TODO(gman): SwapBuffers

}  // namespace gles2
}  // namespace gpu
