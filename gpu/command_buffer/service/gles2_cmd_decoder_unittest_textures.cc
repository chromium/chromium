// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_surface_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_image_stub.h"
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
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace {
class EmulatingRGBImageStub : public gl::GLImageStub {
 protected:
  ~EmulatingRGBImageStub() override = default;
  bool EmulatingRGB() const override {
    return true;
  }
};
}  // namespace

namespace gpu {
namespace gles2 {

TEST_P(GLES2DecoderTest, GenerateMipmapWrongFormatsFails) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 16, 17, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  cmds::GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, GenerateMipmapHandlesOutOfMemory) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  GLint width = 0;
  GLint height = 0;
  EXPECT_FALSE(
      texture->GetLevelSize(GL_TEXTURE_2D, 2, &width, &height, nullptr));
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GenerateMipmapEXT(GL_TEXTURE_2D)).Times(1);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  cmds::GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_FALSE(
      texture->GetLevelSize(GL_TEXTURE_2D, 2, &width, &height, nullptr));
}

TEST_P(GLES2DecoderTest, GenerateMipmapClearsUnclearedTexture) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, 0, 0, 2, 2, 0);
  EXPECT_CALL(*gl_, GenerateMipmapEXT(GL_TEXTURE_2D));
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, GenerateMipmapBaseLevel) {
  EXPECT_CALL(*gl_, GenerateMipmapEXT(_)).Times(0);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 2, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);

  {
    EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2));
    cmds::TexParameteri cmd;
    cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                GL_TEXTURE_2D, GL_TEXTURE_2D, 2, GL_RGBA,
                                GL_UNSIGNED_BYTE, 0, 0, 2, 2, 0);
  EXPECT_CALL(*gl_, GenerateMipmapEXT(GL_TEXTURE_2D));
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, ActiveTextureValidArgs) {
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE1));
  SpecializedSetup<cmds::ActiveTexture, 0>(true);
  cmds::ActiveTexture cmd;
  cmd.Init(GL_TEXTURE1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, ActiveTextureInvalidArgs) {
  EXPECT_CALL(*gl_, ActiveTexture(_)).Times(0);
  SpecializedSetup<cmds::ActiveTexture, 0>(false);
  cmds::ActiveTexture cmd;
  cmd.Init(GL_TEXTURE0 - 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(kNumTextureUnits);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, TexSubImage2DValidArgs) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              TexSubImage2D(GL_TEXTURE_2D,
                            1,
                            1,
                            0,
                            kWidth - 1,
                            kHeight,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 1, 1, 0, kWidth - 1, kHeight, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // 0 size with null SHM
  EXPECT_CALL(*gl_, TexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 0, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, nullptr))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0,
           GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, TexSubImage2DBadArgs) {
  const int kWidth = 8;
  const int kHeight = 4;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D,
               1,
               GL_RGBA,
               kWidth,
               kHeight,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               0,
               0);
  cmds::TexSubImage2D cmd;

  // Invalid target.
  cmd.Init(GL_TEXTURE0, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Invalid format / type.
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_TRUE, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_INT,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Invalid offsets/sizes.
  cmd.Init(GL_TEXTURE_2D, 1, -1, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 1, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, -1, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 1, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth + 1, kHeight, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight + 1, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Incompatible format / type.
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGB, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA,
           GL_UNSIGNED_SHORT_4_4_4_4, shared_memory_id_, kSharedMemoryOffset,
           GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Above errors should still happen with NULL data.
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_INT, 0,
           0, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, -1, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           0, 0, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth + 1, kHeight, GL_RGBA,
           GL_UNSIGNED_BYTE, 0, 0, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Invalid SHM / offset.
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           kInvalidSharedMemoryId, kSharedMemoryOffset, GL_FALSE);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kInvalidSharedMemoryOffset, GL_FALSE);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
           0, 0, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, TexSubImage3DValidArgs) {
  const int kWidth = 8;
  const int kHeight = 4;
  const int kDepth = 2;
  DoBindTexture(GL_TEXTURE_3D, client_texture_id_, kServiceTextureId);
  DoTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA, kWidth, kHeight, kDepth, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              TexSubImage3DWithData(GL_TEXTURE_3D, 1, 1, 0, 0, kWidth - 1,
                                    kHeight, kDepth, GL_RGBA, GL_UNSIGNED_BYTE))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexSubImage3D cmd;
  cmd.Init(GL_TEXTURE_3D, 1, 1, 0, 0, kWidth - 1, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // 0 size with null SHM
  EXPECT_CALL(*gl_, TexSubImage3DNoData(GL_TEXTURE_3D, 1, 1, 0, 0, 0, 0, 0,
                                        GL_RGBA, GL_UNSIGNED_BYTE))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(GL_TEXTURE_3D, 1, 1, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0,
           GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, TexSubImage3DBadArgs) {
  const int kWidth = 4;
  const int kHeight = 4;
  const int kDepth = 2;
  DoBindTexture(GL_TEXTURE_3D, client_texture_id_, kServiceTextureId);
  DoTexImage3D(GL_TEXTURE_3D, 1, GL_RGBA, kWidth, kHeight, kDepth, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, 0, 0);
  cmds::TexSubImage3D cmd;

  // Invalid target.
  cmd.Init(GL_TEXTURE0, 1, 0, 0, 0, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Invalid format.
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight, kDepth, GL_TRUE,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Invalid offsets/sizes.
  cmd.Init(GL_TEXTURE_3D, 1, -1, 0, 0, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 1, 0, 0, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 0, -1, 0, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 0, 1, 0, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, -1, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 1, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth + 1, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight + 1, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight, kDepth + 1, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Incompatible format.
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight, kDepth, GL_RGB,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Above errors should still happen with NULL data.
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight, kDepth, GL_TRUE,
           GL_UNSIGNED_BYTE, 0, 0, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight, kDepth + 1, GL_RGBA,
           GL_UNSIGNED_BYTE, 0, 0, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight, kDepth, GL_RGB,
           GL_UNSIGNED_BYTE, 0, 0, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Invalid SHM / offset.
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, kInvalidSharedMemoryId, kSharedMemoryOffset,
           GL_FALSE);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kInvalidSharedMemoryOffset,
           GL_FALSE);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_TEXTURE_3D, 1, 0, 0, 0, kWidth, kHeight, kDepth, GL_RGBA,
           GL_UNSIGNED_BYTE, 0, 0, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, TexSubImage2DTypesDoNotMatchUnsizedFormat) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
               GL_UNSIGNED_SHORT_4_4_4_4, shared_memory_id_,
               kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              TexSubImage2D(GL_TEXTURE_2D,
                            1,
                            1,
                            0,
                            kWidth - 1,
                            kHeight,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 1, 1, 0, kWidth - 1, kHeight, GL_RGBA,
           GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, TexSubImage2DTypesDoNotMatchSizedFormat) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA4, kWidth, kHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              TexSubImage2D(GL_TEXTURE_2D,
                            1,
                            1,
                            0,
                            kWidth - 1,
                            kHeight,
                            GL_RGBA,
                            GL_UNSIGNED_SHORT_4_4_4_4,
                            shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 1, 1, 0, kWidth - 1, kHeight, GL_RGBA,
           GL_UNSIGNED_SHORT_4_4_4_4, shared_memory_id_, kSharedMemoryOffset,
           GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, CopyTexSubImage2DValidArgs) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              CopyTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 0, 0, kWidth, kHeight))
      .Times(1)
      .RetiresOnSaturation();
  cmds::CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, CopyTexSubImage2DBadArgs) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D,
               1,
               GL_RGBA,
               kWidth,
               kHeight,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               0,
               0);
  cmds::CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE0, 1, 0, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, -1, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 1, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, -1, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 1, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, 0, 0, kWidth + 1, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(GL_TEXTURE_2D, 1, 0, 0, 0, 0, kWidth, kHeight + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest, TexImage2DRedefinitionSucceeds) {
  const int kWidth = 16;
  const int kHeight = 8;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  for (int ii = 0; ii < 2; ++ii) {
    cmds::TexImage2D cmd;
    if (ii == 0) {
      EXPECT_CALL(*gl_,
                  TexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RGBA,
                             kWidth,
                             kHeight,
                             0,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             _))
          .Times(1)
          .RetiresOnSaturation();
      cmd.Init(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, GL_RGBA,
               GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset);
    } else {
      cmd.Init(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               kWidth,
               kHeight,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               0,
               0);
    }
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_CALL(*gl_,
                TexSubImage2D(GL_TEXTURE_2D,
                              0,
                              0,
                              0,
                              kWidth,
                              kHeight - 1,
                              GL_RGBA,
                              GL_UNSIGNED_BYTE,
                              shared_memory_address_))
        .Times(1)
        .RetiresOnSaturation();
    // Consider this TexSubImage2D command part of the previous TexImage2D
    // (last GL_TRUE argument). It will be skipped if there are bugs in the
    // redefinition case.
    cmds::TexSubImage2D cmd2;
    cmd2.Init(GL_TEXTURE_2D, 0, 0, 0, kWidth, kHeight - 1, GL_RGBA,
              GL_UNSIGNED_BYTE, shared_memory_id_, kSharedMemoryOffset,
              GL_TRUE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  }
}

TEST_P(GLES2DecoderTest, TexImage2DGLError) {
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLint border = 0;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_FALSE(
      texture->GetLevelSize(GL_TEXTURE_2D, level, &width, &height, nullptr));
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              TexImage2D(target,
                         level,
                         internal_format,
                         width,
                         height,
                         border,
                         format,
                         type,
                         _))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, format, type,
           shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_FALSE(
      texture->GetLevelSize(GL_TEXTURE_2D, level, &width, &height, nullptr));
}

TEST_P(GLES2DecoderTest, CopyTexImage2DGLError) {
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLint border = 0;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_FALSE(
      texture->GetLevelSize(GL_TEXTURE_2D, level, &width, &height, nullptr));
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_OUT_OF_MEMORY))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  target, level, internal_format, 0, 0, width, height, border))
      .Times(1)
      .RetiresOnSaturation();
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_OUT_OF_MEMORY, GetGLError());
  EXPECT_FALSE(
      texture->GetLevelSize(GL_TEXTURE_2D, level, &width, &height, nullptr));
}

TEST_P(GLES2DecoderManualInitTest, CopyTexImage2DUnsizedInternalFormat) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.extensions = "GL_APPLE_texture_format_BGRA8888 GL_EXT_sRGB";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES2;
  InitDecoder(init);

  GLenum kUnsizedInternalFormats[] = {
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA,
    GL_BGRA_EXT,
    GL_LUMINANCE,
    GL_LUMINANCE_ALPHA,
    GL_SRGB,
    GL_SRGB_ALPHA,
  };
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLsizei width = 2;
  GLsizei height = 4;
  GLint border = 0;
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kNewClientId);

  TextureManager* manager = group().texture_manager();

  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(_))
      .WillRepeatedly(Return(GL_FRAMEBUFFER_COMPLETE));
  for (size_t i = 0; i < base::size(kUnsizedInternalFormats); ++i) {
    // Copy from main framebuffer to texture, using the unsized internal format.
    DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);
    GLenum internal_format = kUnsizedInternalFormats[i];
    DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
    DoCopyTexImage2D(target, level, internal_format, 0, 0, width, height, border);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    TextureRef* ref = manager->GetTexture(client_texture_id_);
    ASSERT_TRUE(ref != nullptr);
    Texture* texture = ref->texture();
    GLenum chosen_type = 0;
    GLenum chosen_internal_format = 0;
    texture->GetLevelType(target, level, &chosen_type, &chosen_internal_format);
    EXPECT_NE(0u, chosen_type);
    EXPECT_NE(0u, chosen_internal_format);

    // Attach texture to FBO, and copy into second texture.
    DoBindFramebuffer(
        GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
    DoFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           client_texture_id_,
                           kServiceTextureId,
                           0,
                           GL_NO_ERROR);
    DoBindTexture(GL_TEXTURE_2D, kNewClientId, kNewServiceId);

    bool complete =
        (DoCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    if (complete) {
      DoCopyTexImage2D(target, level, internal_format,
                       0, 0, width, height, border);
      EXPECT_EQ(GL_NO_ERROR, GetGLError());
    } else {
      cmds::CopyTexImage2D cmd;
      cmd.Init(target, level, internal_format, 0, 0, width, height);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(GL_INVALID_FRAMEBUFFER_OPERATION, GetGLError());
    }
  }
}

TEST_P(GLES2DecoderManualInitTest, CopyTexImage2DUnsizedInternalFormatES3) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.extensions = "GL_APPLE_texture_format_BGRA8888";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoder(init);

  struct UnsizedSizedInternalFormat {
    GLenum unsized;
    GLenum sized;
  };
  UnsizedSizedInternalFormat kUnsizedInternalFormats[] = {
    // GL_RED and GL_RG should not work.
    {GL_RGB, GL_RGB8},
    {GL_RGBA, GL_RGBA8},
    {GL_BGRA_EXT, GL_RGBA8},
    {GL_LUMINANCE, GL_RGB8},
    {GL_LUMINANCE_ALPHA, GL_RGBA8},
  };
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLsizei width = 2;
  GLsizei height = 4;
  GLint border = 0;
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kNewClientId);

  TextureManager* manager = group().texture_manager();

  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(_))
      .WillRepeatedly(Return(GL_FRAMEBUFFER_COMPLETE));
  for (size_t i = 0; i < base::size(kUnsizedInternalFormats); ++i) {
    // Copy from main framebuffer to texture, using the unsized internal format.
    DoBindFramebuffer(GL_FRAMEBUFFER, 0, 0);
    GLenum internal_format = kUnsizedInternalFormats[i].unsized;
    DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
    DoCopyTexImage2D(target, level, internal_format,
                     0, 0, width, height, border);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    TextureRef* ref = manager->GetTexture(client_texture_id_);
    ASSERT_TRUE(ref != nullptr);
    Texture* texture = ref->texture();
    GLenum chosen_type = 0;
    GLenum chosen_internal_format = 0;
    texture->GetLevelType(target, level, &chosen_type, &chosen_internal_format);
    EXPECT_NE(0u, chosen_type);
    EXPECT_NE(0u, chosen_internal_format);

    // Attach texture to FBO, and copy into second texture using the sized
    // internal format.
    DoBindFramebuffer(
        GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
    DoFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           client_texture_id_,
                           kServiceTextureId,
                           0,
                           GL_NO_ERROR);
    if (DoCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      continue;

    internal_format = kUnsizedInternalFormats[i].sized;
    DoBindTexture(GL_TEXTURE_2D, kNewClientId, kNewServiceId);

    bool complete =
        (DoCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    if (complete) {
      DoCopyTexImage2D(target, level, internal_format,
                       0, 0, width, height, border);
      EXPECT_EQ(GL_NO_ERROR, GetGLError());
    } else {
      cmds::CopyTexImage2D cmd;
      cmd.Init(target, level, internal_format, 0, 0, width, height);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(GL_INVALID_FRAMEBUFFER_OPERATION, GetGLError());
    }
  }
}

TEST_P(GLES3DecoderTest, CompressedTexImage3DBucket) {
  const uint32_t kBucketId = 123;
  const uint32_t kBadBucketId = 99;
  const GLenum kTarget = GL_TEXTURE_2D_ARRAY;
  const GLint kLevel = 0;
  const GLenum kInternalFormat = GL_COMPRESSED_R11_EAC;
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  const GLsizei kDepth = 4;
  const GLint kBorder = 0;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  const GLsizei kImageSize = 32;
  bucket->SetSize(kImageSize);

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);

  cmds::CompressedTexImage3DBucket cmd;
  cmd.Init(kTarget,
           kLevel,
           kInternalFormat,
           kWidth,
           kHeight,
           kDepth,
           kBadBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  cmd.Init(kTarget,
           kLevel,
           kInternalFormat,
           kWidth,
           kHeight,
           kDepth,
           kBucketId);
  EXPECT_CALL(*gl_,
              CompressedTexImage3D(kTarget, kLevel, kInternalFormat, kWidth,
                                   kHeight, kDepth, kBorder, kImageSize, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, CompressedTexImage3DBucketBucketSizeIsZero) {
  const uint32_t kBucketId = 123;
  const uint32_t kBadBucketId = 99;
  const GLenum kTarget = GL_TEXTURE_2D_ARRAY;
  const GLint kLevel = 0;
  const GLenum kInternalFormat = GL_COMPRESSED_R11_EAC;
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  const GLsizei kDepth = 4;
  const GLint kBorder = 0;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  const GLsizei kImageSize = 0;
  bucket->SetSize(kImageSize);

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);

  // Bad bucket
  cmds::CompressedTexImage3DBucket cmd;
  cmd.Init(kTarget,
           kLevel,
           kInternalFormat,
           0,
           kHeight,
           kDepth,
           kBadBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Bucket size is zero. Height or width or depth is zero too.
  cmd.Init(kTarget,
           kLevel,
           kInternalFormat,
           0,
           kHeight,
           kDepth,
           kBucketId);
  EXPECT_CALL(*gl_,
              CompressedTexImage3D(kTarget, kLevel, kInternalFormat, 0,
                                   kHeight, kDepth, kBorder, kImageSize, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Bucket size is zero. But height, width and depth are not zero.
  cmd.Init(kTarget,
           kLevel,
           kInternalFormat,
           kWidth,
           kHeight,
           kDepth,
           kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest, CompressedTexImage3DFailsOnES2) {
  const uint32_t kBucketId = 123;
  const GLenum kTarget = GL_TEXTURE_2D_ARRAY;
  const GLint kLevel = 0;
  const GLenum kInternalFormat = GL_COMPRESSED_R11_EAC;
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  const GLsizei kDepth = 4;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  const GLsizei kImageSize = 32;
  bucket->SetSize(kImageSize);

  {
    cmds::CompressedTexImage3DBucket cmd;
    cmd.Init(kTarget,
             kLevel,
             kInternalFormat,
             kWidth,
             kHeight,
             kDepth,
             kBucketId);
    EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));
  }

  {
    cmds::CompressedTexSubImage3DBucket cmd;
    cmd.Init(kTarget,
             kLevel,
             0, 0, 0,
             kWidth,
             kHeight,
             kDepth,
             kInternalFormat,
             kBucketId);
    EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));
  }
}

TEST_P(GLES2DecoderTest, CopyTexSubImage3DFailsOnES2) {
  const GLenum kTarget = GL_TEXTURE_2D_ARRAY;
  const GLint kLevel = 0;
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;

  cmds::CopyTexSubImage3D cmd;
  cmd.Init(kTarget,
           kLevel,
           0, 0, 0,
           0, 0,
           kWidth,
           kHeight);
  EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest, CopyTexSubImage3DFaiures) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 1;
  const GLint kXoffset = 0;
  const GLint kYoffset = 0;
  const GLint kZoffset = 0;
  const GLint kX = 0;
  const GLint kY = 0;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 2;
  const GLsizei kDepth = 2;

  cmds::CopyTexSubImage3D cmd;

  // No texture bound
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Incompatible format / type
  // The default format/type of default read buffer is RGB/UNSIGNED_BYTE
  const GLint kInternalFormat = GL_RGBA8;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth, 0,
               kFormat, kType, shared_memory_id_, kSharedMemoryOffset);

  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexSubImage3DCheckArgs) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 1;
  const GLint kInternalFormat = GL_RGB8;
  const GLint kXoffset = 0;
  const GLint kYoffset = 0;
  const GLint kZoffset = 0;
  const GLint kX = 0;
  const GLint kY = 0;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 2;
  const GLsizei kDepth = 2;
  const GLsizei kBorder = 0;
  const GLenum kFormat = GL_RGB;
  const GLenum kType = GL_UNSIGNED_BYTE;

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth,
               kBorder, kFormat, kType, shared_memory_id_, kSharedMemoryOffset);

  // Valid args
  EXPECT_CALL(*gl_,
              CopyTexSubImage3D(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
                                kX, kY, kWidth, kHeight))
      .Times(1)
      .RetiresOnSaturation();
  cmds::CopyTexSubImage3D cmd;
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Bad target
  cmd.Init(GL_TEXTURE_2D, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Bad Level
  cmd.Init(kTarget, -1, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(kTarget, 0, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Bad xoffest / yoffset of 3D texture
  cmd.Init(kTarget, kLevel, -1, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(kTarget, kLevel, 1, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(kTarget, kLevel, kXoffset, -1, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(kTarget, kLevel, kXoffset, 1, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Bad zoffset: zoffset specifies the layer of the 3D texture to be replaced
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, -1,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kDepth,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Bad width / height
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth + 1, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight + 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexSubImage3DFeedbackLoopSucceeds0) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kInternalFormat = GL_RGB8;
  const GLint kXoffset = 0;
  const GLint kYoffset = 0;
  const GLint kZoffset = 0;
  const GLint kX = 0;
  const GLint kY = 0;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 2;
  const GLsizei kDepth = 2;
  const GLsizei kBorder = 0;
  const GLenum kFormat = GL_RGB;
  const GLenum kType = GL_UNSIGNED_BYTE;

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);

  cmds::FramebufferTextureLayer tex_layer;
  cmds::CopyTexSubImage3D cmd;

  // The source and the target for CopyTexSubImage3D are the same 3d texture.
  // But level of 3D texture != level of read attachment in fbo.
  GLint kLevel = 0;
  GLint kLayer = 0; // kZoffset is 0
  EXPECT_CALL(*gl_, FramebufferTextureLayer(GL_FRAMEBUFFER,
                                            GL_COLOR_ATTACHMENT0,
                                            kServiceTextureId, kLevel, kLayer))
      .Times(1)
      .RetiresOnSaturation();
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth,
               kBorder, kFormat, kType, shared_memory_id_, kSharedMemoryOffset);
  tex_layer.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, client_texture_id_,
                 kLevel, kLayer);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_layer));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  kLevel = 1;
  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(_))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              CopyTexSubImage3D(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
                                kX, kY, kWidth, kHeight))
      .Times(1)
      .RetiresOnSaturation();
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth,
               kBorder, kFormat, kType, shared_memory_id_, kSharedMemoryOffset);
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexSubImage3DFeedbackLoopSucceeds1) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kInternalFormat = GL_RGB8;
  const GLint kXoffset = 0;
  const GLint kYoffset = 0;
  const GLint kZoffset = 0;
  const GLint kX = 0;
  const GLint kY = 0;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 2;
  const GLsizei kDepth = 2;
  const GLsizei kBorder = 0;
  const GLenum kFormat = GL_RGB;
  const GLenum kType = GL_UNSIGNED_BYTE;

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);

  cmds::FramebufferTextureLayer tex_layer;
  cmds::CopyTexSubImage3D cmd;

  // The source and the target for CopyTexSubImage3D are the same 3d texture.
  // But zoffset of 3D texture != layer of read attachment in fbo.
  GLint kLevel = 0;
  GLint kLayer = 1; // kZoffset is 0
  EXPECT_CALL(*gl_, FramebufferTextureLayer(GL_FRAMEBUFFER,
                                            GL_COLOR_ATTACHMENT0,
                                            kServiceTextureId, kLevel, kLayer))
      .Times(1)
      .RetiresOnSaturation();
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth,
               kBorder, kFormat, kType, shared_memory_id_, kSharedMemoryOffset);
  tex_layer.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, client_texture_id_,
                 kLevel, kLayer);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_layer));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(_))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              CopyTexSubImage3D(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
                                kX, kY, kWidth, kHeight))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexSubImage3DFeedbackLoopFails) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kInternalFormat = GL_RGB8;
  const GLint kXoffset = 0;
  const GLint kYoffset = 0;
  const GLint kZoffset = 0;
  const GLint kX = 0;
  const GLint kY = 0;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 2;
  const GLsizei kDepth = 2;
  const GLsizei kBorder = 0;
  const GLenum kFormat = GL_RGB;
  const GLenum kType = GL_UNSIGNED_BYTE;

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);

  cmds::FramebufferTextureLayer tex_layer;
  cmds::CopyTexSubImage3D cmd;

  // The source and the target for CopyTexSubImage3D are the same 3d texture.
  // And level / zoffset of 3D texture equal to level / layer of read attachment
  // in fbo.
  GLint kLevel = 0;  // This has to be base level, or fbo is incomplete.
  GLint kLayer = 0; // kZoffset is 0
  EXPECT_CALL(*gl_, FramebufferTextureLayer(GL_FRAMEBUFFER,
                                            GL_COLOR_ATTACHMENT0,
                                            kServiceTextureId, kLevel, kLayer))
      .Times(1)
      .RetiresOnSaturation();
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth,
               kBorder, kFormat, kType, shared_memory_id_, kSharedMemoryOffset);
  tex_layer.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, client_texture_id_,
                 kLevel, kLayer);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_layer));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(_))
      .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
      .RetiresOnSaturation();
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyTexSubImage3DClearTheUncleared3DTexture) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 0;
  const GLint kXoffset = 0;
  const GLint kYoffset = 0;
  const GLint kZoffset = 0;
  const GLint kX = 0;
  const GLint kY = 0;
  const GLint kInternalFormat = GL_RGB8;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 2;
  const GLsizei kDepth = 2;
  const GLenum kFormat = GL_RGB;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const uint32_t kBufferSize = kWidth * kHeight * kDepth * 4;

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth, 0,
               kFormat, kType, 0, 0);
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();

  EXPECT_FALSE(texture->SafeToRenderFrom());
  EXPECT_FALSE(texture->IsLevelCleared(kTarget, kLevel));

  // CopyTexSubImage3D will clear the uncleared texture
  GLint xoffset[] = {kXoffset};
  GLint yoffset[] = {kYoffset};
  GLint zoffset[] = {kZoffset};
  GLsizei width[] = {kWidth};
  GLsizei height[] = {kHeight};
  GLsizei depth[] = {kDepth};
  SetupClearTexture3DExpectations(kBufferSize, kTarget, kServiceTextureId,
                                  kLevel, kFormat, kType, 1, xoffset, yoffset,
                                  zoffset, width, height, depth, 0);
  EXPECT_CALL(*gl_,
              CopyTexSubImage3D(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
                                kX, kY, kWidth, kHeight))
      .Times(1)
      .RetiresOnSaturation();

  cmds::CopyTexSubImage3D cmd;
  cmd.Init(kTarget, kLevel, kXoffset, kYoffset, kZoffset,
           kX, kY, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(texture->SafeToRenderFrom());
  EXPECT_TRUE(texture->IsLevelCleared(kTarget, kLevel));
}

TEST_P(GLES3DecoderTest, CompressedTexImage3DFailsWithBadImageSize) {
  const uint32_t kBucketId = 123;
  const GLenum kTarget = GL_TEXTURE_2D_ARRAY;
  const GLint kLevel = 0;
  const GLenum kInternalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC;
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 8;
  const GLsizei kDepth = 4;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  const GLsizei kBadImageSize = 64;
  bucket->SetSize(kBadImageSize);

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);

  cmds::CompressedTexImage3DBucket cmd;
  cmd.Init(kTarget,
           kLevel,
           kInternalFormat,
           kWidth,
           kHeight,
           kDepth,
           kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderTest, CompressedTexSubImage3DFails) {
  const uint32_t kBucketId = 123;
  const GLenum kTarget = GL_TEXTURE_2D_ARRAY;
  const GLint kLevel = 0;
  const GLenum kInternalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC;
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 8;
  const GLsizei kDepth = 4;
  const GLint kBorder = 0;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  const GLsizei kImageSize = 128;
  bucket->SetSize(kImageSize);

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);

  cmds::CompressedTexImage3DBucket tex_cmd;
  tex_cmd.Init(kTarget,
               kLevel,
               kInternalFormat,
               kWidth,
               kHeight,
               kDepth,
               kBucketId);
  EXPECT_CALL(*gl_,
              CompressedTexImage3D(kTarget, kLevel, kInternalFormat, kWidth,
                                   kHeight, kDepth, kBorder, kImageSize, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  const GLint kXOffset = 0;
  const GLint kYOffset = 0;
  const GLint kZOffset = 0;
  const GLint kSubWidth = 4;
  const GLint kSubHeight = 4;
  const GLint kSubDepth = 4;
  const GLenum kFormat = kInternalFormat;
  cmds::CompressedTexSubImage3DBucket cmd;

  // Incorrect image size.
  cmd.Init(kTarget,
           kLevel,
           kXOffset,
           kYOffset,
           kZOffset,
           kSubWidth,
           kSubHeight,
           kSubDepth,
           kFormat,
           kBucketId);
  const GLsizei kBadSubImageSize = 32;
  const GLsizei kSubImageSize = 64;
  bucket->SetSize(kBadSubImageSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Incorrect format.
  const GLenum kBadFormat = GL_COMPRESSED_R11_EAC;
  cmd.Init(kTarget,
           kLevel,
           kXOffset,
           kYOffset,
           kZOffset,
           kSubWidth,
           kSubHeight,
           kSubDepth,
           kBadFormat,
           kBucketId);
  bucket->SetSize(kSubImageSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Negative offset.
  cmd.Init(kTarget,
           kLevel,
           kXOffset,
           -4,
           kZOffset,
           kSubWidth,
           kSubHeight,
           kSubDepth,
           kFormat,
           kBucketId);
  bucket->SetSize(kSubImageSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // offset + size > texture size
  cmd.Init(kTarget,
           kLevel,
           kXOffset,
           kYOffset + 8,
           kZOffset,
           kSubWidth,
           kSubHeight,
           kSubDepth,
           kFormat,
           kBucketId);
  bucket->SetSize(kSubImageSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // offset not a multiple of 4.
  cmd.Init(kTarget,
           kLevel,
           kXOffset,
           kYOffset + 1,
           kZOffset,
           kSubWidth,
           kSubHeight,
           kSubDepth,
           kFormat,
           kBucketId);
  bucket->SetSize(kSubImageSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // offset + width not a multlple of 4 .
  cmd.Init(kTarget,
           kLevel,
           kXOffset,
           kYOffset,
           kZOffset,
           kSubWidth,
           kSubHeight + 3,
           kSubDepth,
           kFormat,
           kBucketId);
  const GLsizei kSubImageSize2 = 128;
  bucket->SetSize(kSubImageSize2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Bad bucket id.
  const uint32_t kBadBucketId = 444;
  cmd.Init(kTarget,
           kLevel,
           kXOffset,
           kYOffset,
           kZOffset,
           kSubWidth,
           kSubHeight,
           kSubDepth,
           kFormat,
           kBadBucketId);
  bucket->SetSize(kSubImageSize);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Bad target
  cmd.Init(GL_RGBA,
           kLevel,
           kXOffset,
           kYOffset,
           kZOffset,
           kSubWidth,
           kSubHeight,
           kSubDepth,
           kFormat,
           kBucketId);
  bucket->SetSize(kSubImageSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  // Bad format
  cmd.Init(kTarget,
           kLevel,
           kXOffset,
           kYOffset,
           kZOffset,
           kSubWidth,
           kSubHeight,
           kSubDepth,
           GL_ONE,
           kBucketId);
  bucket->SetSize(kSubImageSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES3DecoderTest, CompressedTexSubImage3DBadSHM) {
  const uint32_t kBucketId = 123;
  const GLenum kTarget = GL_TEXTURE_2D_ARRAY;
  const GLint kLevel = 0;
  const GLenum kInternalFormat = GL_COMPRESSED_RGBA8_ETC2_EAC;
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 8;
  const GLsizei kDepth = 4;
  const GLint kBorder = 0;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  const GLsizei kImageSize = 128;
  bucket->SetSize(kImageSize);

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);

  cmds::CompressedTexImage3DBucket tex_cmd;
  tex_cmd.Init(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth,
               kBucketId);
  EXPECT_CALL(*gl_,
              CompressedTexImage3D(kTarget, kLevel, kInternalFormat, kWidth,
                                   kHeight, kDepth, kBorder, kImageSize, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  const GLint kXOffset = 0;
  const GLint kYOffset = 0;
  const GLint kZOffset = 0;
  const GLint kSubWidth = 4;
  const GLint kSubHeight = 4;
  const GLint kSubDepth = 4;
  const GLenum kFormat = kInternalFormat;
  const GLsizei kSubImageSize = 64;
  const GLsizei kBadSubImageSize = 65;
  cmds::CompressedTexSubImage3D cmd;

  // Invalid args + NULL SHM -> GL error
  cmd.Init(kTarget, kLevel, kXOffset, kYOffset, kZOffset, kSubWidth, kSubHeight,
           kSubDepth, kFormat, kBadSubImageSize, 0, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Valid args + non-empty size + NULL SHM -> command buffer error
  cmd.Init(kTarget, kLevel, kXOffset, kYOffset, kZOffset, kSubWidth, kSubHeight,
           kSubDepth, kFormat, kSubImageSize, 0, 0);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  // Valid args + non-empty size + invalid SHM -> command buffer error
  cmd.Init(kTarget, kLevel, kXOffset, kYOffset, kZOffset, kSubWidth, kSubHeight,
           kSubDepth, kFormat, kSubImageSize, 0, kSharedMemoryOffset);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));

  // Valid args + empty size + NULL SHM -> no error (NOOP).
  EXPECT_CALL(*gl_,
              CompressedTexSubImage3DNoData(kTarget, kLevel, kXOffset, kYOffset,
                                            kZOffset, 0, 0, 0, kFormat, 0))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(kTarget, kLevel, kXOffset, kYOffset, kZOffset, 0, 0, 0, kFormat, 0,
           0, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, CompressedTexImage2DBucketBucketSizeIsZero) {
  const uint32_t kBucketId = 123;
  const uint32_t kBadBucketId = 99;
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLenum kInternalFormat = GL_COMPRESSED_R11_EAC;
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  const GLint kBorder = 0;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  const GLsizei kImageSize = 0;
  bucket->SetSize(kImageSize);

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);

  // Bad bucket
  cmds::CompressedTexImage2DBucket cmd;
  cmd.Init(kTarget,
           kLevel,
           kInternalFormat,
           0,
           kHeight,
           kBadBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  // Bucket size is zero. Height or width is zero too.
  cmd.Init(kTarget,
           kLevel,
           kInternalFormat,
           0,
           kHeight,
           kBucketId);
  EXPECT_CALL(*gl_,
              CompressedTexImage2D(kTarget, kLevel, kInternalFormat, 0,
                                   kHeight, kBorder, kImageSize, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Bucket size is zero. But height and width are not zero.
  cmd.Init(kTarget,
           kLevel,
           kInternalFormat,
           kWidth,
           kHeight,
           kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, CompressedTexImage2DBucketBadBucket) {
  InitState init;
  init.extensions = "GL_EXT_texture_compression_s3tc";
  init.bind_generates_resource = true;
  InitDecoder(init);

  const uint32_t kBadBucketId = 123;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  cmds::CompressedTexImage2DBucket cmd;
  cmd.Init(GL_TEXTURE_2D,
           0,
           GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
           4,
           4,
           kBadBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmds::CompressedTexSubImage2DBucket cmd2;
  cmd2.Init(GL_TEXTURE_2D,
            0,
            0,
            0,
            4,
            4,
            GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
            kBadBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

namespace {

struct S3TCTestData {
  GLenum format;
  size_t block_size;
};

}  // anonymous namespace.

TEST_P(GLES2DecoderManualInitTest, CompressedTexImage2DS3TCWebGL) {
  InitState init;
  init.extensions = "GL_EXT_texture_compression_s3tc";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_WEBGL1;
  InitDecoder(init);
  const uint32_t kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  static const S3TCTestData test_data[] = {
      {
       GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 8,
      },
      {
       GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 8,
      },
      {
       GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 16,
      },
      {
       GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 16,
      },
  };

  for (size_t ii = 0; ii < base::size(test_data); ++ii) {
    const S3TCTestData& test = test_data[ii];
    cmds::CompressedTexImage2DBucket cmd;
    // test small width.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 2, 4, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test bad width.
    cmd.Init(GL_TEXTURE_2D, 0, test.format, 5, 4, kBucketId);
    bucket->SetSize(test.block_size * 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // test small height.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 4, 2, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test too bad height.
    cmd.Init(GL_TEXTURE_2D, 0, test.format, 4, 5, kBucketId);
    bucket->SetSize(test.block_size * 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // test small for level 0.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 1, 1, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test small for level 0.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 2, 2, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test size too large.
    cmd.Init(GL_TEXTURE_2D, 0, test.format, 4, 4, kBucketId);
    bucket->SetSize(test.block_size * 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

    // test size too small.
    cmd.Init(GL_TEXTURE_2D, 0, test.format, 4, 4, kBucketId);
    bucket->SetSize(test.block_size / 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

    // test with 3 mips.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 0, test.format, 4, 4, 0, test.block_size, kBucketId);
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 2, 2, 0, test.block_size, kBucketId);
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 2, test.format, 1, 1, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // Test a 16x16
    DoCompressedTexImage2D(GL_TEXTURE_2D,
                           0,
                           test.format,
                           16,
                           16,
                           0,
                           test.block_size * 4 * 4,
                           kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    cmds::CompressedTexSubImage2DBucket sub_cmd;
    bucket->SetSize(test.block_size);
    // Test sub image bad xoffset
    sub_cmd.Init(GL_TEXTURE_2D, 0, 1, 0, 4, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad yoffset
    sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 2, 4, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad width
    bucket->SetSize(test.block_size * 2);
    sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 5, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad height
    sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 4, 5, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad size
    bucket->SetSize(test.block_size + 1);
    sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 4, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

    for (GLint yoffset = 0; yoffset <= 8; yoffset += 4) {
      for (GLint xoffset = 0; xoffset <= 8; xoffset += 4) {
        for (GLsizei height = 4; height <= 8; height += 4) {
          for (GLsizei width = 4; width <= 8; width += 4) {
            GLsizei size = test.block_size * (width / 4) * (height / 4);
            bucket->SetSize(size);
            EXPECT_CALL(*gl_,
                        CompressedTexSubImage2D(GL_TEXTURE_2D,
                                                0,
                                                xoffset,
                                                yoffset,
                                                width,
                                                height,
                                                test.format,
                                                size,
                                                _))
                .Times(1)
                .RetiresOnSaturation();
            sub_cmd.Init(GL_TEXTURE_2D,
                         0,
                         xoffset,
                         yoffset,
                         width,
                         height,
                         test.format,
                         kBucketId);
            EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
            EXPECT_EQ(GL_NO_ERROR, GetGLError());
          }
        }
      }
    }
  }
}

TEST_P(GLES2DecoderManualInitTest, CompressedTexImage2DS3TC) {
  InitState init;
  init.extensions = "GL_EXT_texture_compression_s3tc";
  init.bind_generates_resource = true;
  InitDecoder(init);
  const uint32_t kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  static const S3TCTestData test_data[] = {
      {
       GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 8,
      },
      {
       GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 8,
      },
      {
       GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 16,
      },
      {
       GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 16,
      },
  };

  for (size_t ii = 0; ii < base::size(test_data); ++ii) {
    const S3TCTestData& test = test_data[ii];
    cmds::CompressedTexImage2DBucket cmd;
    // test small width.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 2, 4, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test non-block-size width.
    bucket->SetSize(test.block_size * 2);
    cmd.Init(GL_TEXTURE_2D, 0, test.format, 5, 4, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // test small height.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 4, 2, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test non-block-size height.
    cmd.Init(GL_TEXTURE_2D, 0, test.format, 4, 5, kBucketId);
    bucket->SetSize(test.block_size * 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // test small for level 0.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 1, 1, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test small for level 0.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 2, 2, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // test size too large.
    cmd.Init(GL_TEXTURE_2D, 0, test.format, 4, 4, kBucketId);
    bucket->SetSize(test.block_size * 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

    // test size too small.
    cmd.Init(GL_TEXTURE_2D, 0, test.format, 4, 4, kBucketId);
    bucket->SetSize(test.block_size / 2);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

    // test with 3 mips.
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 0, test.format, 4, 4, 0, test.block_size, kBucketId);
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 1, test.format, 2, 2, 0, test.block_size, kBucketId);
    DoCompressedTexImage2D(
        GL_TEXTURE_2D, 2, test.format, 1, 1, 0, test.block_size, kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // Test a 16x16
    DoCompressedTexImage2D(GL_TEXTURE_2D,
                           0,
                           test.format,
                           16,
                           16,
                           0,
                           test.block_size * 4 * 4,
                           kBucketId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    cmds::CompressedTexSubImage2DBucket sub_cmd;
    bucket->SetSize(test.block_size);
    // Test sub image bad xoffset
    sub_cmd.Init(GL_TEXTURE_2D, 0, 1, 0, 4, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad yoffset
    sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 2, 4, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad width
    bucket->SetSize(test.block_size * 2);
    sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 5, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad height
    sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 4, 5, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

    // Test sub image bad size
    bucket->SetSize(test.block_size + 1);
    sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 4, 4, test.format, kBucketId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

    for (GLint yoffset = 0; yoffset <= 8; yoffset += 4) {
      for (GLint xoffset = 0; xoffset <= 8; xoffset += 4) {
        for (GLsizei height = 4; height <= 8; height += 4) {
          for (GLsizei width = 4; width <= 8; width += 4) {
            GLsizei size = test.block_size * (width / 4) * (height / 4);
            bucket->SetSize(size);
            EXPECT_CALL(*gl_,
                        CompressedTexSubImage2D(GL_TEXTURE_2D,
                                                0,
                                                xoffset,
                                                yoffset,
                                                width,
                                                height,
                                                test.format,
                                                size,
                                                _))
                .Times(1)
                .RetiresOnSaturation();
            sub_cmd.Init(GL_TEXTURE_2D,
                         0,
                         xoffset,
                         yoffset,
                         width,
                         height,
                         test.format,
                         kBucketId);
            EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
            EXPECT_EQ(GL_NO_ERROR, GetGLError());
          }
        }
      }
    }
  }
}

TEST_P(GLES2DecoderManualInitTest, CompressedTexImage2DETC1) {
  InitState init;
  init.extensions = "GL_OES_compressed_ETC1_RGB8_texture";
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  const uint32_t kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->CreateBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  const GLenum kFormat = GL_ETC1_RGB8_OES;
  const size_t kBlockSize = 8;

  cmds::CompressedTexImage2DBucket cmd;
  // test small width.
  DoCompressedTexImage2D(GL_TEXTURE_2D, 0, kFormat, 4, 8, 0, 16, kBucketId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // test small height.
  DoCompressedTexImage2D(GL_TEXTURE_2D, 0, kFormat, 8, 4, 0, 16, kBucketId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // test size too large.
  cmd.Init(GL_TEXTURE_2D, 0, kFormat, 4, 4, kBucketId);
  bucket->SetSize(kBlockSize * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // test size too small.
  cmd.Init(GL_TEXTURE_2D, 0, kFormat, 4, 4, kBucketId);
  bucket->SetSize(kBlockSize / 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  // Test a 16x16
  DoCompressedTexImage2D(
      GL_TEXTURE_2D, 0, kFormat, 16, 16, 0, kBlockSize * 16, kBucketId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Test CompressedTexSubImage not allowed
  cmds::CompressedTexSubImage2DBucket sub_cmd;
  bucket->SetSize(kBlockSize);
  sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 4, 4, kFormat, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(sub_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Test TexSubImage not allowed for ETC1 compressed texture
  TextureRef* texture_ref = GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  GLenum type, internal_format;
  EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(kFormat, internal_format);
  cmds::TexSubImage2D texsub_cmd;
  texsub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE,
                  shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(texsub_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Test CopyTexSubImage not allowed for ETC1 compressed texture
  cmds::CopyTexSubImage2D copy_cmd;
  copy_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 4, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(copy_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, CopyTextureCHROMIUMBadTarget) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 17, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kNewClientId);

  const GLenum kBadTarget = GL_RGB;
  cmds::CopyTextureCHROMIUM cmd;
  cmd.Init(client_texture_id_, 0, kBadTarget, kNewClientId, 0, GL_RGBA,
           GL_UNSIGNED_BYTE, GL_FALSE, GL_FALSE, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, CopySubTextureCHROMIUMBadTarget) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 17, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);

  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  DoBindTexture(GL_TEXTURE_2D, kNewClientId, kNewServiceId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 17, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               0, 0);

  const GLenum kBadTarget = GL_RGB;
  cmds::CopySubTextureCHROMIUM cmd;
  cmd.Init(client_texture_id_, 0, kBadTarget, kNewClientId, 0, 1, 1, 2, 2, 3, 3,
           GL_FALSE, GL_FALSE, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, EGLImageExternalBindTexture) {
  InitState init;
  init.extensions = "GL_OES_EGL_image_external";
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_EXTERNAL_OES, kNewServiceId));
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId));
  cmds::BindTexture cmd;
  cmd.Init(GL_TEXTURE_EXTERNAL_OES, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  TextureRef* texture_ref = GetTexture(kNewClientId);
  EXPECT_TRUE(texture_ref != nullptr);
  EXPECT_TRUE(texture_ref->texture()->target() == GL_TEXTURE_EXTERNAL_OES);
}

TEST_P(GLES2DecoderManualInitTest, EGLImageExternalGetBinding) {
  InitState init;
  init.extensions = "GL_OES_EGL_image_external";
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  auto* result =
      static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_,
              GetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, result->GetData()))
      .Times(0);
  result->size = 0;
  cmds::GetIntegerv cmd;
  cmd.Init(GL_TEXTURE_BINDING_EXTERNAL_OES,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_TEXTURE_BINDING_EXTERNAL_OES),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(client_texture_id_, (uint32_t)result->GetData()[0]);
}

TEST_P(GLES2DecoderManualInitTest, EGLImageExternalTextureDefaults) {
  InitState init;
  init.extensions = "GL_OES_EGL_image_external";
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);

  TextureRef* texture_ref = GetTexture(client_texture_id_);
  EXPECT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->target() == GL_TEXTURE_EXTERNAL_OES);
  EXPECT_TRUE(texture->min_filter() == GL_LINEAR);
  EXPECT_TRUE(texture->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(texture->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_P(GLES2DecoderManualInitTest, EGLImageExternalTextureParam) {
  InitState init;
  init.extensions = "GL_OES_EGL_image_external";
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);

  EXPECT_CALL(*gl_,
              TexParameteri(
                  GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  EXPECT_CALL(
      *gl_,
      TexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_CALL(
      *gl_,
      TexParameteri(
          GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(
      *gl_,
      TexParameteri(
          GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  TextureRef* texture_ref = GetTexture(client_texture_id_);
  EXPECT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->target() == GL_TEXTURE_EXTERNAL_OES);
  EXPECT_TRUE(texture->min_filter() == GL_LINEAR);
  EXPECT_TRUE(texture->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(texture->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_P(GLES2DecoderManualInitTest, EGLImageExternalTextureParamInvalid) {
  InitState init;
  init.extensions = "GL_OES_EGL_image_external";
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);

  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_EXTERNAL_OES,
           GL_TEXTURE_MIN_FILTER,
           GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  cmd.Init(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  TextureRef* texture_ref = GetTexture(client_texture_id_);
  EXPECT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->target() == GL_TEXTURE_EXTERNAL_OES);
  EXPECT_TRUE(texture->min_filter() == GL_LINEAR);
  EXPECT_TRUE(texture->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(texture->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_P(GLES2DecoderManualInitTest, EGLImageExternalTexImage2DError) {
  InitState init;
  init.extensions = "GL_OES_EGL_image_external";
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);

  GLenum target = GL_TEXTURE_EXTERNAL_OES;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);
  ASSERT_TRUE(GetTexture(client_texture_id_) != nullptr);
  cmds::TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, format, type,
           shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  // TexImage2D is not allowed with GL_TEXTURE_EXTERNAL_OES targets.
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, DefaultTextureZero) {
  InitState init;
  InitDecoder(init);

  cmds::BindTexture cmd1;
  cmd1.Init(GL_TEXTURE_2D, 0);
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, 0));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::BindTexture cmd2;
  cmd2.Init(GL_TEXTURE_CUBE_MAP, 0);
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_CUBE_MAP, 0));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, DefaultTextureBGR) {
  InitState init;
  init.bind_generates_resource = true;
  InitDecoder(init);

  cmds::BindTexture cmd1;
  cmd1.Init(GL_TEXTURE_2D, 0);
  EXPECT_CALL(
      *gl_, BindTexture(GL_TEXTURE_2D, TestHelper::kServiceDefaultTexture2dId));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::BindTexture cmd2;
  cmd2.Init(GL_TEXTURE_CUBE_MAP, 0);
  EXPECT_CALL(*gl_,
              BindTexture(GL_TEXTURE_CUBE_MAP,
                          TestHelper::kServiceDefaultTextureCubemapId));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

// Test that default texture 0 is immutable.
TEST_P(GLES2DecoderManualInitTest, NoDefaultTexParameterf) {
  InitState init;
  InitDecoder(init);

  {
    cmds::BindTexture cmd1;
    cmd1.Init(GL_TEXTURE_2D, 0);
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, 0));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    cmds::TexParameterf cmd2;
    cmd2.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }

  {
    cmds::BindTexture cmd1;
    cmd1.Init(GL_TEXTURE_CUBE_MAP, 0);
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_CUBE_MAP, 0));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    cmds::TexParameterf cmd2;
    cmd2.Init(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }
}

TEST_P(GLES2DecoderManualInitTest, NoDefaultTexParameteri) {
  InitState init;
  InitDecoder(init);

  {
    cmds::BindTexture cmd1;
    cmd1.Init(GL_TEXTURE_2D, 0);
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, 0));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    cmds::TexParameteri cmd2;
    cmd2.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }

  {
    cmds::BindTexture cmd1;
    cmd1.Init(GL_TEXTURE_CUBE_MAP, 0);
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_CUBE_MAP, 0));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    cmds::TexParameteri cmd2;
    cmd2.Init(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }
}

TEST_P(GLES2DecoderManualInitTest, NoDefaultTexParameterfv) {
  InitState init;
  InitDecoder(init);

  {
    cmds::BindTexture cmd1;
    cmd1.Init(GL_TEXTURE_2D, 0);
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, 0));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    GLfloat data = GL_NEAREST;
    auto& cmd2 = *GetImmediateAs<cmds::TexParameterfvImmediate>();
    cmd2.Init(GL_TEXTURE_2D,
              GL_TEXTURE_MAG_FILTER,
              &data);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd2, sizeof(data)));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }

  {
    cmds::BindTexture cmd1;
    cmd1.Init(GL_TEXTURE_CUBE_MAP, 0);
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_CUBE_MAP, 0));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    GLfloat data = GL_NEAREST;
    auto& cmd2 = *GetImmediateAs<cmds::TexParameterfvImmediate>();
    cmd2.Init(GL_TEXTURE_CUBE_MAP,
              GL_TEXTURE_MAG_FILTER,
              &data);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd2, sizeof(data)));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }
}

TEST_P(GLES2DecoderManualInitTest, NoDefaultTexParameteriv) {
  InitState init;
  InitDecoder(init);

  {
    cmds::BindTexture cmd1;
    cmd1.Init(GL_TEXTURE_2D, 0);
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, 0));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    GLfloat data = GL_NEAREST;
    auto& cmd2 = *GetImmediateAs<cmds::TexParameterfvImmediate>();
    cmd2.Init(GL_TEXTURE_2D,
              GL_TEXTURE_MAG_FILTER,
              &data);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd2, sizeof(data)));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }

  {
    cmds::BindTexture cmd1;
    cmd1.Init(GL_TEXTURE_CUBE_MAP, 0);
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_CUBE_MAP, 0));
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    GLfloat data = GL_NEAREST;
    auto& cmd2 = *GetImmediateAs<cmds::TexParameterfvImmediate>();
    cmd2.Init(GL_TEXTURE_CUBE_MAP,
              GL_TEXTURE_MAG_FILTER,
              &data);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd2, sizeof(data)));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }
}

TEST_P(GLES2DecoderManualInitTest, NoDefaultTexImage2D) {
  InitState init;
  InitDecoder(init);

  cmds::BindTexture cmd1;
  cmd1.Init(GL_TEXTURE_2D, 0);
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, 0));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::TexImage2D cmd2;
  cmd2.Init(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE,
            shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, NoDefaultTexSubImage2D) {
  InitState init;
  InitDecoder(init);

  cmds::BindTexture cmd1;
  cmd1.Init(GL_TEXTURE_2D, 0);
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, 0));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::TexSubImage2D cmd2;
  cmd2.Init(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
            shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, ARBTextureRectangleBindTexture) {
  InitState init;
  init.extensions = "GL_ARB_texture_rectangle";
  init.bind_generates_resource = true;
  InitDecoder(init);
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_RECTANGLE_ARB, kNewServiceId));
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId));
  cmds::BindTexture cmd;
  cmd.Init(GL_TEXTURE_RECTANGLE_ARB, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  Texture* texture = GetTexture(kNewClientId)->texture();
  EXPECT_TRUE(texture != nullptr);
  EXPECT_TRUE(texture->target() == GL_TEXTURE_RECTANGLE_ARB);
}

TEST_P(GLES2DecoderManualInitTest, ARBTextureRectangleGetBinding) {
  InitState init;
  init.extensions = "GL_ARB_texture_rectangle";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  auto* result =
      static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_,
              GetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, result->GetData()))
      .Times(0);
  result->size = 0;
  cmds::GetIntegerv cmd;
  cmd.Init(GL_TEXTURE_BINDING_RECTANGLE_ARB,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_TEXTURE_BINDING_RECTANGLE_ARB),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(client_texture_id_, (uint32_t)result->GetData()[0]);
}

TEST_P(GLES2DecoderManualInitTest, ARBTextureRectangleTextureDefaults) {
  InitState init;
  init.extensions = "GL_ARB_texture_rectangle";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);

  Texture* texture = GetTexture(client_texture_id_)->texture();
  EXPECT_TRUE(texture != nullptr);
  EXPECT_TRUE(texture->target() == GL_TEXTURE_RECTANGLE_ARB);
  EXPECT_TRUE(texture->min_filter() == GL_LINEAR);
  EXPECT_TRUE(texture->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(texture->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_P(GLES2DecoderManualInitTest, ARBTextureRectangleTextureParam) {
  InitState init;
  init.extensions = "GL_ARB_texture_rectangle";
  init.bind_generates_resource = true;
  InitDecoder(init);

  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);

  EXPECT_CALL(*gl_,
              TexParameteri(
                  GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  EXPECT_CALL(*gl_,
              TexParameteri(
                  GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_CALL(
      *gl_,
      TexParameteri(
          GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(
      *gl_,
      TexParameteri(
          GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  Texture* texture = GetTexture(client_texture_id_)->texture();
  EXPECT_TRUE(texture != nullptr);
  EXPECT_TRUE(texture->target() == GL_TEXTURE_RECTANGLE_ARB);
  EXPECT_TRUE(texture->min_filter() == GL_LINEAR);
  EXPECT_TRUE(texture->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(texture->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_P(GLES2DecoderManualInitTest, ARBTextureRectangleTextureParamInvalid) {
  InitState init;
  init.extensions = "GL_ARB_texture_rectangle";
  init.bind_generates_resource = true;
  InitDecoder(init);

  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);

  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_RECTANGLE_ARB,
           GL_TEXTURE_MIN_FILTER,
           GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  cmd.Init(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_REPEAT);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  Texture* texture = GetTexture(client_texture_id_)->texture();
  EXPECT_TRUE(texture != nullptr);
  EXPECT_TRUE(texture->target() == GL_TEXTURE_RECTANGLE_ARB);
  EXPECT_TRUE(texture->min_filter() == GL_LINEAR);
  EXPECT_TRUE(texture->wrap_s() == GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(texture->wrap_t() == GL_CLAMP_TO_EDGE);
}

TEST_P(GLES2DecoderManualInitTest, ARBTextureRectangleTexImage2D) {
  InitState init;
  init.extensions = "GL_ARB_texture_rectangle";
  init.bind_generates_resource = true;
  InitDecoder(init);

  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;

  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);
  ASSERT_TRUE(GetTexture(client_texture_id_) != nullptr);

  cmds::TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, format, type,
           shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, ARBTextureRectangleTexImage2DInvalid) {
  InitState init;
  init.extensions = "GL_ARB_texture_rectangle";
  init.bind_generates_resource = true;
  InitDecoder(init);

  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  GLint level = 1;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 2;
  GLsizei height = 4;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;

  DoBindTexture(
      GL_TEXTURE_RECTANGLE_ARB, client_texture_id_, kServiceTextureId);
  ASSERT_TRUE(GetTexture(client_texture_id_) != nullptr);

  cmds::TexImage2D cmd;
  cmd.Init(target, level, internal_format, width, height, format, type,
           shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, TexSubImage2DClearsAfterTexImage2DNULL) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, 0, 1, 2, 1, 0);
  EXPECT_CALL(*gl_, TexSubImage2D(GL_TEXTURE_2D, 0, 0, _, _, 1, GL_RGBA,
                                  GL_UNSIGNED_BYTE, shared_memory_address_))
      .Times(2)
      .RetiresOnSaturation();
  cmds::TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_TEXTURE_2D, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  // Test if we call it again it does not clear.
  EXPECT_CALL(*gl_, TexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, 1, 1, GL_RGBA,
                                  GL_UNSIGNED_BYTE, shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, TexSubImage2DDoesNotClearAfterTexImage2DNULLThenData) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              TexSubImage2D(GL_TEXTURE_2D,
                            0,
                            1,
                            1,
                            1,
                            1,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  // Test if we call it again it does not clear.
  EXPECT_CALL(*gl_,
              TexSubImage2D(GL_TEXTURE_2D,
                            0,
                            1,
                            1,
                            1,
                            1,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, TexSubImage2DClearsAfterTexImage2DWithDataThenNULL) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // Put in data (so it should be marked as cleared)
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
  // Put in no data.
  cmds::TexImage2D tex_cmd;
  tex_cmd.Init(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  // It won't actually call TexImage2D, just mark it as uncleared.
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_cmd));
  // Next call to TexSubImage2d should clear.
  SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, 0, 1, 2, 1, 0);
  EXPECT_CALL(*gl_, TexSubImage2D(GL_TEXTURE_2D, 0, 0, _, _, 1, GL_RGBA,
                                  GL_UNSIGNED_BYTE, shared_memory_address_))
      .Times(2)
      .RetiresOnSaturation();
  cmds::TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_TEXTURE_2D, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest, ClearLevelWithBoundUnpackBuffer) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0,
               0);

  EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_ROW_LENGTH, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0))
      .Times(1)
      .RetiresOnSaturation();
  DoBindBuffer(GL_PIXEL_UNPACK_BUFFER, client_buffer_id_, kServiceBufferId);
  DoBufferData(GL_PIXEL_UNPACK_BUFFER, 8);

  EXPECT_CALL(*gl_, TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA,
                                  GL_UNSIGNED_BYTE, 0))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0,
           GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  // This TexSubImage2D can't be coalesced with the previous one, so it will
  // force a clear.
  SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, 0, 1, 2, 1, kServiceBufferId);

  EXPECT_CALL(*gl_, TexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, 1, 1, GL_RGBA,
                                  GL_UNSIGNED_BYTE, 0))
      .Times(1)
      .RetiresOnSaturation();
  cmd.Init(GL_TEXTURE_2D, 0, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0,
           GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, CopyTexImage2DMarksTextureAsCleared) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, CopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1, 1, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::CopyTexImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  EXPECT_TRUE(texture->SafeToRenderFrom());
}

TEST_P(GLES2DecoderTest, CopyTexSubImage2DTwiceMarksTextureAsCleared) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);

  // This will initialize the top part.
  {
    EXPECT_CALL(*gl_, CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 2, 1))
        .Times(1)
        .RetiresOnSaturation();
    cmds::CopyTexSubImage2D cmd;
    cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 2, 1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  // This will initialize the bottom part.
  {
    EXPECT_CALL(*gl_, CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, 0, 0, 2, 1))
        .Times(1)
        .RetiresOnSaturation();
    cmds::CopyTexSubImage2D cmd;
    cmd.Init(GL_TEXTURE_2D, 0, 0, 1, 0, 0, 2, 1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->SafeToRenderFrom());
}

TEST_P(GLES2DecoderTest, CopyTexSubImage2DTwiceClearsUnclearedTexture) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0,
               0);

  // This will initialize the top part.
  {
    EXPECT_CALL(*gl_, CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 2, 1))
        .Times(1)
        .RetiresOnSaturation();
    cmds::CopyTexSubImage2D cmd;
    cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 2, 1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, 0, 1, 2, 1, 0);

  // This will clear the bottom part as a rectangle is not sufficient to keep
  // track of the initialized area.
  {
    EXPECT_CALL(*gl_, CopyTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 0, 0, 1, 1))
        .Times(1)
        .RetiresOnSaturation();
    cmds::CopyTexSubImage2D cmd;
    cmd.Init(GL_TEXTURE_2D, 0, 1, 1, 0, 0, 1, 1);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->SafeToRenderFrom());
}

TEST_P(GLES2DecoderTest, CopyTexSubImage2DClearsUnclearedBackBufferSizedTexture) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kBackBufferWidth, kBackBufferHeight,
               0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);

  EXPECT_CALL(*gl_, CopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                                      kBackBufferWidth, kBackBufferHeight))
      .Times(1)
      .RetiresOnSaturation();
  cmds::CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 0, 0, kBackBufferWidth, kBackBufferHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->SafeToRenderFrom());
}

TEST_P(GLES2DecoderManualInitTest, CompressedImage2DMarksTextureAsCleared) {
  InitState init;
  init.extensions = "GL_EXT_texture_compression_s3tc";
  init.bind_generates_resource = true;
  InitDecoder(init);

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(
      *gl_,
      CompressedTexImage2D(
          GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0, 8, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::CompressedTexImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 8,
           shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  EXPECT_TRUE(texture_ref->texture()->SafeToRenderFrom());
}

TEST_P(GLES2DecoderTest, TextureUsageAngleExtNotEnabledByDefault) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  cmds::TexParameteri cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_TEXTURE_USAGE_ANGLE, GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, ProduceAndConsumeDirectTextureCHROMIUM) {
  Mailbox mailbox = Mailbox::Generate();

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  DoTexImage2D(
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_EQ(kServiceTextureId, texture->service_id());

  auto& produce_cmd =
      *GetImmediateAs<cmds::ProduceTextureDirectCHROMIUMImmediate>();
  produce_cmd.Init(client_texture_id_, mailbox.name);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(produce_cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Texture didn't change.
  GLsizei width;
  GLsizei height;
  GLenum type;
  GLenum internal_format;

  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr));
  EXPECT_EQ(3, width);
  EXPECT_EQ(1, height);
  EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);

  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height, nullptr));
  EXPECT_EQ(2, width);
  EXPECT_EQ(4, height);
  EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, 1, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);

  // Service ID has not changed.
  EXPECT_EQ(kServiceTextureId, texture->service_id());

  // Consume the texture into a new client ID.
  GLuint new_texture_id = kNewClientId;
  auto& consume_cmd =
      *GetImmediateAs<cmds::CreateAndConsumeTextureINTERNALImmediate>();
  consume_cmd.Init(new_texture_id, mailbox.name);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(consume_cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Make sure the new client ID is associated with the produced service ID.
  texture_ref = group().texture_manager()->GetTexture(new_texture_id);
  ASSERT_TRUE(texture_ref != nullptr);
  texture = texture_ref->texture();
  EXPECT_EQ(kServiceTextureId, texture->service_id());

  DoBindTexture(GL_TEXTURE_2D, kNewClientId, kServiceTextureId);

  // Texture is redefined.
  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr));
  EXPECT_EQ(3, width);
  EXPECT_EQ(1, height);
  EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);

  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height, nullptr));
  EXPECT_EQ(2, width);
  EXPECT_EQ(4, height);
  EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, 1, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
}

TEST_P(GLES2DecoderTest, CreateAndConsumeTextureCHROMIUMInvalidMailbox) {
  // Attempt to consume the mailbox when no texture has been produced with it.
  Mailbox mailbox = Mailbox::Generate();
  GLuint new_texture_id = kNewClientId;

  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE1)).Times(1).RetiresOnSaturation();

  auto& texture_cmd = *GetImmediateAs<cmds::ActiveTexture>();
  texture_cmd.Init(GL_TEXTURE1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(texture_cmd));

  auto& consume_cmd =
      *GetImmediateAs<cmds::CreateAndConsumeTextureINTERNALImmediate>();
  consume_cmd.Init(new_texture_id, mailbox.name);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(consume_cmd, sizeof(mailbox.name)));

  // CreateAndConsumeTexture should fail if the mailbox isn't associated with a
  // texture.
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Make sure the new client_id is associated with a texture ref even though
  // CreateAndConsumeTexture failed.
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(new_texture_id);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  // New texture should be unbound to a target.
  EXPECT_TRUE(texture->target() == GL_NONE);
  // New texture should have a valid service_id.
  EXPECT_EQ(kNewServiceId, texture->service_id());
}

TEST_P(GLES2DecoderTest, CreateAndConsumeTextureCHROMIUMInvalidTexture) {
  Mailbox mailbox = Mailbox::Generate();

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);

  auto& produce_cmd =
      *GetImmediateAs<cmds::ProduceTextureDirectCHROMIUMImmediate>();
  produce_cmd.Init(client_texture_id_, mailbox.name);
  EXPECT_EQ(
      error::kNoError,
      ExecuteImmediateCmd(produce_cmd, sizeof(mailbox.name) + sizeof(GLenum)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Attempt to consume the mailbox with an invalid texture id.
  GLuint new_texture_id = 0;
  auto& consume_cmd =
      *GetImmediateAs<cmds::CreateAndConsumeTextureINTERNALImmediate>();
  consume_cmd.Init(new_texture_id, mailbox.name);
  EXPECT_EQ(error::kNoError,
            ExecuteImmediateCmd(consume_cmd, sizeof(mailbox.name)));

  // CreateAndConsumeTexture should fail.
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

class TestSharedImageBacking : public SharedImageBacking {
 public:
  class TestSharedImageRepresentation
      : public SharedImageRepresentationGLTexture {
   public:
    TestSharedImageRepresentation(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  MemoryTypeTracker* tracker,
                                  gles2::Texture* texture)
        : SharedImageRepresentationGLTexture(manager, backing, tracker),
          texture_(texture) {}

    gles2::Texture* GetTexture() override { return texture_; }

    void set_can_access(bool can_access) { can_access_ = can_access; }
    bool BeginAccess(GLenum mode) override { return can_access_; }

   private:
    gles2::Texture* texture_;
    bool can_access_ = true;
  };

  TestSharedImageBacking(const Mailbox& mailbox,
                         viz::ResourceFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         uint32_t usage,
                         MemoryTypeTracker* memory_tracker,
                         GLuint texture_id)
      : SharedImageBacking(mailbox,
                           format,
                           size,
                           color_space,
                           usage,
                           0 /* estimated_size */,
                           false /* is_thread_safe */) {
    texture_ = new gles2::Texture(texture_id);
    texture_->SetLightweightRef();
  }

  bool IsCleared() const override { return false; }

  void SetCleared() override {}

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override {
    DCHECK(!in_fence);
  }

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    return false;
  }

  void Destroy() override {
    texture_->RemoveLightweightRef(have_context());
    texture_ = nullptr;
  }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {}

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override {
    return std::make_unique<TestSharedImageRepresentation>(manager, this,
                                                           tracker, texture_);
  }

 private:
  gles2::Texture* texture_;
};

TEST_P(GLES2DecoderTest, CreateAndTexStorage2DSharedImageCHROMIUM) {
  MemoryTypeTracker memory_tracker(memory_tracker_.get());
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          std::make_unique<TestSharedImageBacking>(
              mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
              gfx::ColorSpace(), 0, &memory_tracker, kNewServiceId),
          &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, GL_NONE, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Make sure the new client ID is associated with the produced service ID.
  auto* texture_ref = group().texture_manager()->GetTexture(kNewClientId);
  ASSERT_NE(texture_ref, nullptr);
  EXPECT_EQ(kNewServiceId, texture_ref->texture()->service_id());

  // Delete the texture and make sure it is no longer accessible.
  DoDeleteTexture(kNewClientId, kNewServiceId);
  texture_ref = group().texture_manager()->GetTexture(kNewClientId);
  EXPECT_EQ(texture_ref, nullptr);

  shared_image.reset();
}

TEST_P(GLES2DecoderTest,
       CreateAndTexStorage2DSharedImageCHROMIUMInvalidMailbox) {
  MemoryTypeTracker memory_tracker(memory_tracker_.get());

  // Attempt to use an invalid mailbox.
  Mailbox mailbox;
  // We will generate a new texture.
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, GL_NONE, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));

  // CreateAndTexStorage2DSharedImage should fail if the mailbox is invalid.
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Make sure the new client_id is associated with a texture ref even though
  // CreateAndTexStorage2DSharedImage failed.
  TextureRef* texture_ref = group().texture_manager()->GetTexture(kNewClientId);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  // New texture should be unbound to a target.
  EXPECT_TRUE(texture->target() == GL_NONE);
  // New texture should have a valid service_id.
  EXPECT_EQ(kNewServiceId, texture->service_id());
}

TEST_P(GLES2DecoderTest,
       CreateAndTexStorage2DSharedImageCHROMIUMPreexistingTexture) {
  // Try to create a mailbox with kNewClientId.
  MemoryTypeTracker memory_tracker(memory_tracker_.get());
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          std::make_unique<TestSharedImageBacking>(
              mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
              gfx::ColorSpace(), 0, &memory_tracker, kNewServiceId),
          &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(client_texture_id_, GL_NONE, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));

  // CreateAndTexStorage2DSharedImage should fail.
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // We delete a texture when calling |shared_image| reset().
  EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(kNewServiceId)));
  shared_image.reset();
}

TEST_P(GLES2DecoderTest, BeginEndSharedImageAccessCRHOMIUM) {
  MemoryTypeTracker memory_tracker(memory_tracker_.get());
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          std::make_unique<TestSharedImageBacking>(
              mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
              gfx::ColorSpace(), 0, &memory_tracker, kNewServiceId),
          &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, GL_NONE, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Begin/end read access for the created image.
  cmds::BeginSharedImageAccessDirectCHROMIUM read_access_cmd;
  read_access_cmd.Init(kNewClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(read_access_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  cmds::EndSharedImageAccessDirectCHROMIUM read_end_cmd;
  read_end_cmd.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(read_end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Begin/end read/write access for the created image.
  cmds::BeginSharedImageAccessDirectCHROMIUM readwrite_access_cmd;
  readwrite_access_cmd.Init(kNewClientId,
                            GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(readwrite_access_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  cmds::EndSharedImageAccessDirectCHROMIUM readwrite_end_cmd;
  readwrite_end_cmd.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(readwrite_end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Cleanup
  DoDeleteTexture(kNewClientId, kNewServiceId);
  shared_image.reset();
}

TEST_P(GLES2DecoderTest, BeginSharedImageAccessDirectCHROMIUMInvalidMode) {
  // Try to begin access with an invalid mode.
  cmds::BeginSharedImageAccessDirectCHROMIUM bad_mode_access_cmd;
  bad_mode_access_cmd.Init(client_texture_id_, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bad_mode_access_cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, BeginSharedImageAccessDirectCHROMIUMNotSharedImage) {
  // Try to begin access with a texture that is not a shared image.
  cmds::BeginSharedImageAccessDirectCHROMIUM not_shared_image_access_cmd;
  not_shared_image_access_cmd.Init(
      client_texture_id_, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(not_shared_image_access_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, BeginSharedImageAccessDirectCHROMIUMCantBeginAccess) {
  // Create a shared image.
  MemoryTypeTracker memory_tracker(memory_tracker_.get());
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          std::make_unique<TestSharedImageBacking>(
              mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
              gfx::ColorSpace(), 0, &memory_tracker, kNewServiceId),
          &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, GL_NONE, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try to begin access with a shared image representation that fails
  // BeginAccess.
  auto* texture_ref = group().texture_manager()->GetTexture(kNewClientId);
  ASSERT_NE(texture_ref, nullptr);
  ASSERT_NE(texture_ref->shared_image(), nullptr);
  static_cast<TestSharedImageBacking::TestSharedImageRepresentation*>(
      texture_ref->shared_image())
      ->set_can_access(false);
  cmds::BeginSharedImageAccessDirectCHROMIUM read_access_cmd;
  read_access_cmd.Init(kNewClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(read_access_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Cleanup
  DoDeleteTexture(kNewClientId, kNewServiceId);
  shared_image.reset();
}

TEST_P(GLES2DecoderTest, EndSharedImageAccessDirectCHROMIUMNotSharedImage) {
  // Try to end access with a texture that is not a shared image.
  cmds::EndSharedImageAccessDirectCHROMIUM not_shared_image_end_cmd;
  not_shared_image_end_cmd.Init(client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(not_shared_image_end_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, DepthTextureBadArgs) {
  InitState init;
  init.extensions = "GL_ANGLE_depth_texture";
  init.gl_version = "OpenGL ES 2.0";
  init.has_depth = true;
  init.has_stencil = true;
  init.request_depth = true;
  init.request_stencil = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  // Check trying to upload data fails.
  cmds::TexImage2D tex_cmd;
  tex_cmd.Init(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1, 1, GL_DEPTH_COMPONENT,
               GL_UNSIGNED_INT, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // Try level > 0.
  tex_cmd.Init(GL_TEXTURE_2D,
               1,
               GL_DEPTH_COMPONENT,
               1,
               1,
               GL_DEPTH_COMPONENT,
               GL_UNSIGNED_INT,
               0,
               0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // Make a 1 pixel depth texture.
  DoTexImage2D(GL_TEXTURE_2D,
               0,
               GL_DEPTH_COMPONENT,
               1,
               1,
               0,
               GL_DEPTH_COMPONENT,
               GL_UNSIGNED_INT,
               0,
               0);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Check that trying to update it fails.
  cmds::TexSubImage2D tex_sub_cmd;
  tex_sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_DEPTH_COMPONENT,
                   GL_UNSIGNED_INT, shared_memory_id_, kSharedMemoryOffset,
                   GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_sub_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Check that trying to CopyTexImage2D fails
  cmds::CopyTexImage2D copy_tex_cmd;
  copy_tex_cmd.Init(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 0, 0, 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(copy_tex_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Check that trying to CopyTexSubImage2D fails
  cmds::CopyTexSubImage2D copy_sub_cmd;
  copy_sub_cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(copy_sub_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, GenerateMipmapDepthTexture) {
  InitState init;
  init.extensions = "GL_ANGLE_depth_texture";
  init.gl_version = "OpenGL ES 2.0";
  init.has_depth = true;
  init.has_stencil = true;
  init.request_depth = true;
  init.request_stencil = true;
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D,
               0,
               GL_DEPTH_COMPONENT,
               2,
               2,
               0,
               GL_DEPTH_COMPONENT,
               GL_UNSIGNED_INT,
               0,
               0);
  cmds::GenerateMipmap cmd;
  cmd.Init(GL_TEXTURE_2D);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, BindTexImage2DCHROMIUM) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_EQ(kServiceTextureId, texture->service_id());

  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  GetImageManagerForTest()->AddImage(image.get(), 1);
  EXPECT_FALSE(GetImageManagerForTest()->LookupImage(1) == nullptr);

  GLsizei width;
  GLsizei height;
  GLenum type;
  GLenum internal_format;

  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr));
  EXPECT_EQ(3, width);
  EXPECT_EQ(1, height);
  EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == nullptr);

  // Bind image to texture.
  // ScopedGLErrorSuppressor calls GetError on its constructor and destructor.
  DoBindTexImage2DCHROMIUM(GL_TEXTURE_2D, 1);
  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr));
  // Image should now be set.
  EXPECT_FALSE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == nullptr);

  // Define new texture image.
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr));
  // Image should no longer be set.
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == nullptr);
}

TEST_P(GLES2DecoderTest, BindTexImage2DCHROMIUMCubeMapNotAllowed) {
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  GetImageManagerForTest()->AddImage(image.get(), 1);
  DoBindTexture(GL_TEXTURE_CUBE_MAP, client_texture_id_, kServiceTextureId);

  cmds::BindTexImage2DCHROMIUM bind_tex_image_2d_cmd;
  bind_tex_image_2d_cmd.Init(GL_TEXTURE_CUBE_MAP, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bind_tex_image_2d_cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest,
       BindTexImage2DWithInternalformatCHROMIUMBadInternalFormat) {
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  GetImageManagerForTest()->AddImage(image.get(), 1);
  DoBindTexture(GL_TEXTURE_CUBE_MAP, client_texture_id_, kServiceTextureId);

  cmds::BindTexImage2DWithInternalformatCHROMIUM bind_tex_image_2d_cmd;
  bind_tex_image_2d_cmd.Init(GL_TEXTURE_2D, GL_BACK, 1);  // Invalid enum
  EXPECT_EQ(error::kNoError, ExecuteCmd(bind_tex_image_2d_cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, OrphanGLImageWithTexImage2D) {
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  GetImageManagerForTest()->AddImage(image.get(), 1);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  DoBindTexImage2DCHROMIUM(GL_TEXTURE_2D, 1);

  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();

  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == image.get());
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == nullptr);
}

TEST_P(GLES2DecoderTest, GLImageAttachedAfterSubTexImage2D) {
  // Specifically tests that TexSubImage2D is not optimized to TexImage2D
  // in the presence of image attachments.
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  GetImageManagerForTest()->AddImage(image.get(), 1);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLint xoffset = 0;
  GLint yoffset = 0;
  GLsizei width = 1;
  GLsizei height = 1;
  GLint border = 0;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset;
  GLboolean internal = 0;

  // Define texture first.
  DoTexImage2D(target, level, format, width, height, border, format, type,
               pixels_shm_id, pixels_shm_offset);

  // Bind texture to GLImage.
  DoBindTexImage2DCHROMIUM(GL_TEXTURE_2D, 1);

  // Check binding.
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == image.get());

  // TexSubImage2D should not unbind GLImage.
  EXPECT_CALL(*gl_, TexSubImage2D(target, level, xoffset, yoffset, width,
                                  height, format, type, _))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexSubImage2D tex_sub_image_2d_cmd;
  tex_sub_image_2d_cmd.Init(target, level, xoffset, yoffset, width, height,
                            format, type, pixels_shm_id, pixels_shm_offset,
                            internal);
  EXPECT_EQ(error::kNoError, ExecuteCmd(tex_sub_image_2d_cmd));
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == image.get());
}

TEST_P(GLES2DecoderTest, GLImageAttachedAfterClearLevel) {
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  GetImageManagerForTest()->AddImage(image.get(), 1);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLint xoffset = 0;
  GLint yoffset = 0;
  GLsizei width = 1;
  GLsizei height = 1;
  GLint border = 0;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset;

  // Define texture first.
  DoTexImage2D(target, level, format, width, height, border, format, type,
               pixels_shm_id, pixels_shm_offset);

  // Bind texture to GLImage.
  DoBindTexImage2DCHROMIUM(GL_TEXTURE_2D, 1);

  // Check binding.
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == image.get());

  // ClearLevel should use glTexSubImage2D to avoid unbinding GLImage.
  SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                GL_TEXTURE_2D, GL_TEXTURE_2D, level, format,
                                type, xoffset, yoffset, width, height, 0);
  GetDecoder()->ClearLevel(texture, target, level, format, type, 0, 0, width,
                           height);
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == image.get());
}

TEST_P(GLES2DecoderTest, ReleaseTexImage2DCHROMIUM) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 3, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0);
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_EQ(kServiceTextureId, texture->service_id());

  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  GetImageManagerForTest()->AddImage(image.get(), 1);
  EXPECT_FALSE(GetImageManagerForTest()->LookupImage(1) == nullptr);

  GLsizei width;
  GLsizei height;
  GLenum type;
  GLenum internal_format;

  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr));
  EXPECT_EQ(3, width);
  EXPECT_EQ(1, height);
  EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, 0, &type, &internal_format));
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), internal_format);
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == nullptr);

  // Bind image to texture.
  // ScopedGLErrorSuppressor calls GetError on its constructor and destructor.
  DoBindTexImage2DCHROMIUM(GL_TEXTURE_2D, 1);
  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr));
  // Image should now be set.
  EXPECT_FALSE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == nullptr);

  // Release image from texture.
  // ScopedGLErrorSuppressor calls GetError on its constructor and destructor.
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::ReleaseTexImage2DCHROMIUM release_tex_image_2d_cmd;
  release_tex_image_2d_cmd.Init(GL_TEXTURE_2D, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(release_tex_image_2d_cmd));
  EXPECT_TRUE(
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr));
  // Image should no longer be set.
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == nullptr);
}

class MockGLImage : public gl::GLImage {
 public:
  MockGLImage() = default;

  // Overridden from gl::GLImage:
  MOCK_METHOD0(GetSize, gfx::Size());
  MOCK_METHOD0(GetInternalFormat, unsigned());
  MOCK_METHOD0(GetDataFormat, unsigned());
  MOCK_METHOD0(GetDataType, unsigned());
  MOCK_METHOD0(ShouldBindOrCopy, gl::GLImage::BindOrCopy());
  MOCK_METHOD1(BindTexImage, bool(unsigned));
  MOCK_METHOD1(ReleaseTexImage, void(unsigned));
  MOCK_METHOD1(CopyTexImage, bool(unsigned));
  MOCK_METHOD3(CopyTexSubImage,
               bool(unsigned, const gfx::Point&, const gfx::Rect&));
  MOCK_METHOD7(ScheduleOverlayPlane,
               bool(gfx::AcceleratedWidget,
                    int,
                    gfx::OverlayTransform,
                    const gfx::Rect&,
                    const gfx::RectF&,
                    bool,
                    std::unique_ptr<gfx::GpuFence> gpu_fence));
  MOCK_METHOD1(SetColorSpace, void(const gfx::ColorSpace&));
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD3(OnMemoryDump,
               void(base::trace_event::ProcessMemoryDump*,
                    uint64_t,
                    const std::string&));

 protected:
  ~MockGLImage() override = default;
};

TEST_P(GLES2DecoderWithShaderTest, CopyTexImage) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);

  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_EQ(kServiceTextureId, texture->service_id());

  const int32_t kImageId = 1;
  scoped_refptr<MockGLImage> image(new MockGLImage);
  GetImageManagerForTest()->AddImage(image.get(), kImageId);

  // Bind image to texture.
  EXPECT_CALL(*image.get(), ShouldBindOrCopy())
      .WillRepeatedly(Return(gl::GLImage::COPY));
  EXPECT_CALL(*image.get(), BindTexImage(GL_TEXTURE_2D))
      .Times(0)
      .RetiresOnSaturation();
  EXPECT_CALL(*image.get(), GetSize())
      .Times(1)
      .WillOnce(Return(gfx::Size(1, 1)))
      .RetiresOnSaturation();
  EXPECT_CALL(*image.get(), GetInternalFormat())
      .Times(1)
      .WillOnce(Return(GL_RGBA))
      .RetiresOnSaturation();
  EXPECT_CALL(*image.get(), GetDataFormat())
      .Times(1)
      .WillOnce(Return(GL_RGBA))
      .RetiresOnSaturation();
  EXPECT_CALL(*image.get(), GetDataType())
      .Times(1)
      .WillOnce(Return(GL_UNSIGNED_BYTE))
      .RetiresOnSaturation();
  // ScopedGLErrorSuppressor calls GetError on its constructor and destructor.
  DoBindTexImage2DCHROMIUM(GL_TEXTURE_2D, kImageId);
  Mock::VerifyAndClearExpectations(gl_.get());

  EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE1)).Times(1).RetiresOnSaturation();
  cmds::ActiveTexture texture_cmd;
  texture_cmd.Init(GL_TEXTURE1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(texture_cmd));
  Mock::VerifyAndClearExpectations(gl_.get());

  AddExpectationsForSimulatedAttrib0(kNumVertices, 0);
  SetupExpectationsForApplyingDefaultDirtyState();

  // ScopedGLErrorSuppressor calls GetError on its constructor and destructor.
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kServiceTextureId))
      .Times(1)
      .RetiresOnSaturation();
  {
    InSequence seq;
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE0))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*image.get(), BindTexImage(GL_TEXTURE_2D))
        .Times(0)
        .RetiresOnSaturation();
    EXPECT_CALL(*image.get(), CopyTexImage(GL_TEXTURE_2D))
        .Times(1)
        .WillOnce(Return(true))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, DrawArrays(GL_TRIANGLES, 0, kNumVertices))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, ActiveTexture(GL_TEXTURE1))
        .Times(1)
        .RetiresOnSaturation();
  }

  cmds::DrawArrays cmd;
  cmd.Init(GL_TRIANGLES, 0, kNumVertices);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);

  Mock::VerifyAndClearExpectations(gl_.get());
  // Re-bind image to texture.
  cmds::ReleaseTexImage2DCHROMIUM release_tex_image_2d_cmd;
  release_tex_image_2d_cmd.Init(GL_TEXTURE_2D, kImageId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(release_tex_image_2d_cmd));
  EXPECT_CALL(*image.get(), BindTexImage(GL_TEXTURE_2D))
      .Times(0)
      .RetiresOnSaturation();
  EXPECT_CALL(*image.get(), GetSize())
      .Times(1)
      .WillOnce(Return(gfx::Size(1, 1)))
      .RetiresOnSaturation();
  EXPECT_CALL(*image.get(), GetInternalFormat())
      .Times(1)
      .WillOnce(Return(GL_RGBA))
      .RetiresOnSaturation();
  EXPECT_CALL(*image.get(), GetDataFormat())
      .Times(1)
      .WillOnce(Return(GL_RGBA))
      .RetiresOnSaturation();
  EXPECT_CALL(*image.get(), GetDataType())
      .Times(1)
      .WillOnce(Return(GL_UNSIGNED_BYTE))
      .RetiresOnSaturation();
  DoBindTexImage2DCHROMIUM(GL_TEXTURE_2D, kImageId);

  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  // ScopedGLErrorSuppressor calls GetError on its constructor and destructor.
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kServiceTextureId))
      .Times(2)
      .RetiresOnSaturation();
  EXPECT_CALL(*image.get(), CopyTexImage(GL_TEXTURE_2D))
      .Times(1)
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              FramebufferTexture2DEXT(GL_FRAMEBUFFER,
                                      GL_COLOR_ATTACHMENT0,
                                      GL_TEXTURE_2D,
                                      kServiceTextureId,
                                      0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  cmds::FramebufferTexture2D fbtex_cmd;
  fbtex_cmd.Init(GL_FRAMEBUFFER,
                 GL_COLOR_ATTACHMENT0,
                 GL_TEXTURE_2D,
                 client_texture_id_,
                 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(fbtex_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  image = nullptr;
}

TEST_P(GLES2DecoderManualInitTest, DrawWithGLImageExternal) {
  InitState init;
  init.extensions = "GL_OES_EGL_image_external";
  init.gl_version = "OpenGL ES 2.0";
  init.has_alpha = true;
  init.has_depth = true;
  init.request_alpha = true;
  init.request_depth = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  TextureRef* texture_ref = GetTexture(client_texture_id_);
  scoped_refptr<MockGLImage> image(new MockGLImage);
  group().texture_manager()->SetTarget(texture_ref, GL_TEXTURE_EXTERNAL_OES);
  group().texture_manager()->SetLevelInfo(texture_ref, GL_TEXTURE_EXTERNAL_OES,
                                          0, GL_RGBA, 1, 1, 1, 0, GL_RGBA,
                                          GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  group().texture_manager()->SetLevelImage(texture_ref, GL_TEXTURE_EXTERNAL_OES,
                                           0, image.get(), Texture::BOUND);

  DoBindTexture(GL_TEXTURE_EXTERNAL_OES, client_texture_id_, kServiceTextureId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  SetupSamplerExternalProgram();
  SetupIndexBuffer();
  AddExpectationsForSimulatedAttrib0(kMaxValidIndex + 1, 0);
  SetupExpectationsForApplyingDefaultDirtyState();
  EXPECT_TRUE(group().texture_manager()->CanRender(texture_ref));

  InSequence s;
  EXPECT_CALL(*gl_, DrawElements(_, _, _, _)).Times(1);
  cmds::DrawElements cmd;
  cmd.Init(GL_TRIANGLES,
           kValidIndexRangeCount,
           GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, TexImage2DFloatOnGLES2) {
  InitState init;
  init.extensions = "GL_OES_texture_float";
  init.gl_version = "OpenGL ES 2.0";
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 17, 0, GL_RGBA, GL_FLOAT, 0, 0);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 16, 17, 0, GL_RGB, GL_FLOAT, 0, 0);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_LUMINANCE, 16, 17, 0, GL_LUMINANCE, GL_FLOAT, 0, 0);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 16, 17, 0, GL_ALPHA, GL_FLOAT, 0, 0);
  DoTexImage2D(GL_TEXTURE_2D,
               0,
               GL_LUMINANCE_ALPHA,
               16,
               17,
               0,
               GL_LUMINANCE_ALPHA,
               GL_FLOAT,
               0,
               0);
}

TEST_P(GLES2DecoderManualInitTest, TexImage2DFloatOnGLES3) {
  InitState init;
  init.extensions = "GL_OES_texture_float GL_EXT_color_buffer_float";
  init.gl_version = "OpenGL ES 3.0";
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2DConvertInternalFormat(GL_TEXTURE_2D, 0, GL_RGBA, 16, 17, 0,
                                    GL_RGBA, GL_FLOAT, 0, 0, GL_RGBA32F);
  DoTexImage2DConvertInternalFormat(GL_TEXTURE_2D, 0, GL_RGB, 16, 17, 0, GL_RGB,
                                    GL_FLOAT, 0, 0, GL_RGB32F);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA32F, 16, 17, 0, GL_RGBA, GL_FLOAT, 0, 0);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_LUMINANCE, 16, 17, 0, GL_LUMINANCE, GL_FLOAT, 0, 0);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 16, 17, 0, GL_ALPHA, GL_FLOAT, 0, 0);
  DoTexImage2D(GL_TEXTURE_2D,
               0,
               GL_LUMINANCE_ALPHA,
               16,
               17,
               0,
               GL_LUMINANCE_ALPHA,
               GL_FLOAT,
               0,
               0);
}

TEST_P(GLES2DecoderManualInitTest, TexSubImage2DFloatOnGLES3) {
  InitState init;
  init.extensions = "GL_OES_texture_float GL_EXT_color_buffer_float";
  init.gl_version = "OpenGL ES 3.0";
  InitDecoder(init);
  const int kWidth = 8;
  const int kHeight = 4;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA32F,
               kWidth,
               kHeight,
               0,
               GL_RGBA,
               GL_FLOAT,
               0,
               0);
  EXPECT_CALL(*gl_,
              TexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA32F,
                         kWidth,
                         kHeight,
                         0,
                         GL_RGBA,
                         GL_FLOAT,
                         shared_memory_address_))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, 0, 0, kWidth, kHeight, GL_RGBA, GL_FLOAT,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, TexSubImage2DFloatDoesClearOnGLES3) {
  InitState init;
  init.extensions = "GL_OES_texture_float GL_EXT_color_buffer_float";
  init.gl_version = "OpenGL ES 3.0";
  InitDecoder(init);
  const int kWidth = 8;
  const int kHeight = 4;
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA32F,
               kWidth,
               kHeight,
               0,
               GL_RGBA,
               GL_FLOAT,
               0,
               0);
  SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                GL_FLOAT, 0, kHeight - 1, kWidth, 1, 0);
  EXPECT_CALL(*gl_, TexSubImage2D(GL_TEXTURE_2D, 0, 0, _, _, _, GL_RGBA,
                                  GL_FLOAT, shared_memory_address_))
      .Times(2)
      .RetiresOnSaturation();
  cmds::TexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, 0, 0, kWidth, kHeight - 1, GL_RGBA, GL_FLOAT,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_TEXTURE_2D, 0, 0, kHeight - 1, kWidth - 1, 1, GL_RGBA, GL_FLOAT,
           shared_memory_id_, kSharedMemoryOffset, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, TexImage2DFloatConvertsFormatDesktop) {
  InitState init;
  init.extensions = "GL_ARB_texture_float";
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA32F, 16, 17, 0, GL_RGBA, GL_FLOAT, 0, 0);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, 16, 17, 0, GL_RGB, GL_FLOAT, 0, 0);
  DoTexImage2DConvertInternalFormat(GL_TEXTURE_2D,
                                    0,
                                    GL_RGBA,
                                    16,
                                    17,
                                    0,
                                    GL_RGBA,
                                    GL_FLOAT,
                                    0,
                                    0,
                                    GL_RGBA32F_ARB);
  DoTexImage2DConvertInternalFormat(GL_TEXTURE_2D,
                                    0,
                                    GL_RGB,
                                    16,
                                    17,
                                    0,
                                    GL_RGB,
                                    GL_FLOAT,
                                    0,
                                    0,
                                    GL_RGB32F_ARB);
  DoTexImage2DConvertInternalFormat(GL_TEXTURE_2D,
                                    0,
                                    GL_LUMINANCE,
                                    16,
                                    17,
                                    0,
                                    GL_LUMINANCE,
                                    GL_FLOAT,
                                    0,
                                    0,
                                    GL_LUMINANCE32F_ARB);
  DoTexImage2DConvertInternalFormat(GL_TEXTURE_2D,
                                    0,
                                    GL_ALPHA,
                                    16,
                                    17,
                                    0,
                                    GL_ALPHA,
                                    GL_FLOAT,
                                    0,
                                    0,
                                    GL_ALPHA32F_ARB);
  DoTexImage2DConvertInternalFormat(GL_TEXTURE_2D,
                                    0,
                                    GL_LUMINANCE_ALPHA,
                                    16,
                                    17,
                                    0,
                                    GL_LUMINANCE_ALPHA,
                                    GL_FLOAT,
                                    0,
                                    0,
                                    GL_LUMINANCE_ALPHA32F_ARB);
}

TEST_P(GLES2DecoderManualInitTest, TexImage2Dnorm16OnGLES2) {
  InitState init;
  init.extensions = "GL_EXT_texture_norm16";
  init.gl_version = "OpenGL ES 2.0";
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 16, 17, 0, GL_RED, GL_UNSIGNED_SHORT,
               0, 0);
}

TEST_P(GLES2DecoderManualInitTest, TexImage2Dnorm16OnGLES3) {
  InitState init;
  init.extensions = "GL_EXT_texture_norm16";
  init.gl_version = "OpenGL ES 3.0";
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_R16_EXT, 16, 17, 0, GL_RED,
               GL_UNSIGNED_SHORT, 0, 0);
}

class GLES2DecoderCompressedFormatsTest : public GLES2DecoderManualInitTest {
 public:
  GLES2DecoderCompressedFormatsTest() = default;

  static bool ValueInArray(GLint value, GLint* array, GLint count) {
    for (GLint ii = 0; ii < count; ++ii) {
      if (array[ii] == value) {
        return true;
      }
    }
    return false;
  }

  void CheckFormats(const char* extension, const GLenum* formats, int count) {
    InitState init;
    init.extensions = extension;
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
    cmds::GetIntegerv cmd;
    result->size = 0;
    EXPECT_CALL(*gl_, GetIntegerv(_, _)).Times(0).RetiresOnSaturation();
    cmd.Init(GL_NUM_COMPRESSED_TEXTURE_FORMATS,
             shared_memory_id_,
             shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(1, result->GetNumResults());
    GLint num_formats = result->GetData()[0];
    EXPECT_EQ(count, num_formats);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    result->size = 0;
    cmd.Init(GL_COMPRESSED_TEXTURE_FORMATS,
             shared_memory_id_,
             shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(num_formats, result->GetNumResults());

    for (int i = 0; i < count; ++i) {
      EXPECT_TRUE(
          ValueInArray(formats[i], result->GetData(), result->GetNumResults()));
    }

    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderCompressedFormatsTest,
                         ::testing::Bool());

TEST_P(GLES2DecoderCompressedFormatsTest, GetCompressedTextureFormatsS3TC) {
  const GLenum formats[] = {
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT};
  CheckFormats("GL_EXT_texture_compression_s3tc", formats, 4);
}

TEST_P(GLES2DecoderCompressedFormatsTest, GetCompressedTextureFormatsATC) {
  const GLenum formats[] = {GL_ATC_RGB_AMD, GL_ATC_RGBA_EXPLICIT_ALPHA_AMD,
                            GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD};
  CheckFormats("GL_AMD_compressed_ATC_texture", formats, 3);
}

TEST_P(GLES2DecoderCompressedFormatsTest, GetCompressedTextureFormatsPVRTC) {
  const GLenum formats[] = {
      GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG, GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,
      GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG};
  CheckFormats("GL_IMG_texture_compression_pvrtc", formats, 4);
}

TEST_P(GLES2DecoderCompressedFormatsTest, GetCompressedTextureFormatsETC1) {
  const GLenum formats[] = {GL_ETC1_RGB8_OES};
  CheckFormats("GL_OES_compressed_ETC1_RGB8_texture", formats, 1);
}

TEST_P(GLES2DecoderCompressedFormatsTest, GetCompressedTextureFormatsASTC) {
  const GLenum formats[] = {
      GL_COMPRESSED_RGBA_ASTC_4x4_KHR,
      GL_COMPRESSED_RGBA_ASTC_5x4_KHR,
      GL_COMPRESSED_RGBA_ASTC_5x5_KHR,
      GL_COMPRESSED_RGBA_ASTC_6x5_KHR,
      GL_COMPRESSED_RGBA_ASTC_6x6_KHR,
      GL_COMPRESSED_RGBA_ASTC_8x5_KHR,
      GL_COMPRESSED_RGBA_ASTC_8x6_KHR,
      GL_COMPRESSED_RGBA_ASTC_8x8_KHR,
      GL_COMPRESSED_RGBA_ASTC_10x5_KHR,
      GL_COMPRESSED_RGBA_ASTC_10x6_KHR,
      GL_COMPRESSED_RGBA_ASTC_10x8_KHR,
      GL_COMPRESSED_RGBA_ASTC_10x10_KHR,
      GL_COMPRESSED_RGBA_ASTC_12x10_KHR,
      GL_COMPRESSED_RGBA_ASTC_12x12_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR,
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR};
  CheckFormats("GL_KHR_texture_compression_astc_ldr", formats, 28);
}

TEST_P(GLES2DecoderManualInitTest, GetNoCompressedTextureFormats) {
  InitState init;
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
  cmds::GetIntegerv cmd;
  result->size = 0;
  EXPECT_CALL(*gl_, GetIntegerv(_, _)).Times(0).RetiresOnSaturation();
  cmd.Init(GL_NUM_COMPRESSED_TEXTURE_FORMATS,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  GLint num_formats = result->GetData()[0];
  EXPECT_EQ(0, num_formats);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  result->size = 0;
  cmd.Init(
      GL_COMPRESSED_TEXTURE_FORMATS, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(num_formats, result->GetNumResults());

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, TexStorageInvalidLevels) {
  InitState init;
  init.gl_version = "OpenGL 4.2";
  init.extensions = "GL_ARB_texture_rectangle GL_ARB_texture_storage";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_RECTANGLE_ARB, client_texture_id_,
                kServiceTextureId);
  cmds::TexStorage2DEXT cmd;
  cmd.Init(GL_TEXTURE_RECTANGLE_ARB, 2, GL_RGBA8, 4, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, TexStorageInvalidSize) {
  InitState init;
  init.gl_version = "OpenGL 4.2";
  init.extensions = "GL_ARB_texture_storage";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  {
    cmds::TexStorage2DEXT cmd;
    cmd.Init(GL_TEXTURE_2D, 1, GL_RGBA8, 0, 4);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }
  {
    cmds::TexStorage2DEXT cmd;
    cmd.Init(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 0);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }
  {
    cmds::TexStorage2DEXT cmd;
    cmd.Init(GL_TEXTURE_2D, 1, GL_RGBA8, 0, 0);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  }
}

class GLES2DecoderTexStorageFormatAndTypeTest
    : public GLES2DecoderManualInitTest {
 public:
  GLES2DecoderTexStorageFormatAndTypeTest() = default;

  void DoTexStorageFormatAndType(const InitState& init,
                                 GLenum format,
                                 GLenum adjusted_internal_format) {
    GLsizei kWidth = 512;
    GLsizei kHeight = 512;
    // Purposely set kLevels to be smaller than 10 = log2(512) + 1.
    GLsizei kLevels = 5;
    InitDecoder(init);
    DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
    EXPECT_CALL(
        *gl_, TexStorage2DEXT(GL_TEXTURE_2D, kLevels, format, kWidth, kHeight))
        .Times(1)
        .RetiresOnSaturation();
    cmds::TexStorage2DEXT cmd;
    cmd.Init(GL_TEXTURE_2D, kLevels, format, kWidth, kHeight);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    TextureRef* texture_ref =
        group().texture_manager()->GetTexture(client_texture_id_);
    Texture* texture = texture_ref->texture();
    for (GLsizei ii = 0; ii < kLevels; ++ii) {
      GLenum type = 0, internal_format = 0;
      GLsizei level_width = 0, level_height = 0;
      EXPECT_TRUE(texture->GetLevelType(GL_TEXTURE_2D, static_cast<GLint>(ii),
                                        &type, &internal_format));
      EXPECT_EQ(static_cast<GLenum>(adjusted_internal_format), internal_format);
      EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
      EXPECT_TRUE(texture->GetLevelSize(GL_TEXTURE_2D, static_cast<GLint>(ii),
                                        &level_width, &level_height, nullptr));
      EXPECT_EQ(kWidth >> ii, level_width);
      EXPECT_EQ(kHeight >> ii, level_height);
    }
    EXPECT_TRUE(texture->texture_complete());
  }
};

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderTexStorageFormatAndTypeTest,
                         ::testing::Bool());

TEST_P(GLES2DecoderTexStorageFormatAndTypeTest, ES2) {
  InitState init;
  init.gl_version = "OpenGL ES 2.0";
  init.extensions = "GL_ARB_texture_storage";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES2;
  DoTexStorageFormatAndType(init, GL_RGBA8_OES, GL_RGBA);
}

TEST_P(GLES2DecoderTexStorageFormatAndTypeTest, WebGL1) {
  InitState init;
  init.gl_version = "OpenGL ES 2.0";
  init.extensions = "GL_ARB_texture_storage";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_WEBGL1;
  DoTexStorageFormatAndType(init, GL_RGBA8_OES, GL_RGBA);
}

TEST_P(GLES2DecoderTexStorageFormatAndTypeTest, ES3) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  DoTexStorageFormatAndType(init, GL_RGBA8, GL_RGBA8);
}

TEST_P(GLES2DecoderTexStorageFormatAndTypeTest, WebGL2) {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_WEBGL2;
  DoTexStorageFormatAndType(init, GL_RGBA8, GL_RGBA8);
}

TEST_P(GLES3DecoderTest, TexStorage3DValidArgs) {
  DoBindTexture(GL_TEXTURE_3D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, TexStorage3D(GL_TEXTURE_3D, 2, GL_RGB565, 4, 5, 6))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexStorage3D cmd;
  cmd.Init(GL_TEXTURE_3D, 2, GL_RGB565, 4, 5, 6);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, TexImage3DValidArgs) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 2;
  const GLint kInternalFormat = GL_RGBA8;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 2;
  const GLsizei kDepth = 2;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth, 0,
               kFormat, kType, shared_memory_id_, kSharedMemoryOffset);
}

TEST_P(GLES3DecoderTest, ClearLevel3DSingleCall) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 0;
  const GLint kInternalFormat = GL_RGBA8;
  const GLsizei kWidth = 2;
  const GLsizei kHeight = 2;
  const GLsizei kDepth = 2;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const uint32_t kBufferSize = kWidth * kHeight * kDepth * 4;

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth, 0,
               kFormat, kType, 0, 0);
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();

  // It takes 1 call to clear the entire 3D texture.
  GLint xoffset[] = {0};
  GLint yoffset[] = {0};
  GLint zoffset[] = {0};
  GLsizei width[] = {kWidth};
  GLsizei height[] = {kHeight};
  GLsizei depth[] = {kDepth};
  SetupClearTexture3DExpectations(kBufferSize, kTarget, kServiceTextureId,
                                  kLevel, kFormat, kType, 1, xoffset, yoffset,
                                  zoffset, width, height, depth, 0);

  EXPECT_TRUE(decoder_->ClearLevel3D(
      texture, kTarget, kLevel, kFormat, kType, kWidth, kHeight, kDepth));
}

TEST_P(GLES3DecoderTest, ClearLevel3DMultipleLayersPerCall) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 0;
  const GLint kInternalFormat = GL_RGBA8;
  const GLsizei kWidth = 512;
  const GLsizei kHeight = 512;
  const GLsizei kDepth = 7;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const uint32_t kBufferSize = 1024 * 1024 * 2;  // Max buffer size per call.

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth, 0,
               kFormat, kType, 0, 0);
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();

  // It takes 4 calls to clear the 3D texture, each clears 2/2/2/1 layers.
  GLint xoffset[] = {0, 0, 0, 0};
  GLint yoffset[] = {0, 0, 0, 0};
  GLint zoffset[] = {0, 2, 4, 6};
  GLsizei width[] = {kWidth, kWidth, kWidth, kWidth};
  GLsizei height[] = {kHeight, kHeight, kHeight, kHeight};
  GLsizei depth[] = {2, 2, 2, 1};
  SetupClearTexture3DExpectations(kBufferSize, kTarget, kServiceTextureId,
                                  kLevel, kFormat, kType, 4, xoffset, yoffset,
                                  zoffset, width, height, depth, 0);

  EXPECT_TRUE(decoder_->ClearLevel3D(
      texture, kTarget, kLevel, kFormat, kType, kWidth, kHeight, kDepth));
}

TEST_P(GLES3DecoderTest, ClearLevel3DMultipleCallsPerLayer) {
  const GLenum kTarget = GL_TEXTURE_3D;
  const GLint kLevel = 0;
  const GLint kInternalFormat = GL_RGBA8;
  const GLsizei kWidth = 1024;
  const GLsizei kHeight = 1000;
  const GLsizei kDepth = 2;
  const GLenum kFormat = GL_RGBA;
  const GLenum kType = GL_UNSIGNED_BYTE;
  const uint32_t kBufferSize = 1024 * 1024 * 2;  // Max buffer size per call.

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  DoTexImage3D(kTarget, kLevel, kInternalFormat, kWidth, kHeight, kDepth, 0,
               kFormat, kType, 0, 0);
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();

  // It takes two calls to clear one layer of the 3D texture, each clears
  // 512/488 rows.
  GLint xoffset[] = {0, 0, 0, 0};
  GLint yoffset[] = {0, 512, 0, 512};
  GLint zoffset[] = {0, 0, 1, 1};
  GLsizei width[] = {kWidth, kWidth, kWidth, kWidth};
  GLsizei height[] = {512, 488, 512, 488};
  GLsizei depth[] = {1, 1, 1, 1};
  SetupClearTexture3DExpectations(kBufferSize, kTarget, kServiceTextureId,
                                  kLevel, kFormat, kType, 4, xoffset, yoffset,
                                  zoffset, width, height, depth, 0);

  EXPECT_TRUE(decoder_->ClearLevel3D(
      texture, kTarget, kLevel, kFormat, kType, kWidth, kHeight, kDepth));
}

TEST_P(GLES3DecoderTest, BindSamplerInvalidUnit) {
  cmds::BindSampler cmd;
  cmd.Init(kNumTextureUnits, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());

  cmd.Init(kNumTextureUnits, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

// Test that copyTexImage2D uses the emulated internal format, rather than the
// real internal format.
TEST_P(GLES2DecoderWithShaderTest, CHROMIUMImageEmulatingRGB) {
  const GLuint kFBOClientTextureId = 4100;
  const GLuint kFBOServiceTextureId = 4101;
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLsizei width = 1;
  GLsizei height = 1;

  // Generate the source framebuffer.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kFBOServiceTextureId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kFBOClientTextureId);
  DoBindFramebuffer(GL_FRAMEBUFFER, client_framebuffer_id_,
                    kServiceFramebufferId);

  GLenum destination_texture_formats[] = {GL_RGBA, GL_RGB};
  for (int use_rgb_emulation = 0; use_rgb_emulation < 2; ++ use_rgb_emulation) {
    for (size_t destination_texture_index = 0; destination_texture_index < 2;
         ++destination_texture_index) {
      // Generate and bind the source image. Making a new image for each set of
      // parameters is easier than trying to reuse images. Obviously there's no
      // performance penalty.
      int image_id = use_rgb_emulation * 2 + destination_texture_index;
      scoped_refptr<gl::GLImage> image;
      if (use_rgb_emulation)
        image = new EmulatingRGBImageStub;
      else
        image = new gl::GLImageStub;
      GetImageManagerForTest()->AddImage(image.get(), image_id);
      EXPECT_FALSE(GetImageManagerForTest()->LookupImage(image_id) == nullptr);
      DoBindTexture(GL_TEXTURE_2D, kFBOClientTextureId, kFBOServiceTextureId);

      DoBindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);
      DoFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target,
                             kFBOClientTextureId, kFBOServiceTextureId, level,
                             GL_NO_ERROR);

      GLenum destination_texture_format =
          destination_texture_formats[destination_texture_index];
      bool should_succeed =
          !use_rgb_emulation || (destination_texture_format == GL_RGB);
      if (should_succeed) {
        EXPECT_CALL(*gl_, GetError())
            .WillOnce(Return(GL_NO_ERROR))
            .RetiresOnSaturation();
        EXPECT_CALL(*gl_,
                    CopyTexImage2D(GL_TEXTURE_2D, 0, destination_texture_format,
                                   0, 0, 1, 1, 0))
            .Times(1)
            .RetiresOnSaturation();
        EXPECT_CALL(*gl_, GetError())
            .WillOnce(Return(GL_NO_ERROR))
            .RetiresOnSaturation();
      }

      if (destination_texture_index == 0 || !framebuffer_completeness_cache()) {
        EXPECT_CALL(*gl_, CheckFramebufferStatusEXT(GL_FRAMEBUFFER))
            .WillOnce(Return(GL_FRAMEBUFFER_COMPLETE))
            .RetiresOnSaturation();
      }

      DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
      cmds::CopyTexImage2D cmd;
      cmd.Init(target, level, destination_texture_format, 0, 0, width, height);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      GLenum expectation = should_succeed ? GL_NO_ERROR : GL_INVALID_OPERATION;
      EXPECT_EQ(expectation, static_cast<GLenum>(GetGLError()));
    }
  }
}

TEST_P(GLES2DecoderTest, BindTextureValidArgs) {
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kServiceTextureId))
      .Times(1)
      .RetiresOnSaturation();
  if (!feature_info()->gl_version_info().BehavesLikeGLES() &&
      feature_info()->gl_version_info().IsAtLeastGL(3, 2)) {
    EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D,
                                    GL_DEPTH_TEXTURE_MODE,
                                    GL_RED))
        .Times(1)
        .RetiresOnSaturation();
  }
  cmds::BindTexture cmd;
  cmd.Init(GL_TEXTURE_2D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, BindTextureValidArgsNewId) {
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kNewServiceId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId));
  if (!feature_info()->gl_version_info().BehavesLikeGLES() &&
      feature_info()->gl_version_info().IsAtLeastGL(3, 2)) {
    EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D,
                                    GL_DEPTH_TEXTURE_MODE,
                                    GL_RED))
        .Times(1)
        .RetiresOnSaturation();
  }
  cmds::BindTexture cmd;
  cmd.Init(GL_TEXTURE_2D, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetTexture(kNewClientId) != nullptr);
}

TEST_P(GLES2DecoderTest, BindTextureInvalidArgs) {
  EXPECT_CALL(*gl_, BindTexture(_, _)).Times(0);
  cmds::BindTexture cmd;
  cmd.Init(GL_TEXTURE_1D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());

  cmd.Init(GL_TEXTURE_3D, client_texture_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES3DecoderTest, TexSwizzleAllowed) {
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLenum kSwizzleParam = GL_TEXTURE_SWIZZLE_R;
  const GLenum kSwizzleValue = GL_BLUE;
  const GLenum kInvalidSwizzleValue = GL_RG;

  {
    EXPECT_CALL(*gl_, TexParameteri(kTarget, kSwizzleParam, kSwizzleValue));
    cmds::TexParameteri cmd;
    cmd.Init(kTarget, kSwizzleParam, kSwizzleValue);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  {
    cmds::TexParameteri cmd;
    cmd.Init(kTarget, kSwizzleParam, kInvalidSwizzleValue);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  }

  {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    auto* result =
        static_cast<cmds::GetTexParameteriv::Result*>(shared_memory_address_);
    result->size = 0;
    cmds::GetTexParameteriv cmd;
    cmd.Init(kTarget, kSwizzleParam, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(kSwizzleParam),
              result->GetNumResults());
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_EQ(kSwizzleValue, static_cast<GLenum>(result->GetData()[0]));
  }
}

TEST_P(WebGL2DecoderTest, TexSwizzleDisabled) {
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLenum kSwizzleParam = GL_TEXTURE_SWIZZLE_R;
  const GLenum kSwizzleValue = GL_BLUE;

  {
    cmds::TexParameteri cmd;
    cmd.Init(kTarget, kSwizzleParam, kSwizzleValue);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  }

  {
    auto* result =
        static_cast<cmds::GetTexParameteriv::Result*>(shared_memory_address_);
    result->size = 0;
    cmds::GetTexParameteriv cmd;
    cmd.Init(kTarget, kSwizzleParam, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  }
}

TEST_P(GLES2DecoderTest, TestInitDiscardableTexture) {
  EXPECT_EQ(0u, group().discardable_manager()->NumCacheEntriesForTesting());
  DoInitializeDiscardableTextureCHROMIUM(client_texture_id_);
  EXPECT_EQ(1u, group().discardable_manager()->NumCacheEntriesForTesting());
}

TEST_P(GLES2DecoderTest, TestInitInvalidDiscardableTexture) {
  EXPECT_EQ(0u, group().discardable_manager()->NumCacheEntriesForTesting());
  DoInitializeDiscardableTextureCHROMIUM(0);
  EXPECT_EQ(0u, group().discardable_manager()->NumCacheEntriesForTesting());
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest, TestInitDiscardableTextureWithInvalidArguments) {
  EXPECT_EQ(0u, group().discardable_manager()->NumCacheEntriesForTesting());

  // Manually initialize an init command with an invalid buffer.
  {
    cmds::InitializeDiscardableTextureCHROMIUM cmd;
    cmd.Init(client_texture_id_, kInvalidSharedMemoryId, 0);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
    EXPECT_EQ(0u, group().discardable_manager()->NumCacheEntriesForTesting());
  }

  // Manually initialize an init command with an out of bounds offset.
  {
    cmds::InitializeDiscardableTextureCHROMIUM cmd;
    cmd.Init(client_texture_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
    EXPECT_EQ(0u, group().discardable_manager()->NumCacheEntriesForTesting());
  }

  // Manually initialize an init command with a non-atomic32-aligned offset.
  {
    cmds::InitializeDiscardableTextureCHROMIUM cmd;
    cmd.Init(client_texture_id_, shared_memory_id_, 1);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
    EXPECT_EQ(0u, group().discardable_manager()->NumCacheEntriesForTesting());
  }
}

TEST_P(GLES2DecoderTest, TestUnlockDiscardableTexture) {
  const ContextGroup& context_group = group();
  EXPECT_EQ(0u,
            context_group.discardable_manager()->NumCacheEntriesForTesting());
  DoInitializeDiscardableTextureCHROMIUM(client_texture_id_);
  EXPECT_TRUE(context_group.discardable_manager()->IsEntryLockedForTesting(
      client_texture_id_, context_group.texture_manager()));
  DoUnlockDiscardableTextureCHROMIUM(client_texture_id_);
  EXPECT_FALSE(context_group.discardable_manager()->IsEntryLockedForTesting(
      client_texture_id_, context_group.texture_manager()));
}

TEST_P(GLES2DecoderTest, TestDeleteDiscardableTexture) {
  EXPECT_EQ(0u, group().discardable_manager()->NumCacheEntriesForTesting());
  DoInitializeDiscardableTextureCHROMIUM(client_texture_id_);
  EXPECT_EQ(1u, group().discardable_manager()->NumCacheEntriesForTesting());
  DoDeleteTexture(client_texture_id_, kServiceTextureId);
  EXPECT_EQ(0u, group().discardable_manager()->NumCacheEntriesForTesting());
}

TEST_P(GLES2DecoderManualInitTest,
       TestDiscardableTextureUnusableWhileUnlocked) {
  InitState init;
  init.bind_generates_resource = false;
  InitDecoder(init);

  DoInitializeDiscardableTextureCHROMIUM(client_texture_id_);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, 0)).RetiresOnSaturation();
  DoUnlockDiscardableTextureCHROMIUM(client_texture_id_);
  {
    // Avoid DoBindTexture, as we expect failure.
    cmds::BindTexture cmd;
    cmd.Init(GL_TEXTURE_2D, client_texture_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  DoLockDiscardableTextureCHROMIUM(client_texture_id_);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, TestDiscardableTextureBindGeneratesUnlocked) {
  DoInitializeDiscardableTextureCHROMIUM(client_texture_id_);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Unlock will unbind the texture.
  EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, 0)).RetiresOnSaturation();
  DoUnlockDiscardableTextureCHROMIUM(client_texture_id_);

  // At this point, the texture is unlocked and unusable. Bind will generate a
  // new resource.
  EXPECT_CALL(*gl_, GenTextures(_, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kNewServiceId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Re-locking should delete the previous resource (preserving the generated
  // one).
  EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(kServiceTextureId)))
      .RetiresOnSaturation();
  DoLockDiscardableTextureCHROMIUM(client_texture_id_);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kNewServiceId);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, CopySubTextureCHROMIUMTwiceClearsUnclearedTexture) {
  // Create uninitialized source texture.
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  GenHelper<cmds::GenTexturesImmediate>(kNewClientId);
  DoBindTexture(GL_TEXTURE_2D, kNewClientId, kNewServiceId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0,
               0);

  // Create uninitialized dest texture.
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0,
               0);

  // This will write the top half of the destination.
  {
    // Source is undefined, so first call to CopySubTexture will clear the
    // source.
    SetupClearTextureExpectations(kNewServiceId, kServiceTextureId,
                                  GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, 0, 0, 2, 2, 0);
    cmds::CopySubTextureCHROMIUM cmd;
    cmd.Init(kNewClientId /* source_id */, 0 /* source_level */,
             GL_TEXTURE_2D /* dest_target */, client_texture_id_ /* dest_id */,
             0 /* dest_level */, 0 /* xoffset */, 0 /* yoffset */, 0 /* x */,
             0 /* y */, 2 /* width */, 1 /* height */,
             false /* unpack_flip_y */, false /* unpack_premultiply_alpha */,
             false /* unpack_unmultiply_alpha */);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  // This will write the bottom right pixel of the destination.
  {
    // This will clear the bottom part of destination as a rectangle is not
    // sufficient to keep track of the initialized area.
    SetupClearTextureExpectations(kServiceTextureId, kServiceTextureId,
                                  GL_TEXTURE_2D, GL_TEXTURE_2D, 0, GL_RGBA,
                                  GL_UNSIGNED_BYTE, 0, 1, 2, 1, 0);
    cmds::CopySubTextureCHROMIUM cmd;
    cmd.Init(kNewClientId /* source_id */, 0 /* source_level */,
             GL_TEXTURE_2D /* dest_target */, client_texture_id_ /* dest_id */,
             0 /* dest_level */, 1 /* xoffset */, 1 /* yoffset */, 0 /* x */,
             0 /* y */, 1 /* width */, 1 /* height */,
             false /* unpack_flip_y */, false /* unpack_premultiply_alpha */,
             false /* unpack_unmultiply_alpha */);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();
  EXPECT_TRUE(texture->SafeToRenderFrom());
}

TEST_P(GLES3DecoderTest, ImmutableTextureBaseLevelMaxLevelClamping) {
  GLenum kTarget = GL_TEXTURE_3D;
  GLint kBaseLevel = 1416354905;
  GLint kMaxLevel = 800;
  GLsizei kLevels = 4;
  GLenum kInternalFormat = GL_R8;
  GLsizei kWidth = 20;
  GLsizei kHeight = 20;
  GLsizei kDepth = 20;
  GLint kClampedBaseLevel = kLevels - 1;
  GLint kClampedMaxLevel = kLevels - 1;

  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);
  TextureRef* texture_ref =
      group().texture_manager()->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);
  Texture* texture = texture_ref->texture();

  // Before TexStorage3D call, base/max levels are not clamped.
  {
    EXPECT_CALL(*gl_, TexParameteri(kTarget, GL_TEXTURE_BASE_LEVEL, kBaseLevel))
        .Times(1)
        .RetiresOnSaturation();
    cmds::TexParameteri cmd;
    cmd.Init(kTarget, GL_TEXTURE_BASE_LEVEL, kBaseLevel);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  {
    EXPECT_CALL(*gl_, TexParameteri(kTarget, GL_TEXTURE_MAX_LEVEL, kMaxLevel))
        .Times(1)
        .RetiresOnSaturation();
    cmds::TexParameteri cmd;
    cmd.Init(kTarget, GL_TEXTURE_MAX_LEVEL, kMaxLevel);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(kBaseLevel, texture->base_level());
  EXPECT_EQ(kMaxLevel, texture->max_level());

  {
    EXPECT_CALL(*gl_, TexStorage3D(kTarget, kLevels, kInternalFormat, kWidth,
                                   kHeight, kDepth))
        .Times(1)
        .RetiresOnSaturation();
    cmds::TexStorage3D cmd;
    cmd.Init(kTarget, kLevels, kInternalFormat, kWidth, kHeight, kDepth);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
  EXPECT_EQ(kClampedBaseLevel, texture->base_level());
  EXPECT_EQ(kClampedMaxLevel, texture->max_level());

  GLint kNewBaseLevel = 827344;
  GLint kNewMaxLevel = 17619;
  // After TexStorage3D call, base/max levels are clamped.
  {
    EXPECT_CALL(
        *gl_, TexParameteri(kTarget, GL_TEXTURE_BASE_LEVEL, kClampedBaseLevel))
        .Times(1)
        .RetiresOnSaturation();
    cmds::TexParameteri cmd;
    cmd.Init(kTarget, GL_TEXTURE_BASE_LEVEL, kNewBaseLevel);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  {
    EXPECT_CALL(*gl_,
                TexParameteri(kTarget, GL_TEXTURE_MAX_LEVEL, kClampedMaxLevel))
        .Times(1)
        .RetiresOnSaturation();
    cmds::TexParameteri cmd;
    cmd.Init(kTarget, GL_TEXTURE_MAX_LEVEL, kNewMaxLevel);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(kClampedBaseLevel, texture->base_level());
  EXPECT_EQ(kClampedMaxLevel, texture->max_level());

  // GetTexParameteriv still returns unclamped values.
  {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    auto* result =
        static_cast<cmds::GetTexParameteriv::Result*>(shared_memory_address_);
    result->size = 0;
    cmds::GetTexParameteriv cmd;
    cmd.Init(kTarget, GL_TEXTURE_BASE_LEVEL, shared_memory_id_,
             shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(kNewBaseLevel, static_cast<GLint>(result->GetData()[0]));
  }
  {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    auto* result =
        static_cast<cmds::GetTexParameteriv::Result*>(shared_memory_address_);
    result->size = 0;
    cmds::GetTexParameteriv cmd;
    cmd.Init(kTarget, GL_TEXTURE_MAX_LEVEL, shared_memory_id_,
             shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(kNewMaxLevel, static_cast<GLint>(result->GetData()[0]));
  }
}

TEST_P(GLES3DecoderTest, ClearRenderableLevelsWithOutOfRangeBaseLevel) {
  // Regression test for https://crbug.com/983938
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0,
               0);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  TextureManager* manager = group().texture_manager();
  TextureRef* texture_ref = manager->GetTexture(client_texture_id_);
  ASSERT_TRUE(texture_ref != nullptr);

  {
    EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 55));
    cmds::TexParameteri cmd;
    cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 55);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  // The following call will trigger out-of-bounds access in asan build
  // without fixing the bug.
  manager->ClearRenderableLevels(GetDecoder(), texture_ref);
}

// TODO(gman): Complete this test.
// TEST_P(GLES2DecoderTest, CompressedTexImage2DGLError) {
// }

// TODO(gman): CompressedTexImage2D

// TODO(gman): CompressedTexImage2DImmediate

// TODO(gman): CompressedTexSubImage2DImmediate

// TODO(gman): TexImage2D

// TODO(gman): TexImage2DImmediate

// TODO(gman): TexSubImage2DImmediate

}  // namespace gles2
}  // namespace gpu
