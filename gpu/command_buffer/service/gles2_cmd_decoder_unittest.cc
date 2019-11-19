// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_stream_texture_image_stub.h"
#include "gpu/command_buffer/service/gl_surface_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/validating_abstract_texture_impl.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_image_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gpu_timing_fake.h"
#include "ui/gl/scoped_make_current.h"

#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::AtLeast;
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

void GLES2DecoderRGBBackbufferTest::SetUp() {
  InitState init;
  init.bind_generates_resource = true;
  InitDecoder(init);
  SetupDefaultProgram();
}

void GLES2DecoderManualInitTest::EnableDisableTest(GLenum cap,
                                                   bool enable,
                                                   bool expect_set) {
  if (expect_set) {
    SetupExpectationsForEnableDisable(cap, enable);
  }
  if (enable) {
    cmds::Enable cmd;
    cmd.Init(cap);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  } else {
    cmds::Disable cmd;
    cmd.Init(cap);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
}


TEST_P(GLES3DecoderTest, Basic) {
  // Make sure the setup is correct for ES3.
  EXPECT_TRUE(feature_info()->IsWebGL2OrES3Context());
  EXPECT_FALSE(feature_info()->IsWebGLContext());
  EXPECT_TRUE(feature_info()->validators()->texture_bind_target.IsValid(
      GL_TEXTURE_3D));
}

TEST_P(WebGL2DecoderTest, Basic) {
  // Make sure the setup is correct for WebGL2.
  EXPECT_TRUE(feature_info()->IsWebGL2OrES3Context());
  EXPECT_TRUE(feature_info()->IsWebGLContext());
  EXPECT_TRUE(feature_info()->validators()->texture_bind_target.IsValid(
      GL_TEXTURE_3D));
}

TEST_P(GLES2DecoderTest, InvalidCommand) {
  cmd::Noop cmd;
  cmd.header.Init(gles2::kNumCommands, 1);
  EXPECT_EQ(error::kUnknownCommand, ExecuteImmediateCmd(cmd, 0));
}

TEST_P(GLES2DecoderTest, GetIntegervCached) {
  struct TestInfo {
    GLenum pname;
    GLint expected;
  };
  TestInfo tests[] = {
      {
       GL_MAX_TEXTURE_SIZE, TestHelper::kMaxTextureSize,
      },
      {
       GL_MAX_CUBE_MAP_TEXTURE_SIZE, TestHelper::kMaxCubeMapTextureSize,
      },
      {
       GL_MAX_RENDERBUFFER_SIZE, TestHelper::kMaxRenderbufferSize,
      },
  };
  for (size_t ii = 0; ii < sizeof(tests) / sizeof(tests[0]); ++ii) {
    const TestInfo& test = tests[ii];
    auto* result =
        static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetIntegerv(test.pname, _)).Times(0);
    result->size = 0;
    cmds::GetIntegerv cmd2;
    cmd2.Init(test.pname, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
    EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(test.pname),
              result->GetNumResults());
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_EQ(test.expected, result->GetData()[0]);
  }
}

TEST_P(GLES2DecoderWithShaderTest, GetMaxValueInBufferCHROMIUM) {
  SetupIndexBuffer();
  auto* result = static_cast<cmds::GetMaxValueInBufferCHROMIUM::Result*>(
      shared_memory_address_);
  *result = 0;

  cmds::GetMaxValueInBufferCHROMIUM cmd;
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(7u, *result);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT, kValidIndexRangeStart * 2, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(100u, *result);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmd.Init(kInvalidClientId, kValidIndexRangeCount, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  cmd.Init(client_element_buffer_id_, kOutOfRangeIndexRangeEnd,
           GL_UNSIGNED_SHORT, kValidIndexRangeStart * 2, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT, kOutOfRangeIndexRangeEnd * 2, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT, kValidIndexRangeStart * 2, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_buffer_id_, kValidIndexRangeCount + 1, GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  cmd.Init(client_element_buffer_id_,
           kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT,
           kValidIndexRangeStart * 2,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_element_buffer_id_, kValidIndexRangeCount + 1,
           GL_UNSIGNED_SHORT, kValidIndexRangeStart * 2, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, IsBuffer) {
  EXPECT_FALSE(DoIsBuffer(client_buffer_id_));
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  EXPECT_TRUE(DoIsBuffer(client_buffer_id_));
  EXPECT_CALL(*gl_, BindBuffer(GL_ARRAY_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  DoDeleteBuffer(client_buffer_id_, kServiceBufferId);
  EXPECT_FALSE(DoIsBuffer(client_buffer_id_));
}

TEST_P(GLES2DecoderTest, IsFramebuffer) {
  EXPECT_FALSE(DoIsFramebuffer(client_framebuffer_id_));
  DoBindFramebuffer(
      GL_FRAMEBUFFER, client_framebuffer_id_, kServiceFramebufferId);
  EXPECT_TRUE(DoIsFramebuffer(client_framebuffer_id_));
  DoDeleteFramebuffer(client_framebuffer_id_,
                      kServiceFramebufferId,
                      true,
                      GL_FRAMEBUFFER,
                      0,
                      true,
                      GL_FRAMEBUFFER,
                      0);
  EXPECT_FALSE(DoIsFramebuffer(client_framebuffer_id_));
}

TEST_P(GLES2DecoderTest, IsProgram) {
  // IsProgram is true as soon as the program is created.
  EXPECT_TRUE(DoIsProgram(client_program_id_));
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  DoDeleteProgram(client_program_id_, kServiceProgramId);
  EXPECT_FALSE(DoIsProgram(client_program_id_));
}

TEST_P(GLES2DecoderTest, IsRenderbuffer) {
  EXPECT_FALSE(DoIsRenderbuffer(client_renderbuffer_id_));
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  EXPECT_TRUE(DoIsRenderbuffer(client_renderbuffer_id_));
  DoDeleteRenderbuffer(client_renderbuffer_id_, kServiceRenderbufferId);
  EXPECT_FALSE(DoIsRenderbuffer(client_renderbuffer_id_));
}

TEST_P(GLES2DecoderTest, IsShader) {
  // IsShader is true as soon as the program is created.
  EXPECT_TRUE(DoIsShader(client_shader_id_));
  DoDeleteShader(client_shader_id_, kServiceShaderId);
  EXPECT_FALSE(DoIsShader(client_shader_id_));
}

TEST_P(GLES2DecoderTest, IsTexture) {
  EXPECT_FALSE(DoIsTexture(client_texture_id_));
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_TRUE(DoIsTexture(client_texture_id_));
  DoDeleteTexture(client_texture_id_, kServiceTextureId);
  EXPECT_FALSE(DoIsTexture(client_texture_id_));
}

TEST_P(GLES2DecoderTest, TestImageBindingForDecoderManagement) {
  const GLuint service_id = 123;
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .Times(1)
      .WillOnce(SetArgPointee<1>(service_id))
      .RetiresOnSaturation();
  const GLenum target = GL_TEXTURE_EXTERNAL_OES;
  std::unique_ptr<AbstractTexture> abstract_texture =
      GetDecoder()->CreateAbstractTexture(target, GL_RGBA, 256, /* width */
                                          256,                  /* height */
                                          1,                    /* depth */
                                          0,                    /* border */
                                          GL_RGBA, GL_UNSIGNED_BYTE);
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  abstract_texture->BindImage(image.get(), GetParam());
  auto* validating_texture =
      static_cast<ValidatingAbstractTextureImpl*>(abstract_texture.get());
  TextureRef* texture_ref = validating_texture->GetTextureRefForTesting();
  Texture::ImageState state;
  EXPECT_EQ(texture_ref->texture()->GetLevelImage(target, 0, &state),
            image.get());
  EXPECT_EQ(state, GetParam() ? Texture::ImageState::BOUND
                              : Texture::ImageState::UNBOUND);

  EXPECT_CALL(*gl_, DeleteTextures(1, _)).Times(1).RetiresOnSaturation();
  abstract_texture.reset();
}

TEST_P(GLES2DecoderTest, CreateAbstractTexture) {
  const GLuint service_id = 123;
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .Times(1)
      .WillOnce(SetArgPointee<1>(service_id))
      .RetiresOnSaturation();
  const GLenum target = GL_TEXTURE_EXTERNAL_OES;
  std::unique_ptr<AbstractTexture> abstract_texture =
      GetDecoder()->CreateAbstractTexture(target, GL_RGBA, 256, /* width */
                                          256,                  /* height */
                                          1,                    /* depth */
                                          0,                    /* border */
                                          GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_EQ(abstract_texture->GetTextureBase()->target(), target);
  EXPECT_EQ(abstract_texture->service_id(), service_id);
  Texture* texture = Texture::CheckedCast(abstract_texture->GetTextureBase());
  EXPECT_EQ(texture->SafeToRenderFrom(), false);

  // Set some parameters, and verify that we set them.
  // These three are for ScopedTextureBinder.
  // TODO(liberato): Is there a way to make this less brittle?
  EXPECT_CALL(*gl_, GetIntegerv(_, _)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(target, _)).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*gl_, BindTexture(target, abstract_texture->service_id()))
      .Times(1)
      .RetiresOnSaturation();

  // This one we actually care about.
  EXPECT_CALL(*gl_, TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  abstract_texture->SetParameteri(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_EQ(texture->min_filter(), static_cast<GLenum>(GL_LINEAR));

  // Attach an image and see if it works.
  scoped_refptr<gl::GLImage> image(new gl::GLImageStub);
  abstract_texture->BindImage(image.get(), true);
  EXPECT_EQ(abstract_texture->GetImage(), image.get());
  // Binding an image should make the texture renderable.
  EXPECT_EQ(texture->SafeToRenderFrom(), true);
  EXPECT_EQ(texture->GetLevelImage(target, 0), image.get());

  // Unbinding should make it not renderable.
  abstract_texture->BindImage(nullptr, false);
  EXPECT_EQ(texture->SafeToRenderFrom(), false);
  EXPECT_EQ(abstract_texture->GetImage(), nullptr);

  // Attach a stream image, and verify that the image changes and the service_id
  // matches the one we provide.
  scoped_refptr<gpu::gles2::GLStreamTextureImage> stream_image(
      new gpu::gles2::GLStreamTextureImageStub);
  const GLuint surface_texture_service_id = service_id + 1;
  abstract_texture->BindStreamTextureImage(stream_image.get(),
                                           surface_texture_service_id);
  EXPECT_EQ(texture->SafeToRenderFrom(), true);
  EXPECT_EQ(texture->GetLevelStreamTextureImage(target, 0), stream_image.get());
  EXPECT_EQ(abstract_texture->service_id(), surface_texture_service_id);

  // Deleting |abstract_texture| should delete the platform texture as well,
  // since we haven't make a copy of the TextureRef.  Also make sure that the
  // cleanup CB is called.
  EXPECT_CALL(*gl_, DeleteTextures(1, _)).Times(1).RetiresOnSaturation();
  bool cleanup_flag = false;
  abstract_texture->SetCleanupCallback(base::BindOnce(
      [](bool* flag, AbstractTexture*) { *flag = true; }, &cleanup_flag));
  abstract_texture.reset();
  EXPECT_TRUE(cleanup_flag);
}

TEST_P(GLES2DecoderTest, AbstractTextureIsDestroyedWithDecoder) {
  // Deleting the decoder should delete the AbstractTexture's TextureRef.
  const GLuint service_id = 123;
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .Times(1)
      .WillOnce(SetArgPointee<1>(service_id))
      .RetiresOnSaturation();
  const GLenum target = GL_TEXTURE_EXTERNAL_OES;
  std::unique_ptr<AbstractTexture> abstract_texture =
      GetDecoder()->CreateAbstractTexture(target, GL_RGBA, 256, /* width */
                                          256,                  /* height */
                                          1,                    /* depth */
                                          0,                    /* border */
                                          GL_RGBA, GL_UNSIGNED_BYTE);
  bool cleanup_flag = false;
  abstract_texture->SetCleanupCallback(base::BindOnce(
      [](bool* flag, AbstractTexture*) { *flag = true; }, &cleanup_flag));

  // There is only one TextureRef, so it should delete the platform texture.  It
  // should also call the cleanup cb.
  EXPECT_CALL(*gl_, DeleteTextures(1, _)).Times(1).RetiresOnSaturation();
  ResetDecoder();
  // The texture should no longer have a TextureRef.
  EXPECT_EQ(abstract_texture->GetTextureBase(), nullptr);
  EXPECT_TRUE(cleanup_flag);
}

TEST_P(GLES2DecoderTest, AbstractTextureIsDestroyedWhenMadeCurrent) {
  // When an AbstractTexture is destroyed, the ref will be dropped by the next
  // call to MakeCurrent if the context isn't already current.
  const GLuint service_id = 123;
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .Times(1)
      .WillOnce(SetArgPointee<1>(service_id))
      .RetiresOnSaturation();
  const GLenum target = GL_TEXTURE_EXTERNAL_OES;
  std::unique_ptr<AbstractTexture> abstract_texture =
      GetDecoder()->CreateAbstractTexture(target, GL_RGBA, 256, /* width */
                                          256,                  /* height */
                                          1,                    /* depth */
                                          0,                    /* border */
                                          GL_RGBA, GL_UNSIGNED_BYTE);

  // Make the context not current, so that it's not destroyed immediately.
  context_->ReleaseCurrent(surface_.get());
  abstract_texture.reset();
  // Make the context current again, |context_| overrides it with a mock.
  context_->GLContextStub::MakeCurrent(surface_.get());

  // Having textures to delete should signal idle work.
  EXPECT_EQ(GetDecoder()->HasMoreIdleWork(), true);
  EXPECT_CALL(*gl_, DeleteTextures(1, _)).Times(1).RetiresOnSaturation();

  // Allow the context to be made current.
  EXPECT_CALL(*context_, MakeCurrent(surface_.get())).WillOnce(Return(true));
  GetDecoder()->MakeCurrent();
}

TEST_P(GLES2DecoderTest, AbstractTextureIsDestroyedIfAlreadyCurrent) {
  // When an AbstractTexture is destroyed, the ref will be dropped immediately
  // if the context is current.
  const GLuint service_id = 123;
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .Times(1)
      .WillOnce(SetArgPointee<1>(service_id))
      .RetiresOnSaturation();
  const GLenum target = GL_TEXTURE_EXTERNAL_OES;
  std::unique_ptr<AbstractTexture> abstract_texture =
      GetDecoder()->CreateAbstractTexture(target, GL_RGBA, 256, /* width */
                                          256,                  /* height */
                                          1,                    /* depth */
                                          0,                    /* border */
                                          GL_RGBA, GL_UNSIGNED_BYTE);

  EXPECT_CALL(*gl_, DeleteTextures(1, _)).Times(1).RetiresOnSaturation();
  abstract_texture.reset();
  EXPECT_EQ(GetDecoder()->HasMoreIdleWork(), false);
}

TEST_P(GLES2DecoderTest, TestAbstractTextureSetClearedWorks) {
  const GLuint service_id = 123;
  EXPECT_CALL(*gl_, GenTextures(1, _))
      .Times(1)
      .WillOnce(SetArgPointee<1>(service_id))
      .RetiresOnSaturation();
  const GLenum target = GL_TEXTURE_2D;
  std::unique_ptr<AbstractTexture> abstract_texture =
      GetDecoder()->CreateAbstractTexture(target, GL_RGBA, 256, /* width */
                                          256,                  /* height */
                                          1,                    /* depth */
                                          0,                    /* border */
                                          GL_RGBA, GL_UNSIGNED_BYTE);
  Texture* texture = Texture::CheckedCast(abstract_texture->GetTextureBase());

  // Texture should start off unrenderable.
  EXPECT_EQ(texture->SafeToRenderFrom(), false);

  // Setting it to be cleared should make it renderable.
  abstract_texture->SetCleared();
  EXPECT_EQ(texture->SafeToRenderFrom(), true);

  EXPECT_CALL(*gl_, DeleteTextures(1, _)).Times(1).RetiresOnSaturation();
  abstract_texture.reset();
}

TEST_P(GLES3DecoderTest, GetInternalformativValidArgsSamples) {
  const GLint kNumSampleCounts = 8;
  auto* result =
      static_cast<cmds::GetInternalformativ::Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetInternalformativ(GL_RENDERBUFFER, GL_RGBA8,
                                        GL_NUM_SAMPLE_COUNTS, 1, _))
      .WillOnce(SetArgPointee<4>(kNumSampleCounts))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_SAMPLES,
                                        kNumSampleCounts, _))
      .Times(1)
      .RetiresOnSaturation();
  result->size = 0;
  cmds::GetInternalformativ cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA8, GL_SAMPLES,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kNumSampleCounts, result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, GetInternalformativValidArgsNumSampleCounts) {
  const GLint kNumSampleCounts = 8;
  auto* result =
      static_cast<cmds::GetInternalformativ::Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetInternalformativ(GL_RENDERBUFFER, GL_RGBA8,
                                        GL_NUM_SAMPLE_COUNTS, 1, _))
      .WillOnce(SetArgPointee<4>(kNumSampleCounts))
      .RetiresOnSaturation();
  result->size = 0;
  cmds::GetInternalformativ cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  EXPECT_EQ(kNumSampleCounts, result->GetData()[0]);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, ClientWaitSyncValid) {
  auto* result =
      static_cast<cmds::ClientWaitSync::Result*>(shared_memory_address_);
  cmds::ClientWaitSync cmd;
  cmd.Init(client_sync_id_, GL_SYNC_FLUSH_COMMANDS_BIT, 0,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_,
              ClientWaitSync(reinterpret_cast<GLsync>(kServiceSyncId),
                             GL_SYNC_FLUSH_COMMANDS_BIT, 0))
      .WillOnce(Return(GL_CONDITION_SATISFIED))
      .RetiresOnSaturation();
  *result = GL_WAIT_FAILED;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(static_cast<GLenum>(GL_CONDITION_SATISFIED), *result);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, ClientWaitSyncNonZeroTimeoutValid) {
  auto* result =
      static_cast<cmds::ClientWaitSync::Result*>(shared_memory_address_);
  cmds::ClientWaitSync cmd;
  const GLuint64 kTimeout = 0xABCDEF0123456789;
  cmd.Init(client_sync_id_, GL_SYNC_FLUSH_COMMANDS_BIT, kTimeout,
           shared_memory_id_, shared_memory_offset_);
  EXPECT_CALL(*gl_,
              ClientWaitSync(reinterpret_cast<GLsync>(kServiceSyncId),
                             GL_SYNC_FLUSH_COMMANDS_BIT, kTimeout))
      .WillOnce(Return(GL_CONDITION_SATISFIED))
      .RetiresOnSaturation();
  *result = GL_WAIT_FAILED;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(static_cast<GLenum>(GL_CONDITION_SATISFIED), *result);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, ClientWaitSyncInvalidSyncFails) {
  auto* result =
      static_cast<cmds::ClientWaitSync::Result*>(shared_memory_address_);
  cmds::ClientWaitSync cmd;
  cmd.Init(kInvalidClientId, GL_SYNC_FLUSH_COMMANDS_BIT, 0,
           shared_memory_id_, shared_memory_offset_);
  *result = GL_WAIT_FAILED;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(static_cast<GLenum>(GL_WAIT_FAILED), *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderTest, ClientWaitSyncResultNotInitFails) {
  auto* result =
      static_cast<cmds::ClientWaitSync::Result*>(shared_memory_address_);
  cmds::ClientWaitSync cmd;
  cmd.Init(client_sync_id_, GL_SYNC_FLUSH_COMMANDS_BIT, 0,
           shared_memory_id_, shared_memory_offset_);
  *result = 1;  // Any value other than GL_WAIT_FAILED
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest, ClientWaitSyncBadSharedMemoryFails) {
  auto* result =
      static_cast<cmds::ClientWaitSync::Result*>(shared_memory_address_);
  cmds::ClientWaitSync cmd;
  *result = GL_WAIT_FAILED;
  cmd.Init(client_sync_id_, GL_SYNC_FLUSH_COMMANDS_BIT, 0,
           kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));

  *result = GL_WAIT_FAILED;
  cmd.Init(client_sync_id_, GL_SYNC_FLUSH_COMMANDS_BIT, 0,
           shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest, WaitSyncValidArgs) {
  const GLuint64 kTimeout = GL_TIMEOUT_IGNORED;
  EXPECT_CALL(*gl_, WaitSync(reinterpret_cast<GLsync>(kServiceSyncId),
                             0, kTimeout))
      .Times(1)
      .RetiresOnSaturation();

  cmds::WaitSync cmd;
  cmd.Init(client_sync_id_, 0, kTimeout);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, InitFailsIfLostContext) {
  InitState init;
  init.extensions = "GL_KHR_robustness";
  init.lose_context_on_init = true;
  EXPECT_EQ(ContextResult::kTransientFailure,
            MaybeInitDecoderWithWorkarounds(init, GpuDriverBugWorkarounds()));
}

TEST_P(GLES2DecoderManualInitTest, BindGeneratesResourceFalse) {
  InitState init;
  InitDecoder(init);

  cmds::BindTexture cmd1;
  cmd1.Init(GL_TEXTURE_2D, kInvalidClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd1));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  cmds::BindBuffer cmd2;
  cmd2.Init(GL_ARRAY_BUFFER, kInvalidClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  cmds::BindFramebuffer cmd3;
  cmd3.Init(GL_FRAMEBUFFER, kInvalidClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd3));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  cmds::BindRenderbuffer cmd4;
  cmd4.Init(GL_RENDERBUFFER, kInvalidClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd4));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, EnableFeatureCHROMIUMBadBucket) {
  const uint32_t kBadBucketId = 123;
  cmds::EnableFeatureCHROMIUM cmd;
  cmd.Init(kBadBucketId, shared_memory_id_, shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, RequestExtensionCHROMIUMBadBucket) {
  const uint32_t kBadBucketId = 123;
  cmds::RequestExtensionCHROMIUM cmd;
  cmd.Init(kBadBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, BeginQueryEXTDisabled) {
  // Test something fails if off.
}

TEST_P(GLES2DecoderTest, GenQueriesEXTImmediateValidArgs) {
  cmds::GenQueriesEXTImmediate* cmd =
      GetImmediateAs<cmds::GenQueriesEXTImmediate>();
  GLuint temp = kNewClientId;
  cmd->Init(1, &temp);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(*cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  EXPECT_TRUE(query_manager->IsValidQuery(kNewClientId));
}

TEST_P(GLES2DecoderTest, GenQueriesEXTImmediateDuplicateOrNullIds) {
  cmds::GenQueriesEXTImmediate* cmd =
      GetImmediateAs<cmds::GenQueriesEXTImmediate>();
  GLuint temp[3] = {kNewClientId, kNewClientId + 1, kNewClientId};
  cmd->Init(3, temp);
  EXPECT_EQ(error::kInvalidArguments, ExecuteImmediateCmd(*cmd, sizeof(temp)));
  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  EXPECT_FALSE(query_manager->IsValidQuery(kNewClientId));
  EXPECT_FALSE(query_manager->IsValidQuery(kNewClientId + 1));
  GLuint null_id[2] = {kNewClientId, 0};
  cmd->Init(2, null_id);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(*cmd, sizeof(temp)));
  EXPECT_FALSE(query_manager->IsValidQuery(kNewClientId));
}

TEST_P(GLES2DecoderTest, GenQueriesEXTImmediateInvalidArgs) {
  cmds::GenQueriesEXTImmediate* cmd =
      GetImmediateAs<cmds::GenQueriesEXTImmediate>();
  cmd->Init(1, &client_query_id_);
  EXPECT_EQ(error::kInvalidArguments,
            ExecuteImmediateCmd(*cmd, sizeof(&client_query_id_)));
}


TEST_P(GLES2DecoderManualInitTest, BeginEndQueryEXT) {
  InitState init;
  init.extensions = "GL_EXT_occlusion_query_boolean";
  init.gl_version = "OpenGL ES 2.0";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  // Test end fails if no begin.
  cmds::EndQueryEXT end_cmd;
  end_cmd.Init(GL_ANY_SAMPLES_PASSED_EXT, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  cmds::BeginQueryEXT begin_cmd;

  // Test id = 0 fails.
  begin_cmd.Init(GL_ANY_SAMPLES_PASSED_EXT, 0, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  // Test valid parameters work.
  EXPECT_CALL(*gl_, GenQueries(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BeginQuery(GL_ANY_SAMPLES_PASSED_EXT, kNewServiceId))
      .Times(1)
      .RetiresOnSaturation();

  // Query object should not be created untill BeginQueriesEXT.
  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  EXPECT_TRUE(query == nullptr);

  // BeginQueryEXT should fail  if id is not generated from GenQueriesEXT.
  begin_cmd.Init(GL_ANY_SAMPLES_PASSED_EXT, kInvalidClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  begin_cmd.Init(GL_ANY_SAMPLES_PASSED_EXT, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // After BeginQueriesEXT id name should have query object associated with it.
  query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());
  EXPECT_TRUE(query->IsActive());

  // Test trying begin again fails
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Test end fails with different target
  end_cmd.Init(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Test end succeeds.
  EXPECT_CALL(*gl_, EndQuery(GL_ANY_SAMPLES_PASSED_EXT))
      .Times(1)
      .RetiresOnSaturation();
  end_cmd.Init(GL_ANY_SAMPLES_PASSED_EXT, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(query->IsPending());
  EXPECT_FALSE(query->IsActive());

  // Begin should fail if using a different target
  begin_cmd.Init(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, kNewClientId,
                 shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Begin should fail if using a different sync
  begin_cmd.Init(GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT, kNewClientId,
                 shared_memory_id_, kSharedMemoryOffset + sizeof(QuerySync));
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // QueryCounter should fail if using a different target
  cmds::QueryCounterEXT query_counter_cmd;
  query_counter_cmd.Init(kNewClientId, GL_TIMESTAMP, shared_memory_id_,
                         kSharedMemoryOffset, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(query_counter_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // QueryCounter should fail if using a different sync
  query_counter_cmd.Init(kNewClientId, GL_TIMESTAMP, shared_memory_id_,
                         kSharedMemoryOffset + sizeof(QuerySync), 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(query_counter_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  EXPECT_CALL(*gl_, DeleteQueries(1, _)).Times(1).RetiresOnSaturation();
}

struct QueryType {
  GLenum type;
  bool is_counter;
};

const QueryType kQueryTypes[] = {
    {GL_COMMANDS_ISSUED_CHROMIUM, false},
    {GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM, true},
    {GL_LATENCY_QUERY_CHROMIUM, false},
    {GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM, false},
    {GL_GET_ERROR_QUERY_CHROMIUM, false},
    {GL_COMMANDS_COMPLETED_CHROMIUM, false},
    {GL_ANY_SAMPLES_PASSED_EXT, false},
    {GL_TIME_ELAPSED, false},
    {GL_TIMESTAMP, true},
};
const GLsync kGlSync = reinterpret_cast<GLsync>(0xdeadbeef);

static void ExecuteGenerateQueryCmd(GLES2DecoderTestBase* test,
                                    ::gl::MockGLInterface* gl,
                                    GLenum target,
                                    GLuint client_id,
                                    GLuint service_id) {
  test->GenHelper<cmds::GenQueriesEXTImmediate>(client_id);
  if (GL_ANY_SAMPLES_PASSED_EXT == target) {
    EXPECT_CALL(*gl, GenQueries(1, _))
        .WillOnce(SetArgPointee<1>(service_id))
        .RetiresOnSaturation();
  }
}

static error::Error ExecuteBeginQueryCmd(GLES2DecoderTestBase* test,
                                         ::gl::MockGLInterface* gl,
                                         ::gl::GPUTimingFake* timing_queries,
                                         GLenum target,
                                         GLuint client_id,
                                         GLuint service_id,
                                         int32_t shm_id,
                                         uint32_t shm_offset) {
  if (GL_ANY_SAMPLES_PASSED_EXT == target) {
    EXPECT_CALL(*gl, BeginQuery(target, service_id))
        .Times(1)
        .RetiresOnSaturation();
  } else if (GL_TIME_ELAPSED == target) {
    timing_queries->ExpectGPUTimerQuery(*gl, true);
  }

  cmds::BeginQueryEXT begin_cmd;
  begin_cmd.Init(target, client_id, shm_id, shm_offset);
  return test->ExecuteCmd(begin_cmd);
}

static error::Error ExecuteEndQueryCmd(GLES2DecoderTestBase* test,
                                       ::gl::MockGLInterface* gl,
                                       GLenum target,
                                       uint32_t submit_count) {
  if (GL_ANY_SAMPLES_PASSED_EXT == target) {
    EXPECT_CALL(*gl, EndQuery(target))
        .Times(1)
        .RetiresOnSaturation();
  } else if (GL_GET_ERROR_QUERY_CHROMIUM == target) {
    EXPECT_CALL(*gl, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  } else if (GL_COMMANDS_COMPLETED_CHROMIUM == target) {
    EXPECT_CALL(*gl, Flush()).RetiresOnSaturation();
    EXPECT_CALL(*gl, FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
        .WillOnce(Return(kGlSync))
        .RetiresOnSaturation();
#if DCHECK_IS_ON()
    EXPECT_CALL(*gl, IsSync(kGlSync))
        .WillRepeatedly(Return(GL_TRUE));
#endif
  }

  cmds::EndQueryEXT end_cmd;
  end_cmd.Init(target, submit_count);
  return test->ExecuteCmd(end_cmd);
}

static error::Error ExecuteQueryCounterCmd(GLES2DecoderTestBase* test,
                                           ::gl::MockGLInterface* gl,
                                           ::gl::GPUTimingFake* timing_queries,
                                           GLenum target,
                                           GLuint client_id,
                                           GLuint service_id,
                                           int32_t shm_id,
                                           uint32_t shm_offset,
                                           uint32_t submit_count) {
  if (GL_TIMESTAMP == target) {
    timing_queries->ExpectGPUTimeStampQuery(*gl, false);
  }

  cmds::QueryCounterEXT query_counter_cmd;
  query_counter_cmd.Init(client_id,
                         target,
                         shm_id,
                         shm_offset,
                         submit_count);
  return test->ExecuteCmd(query_counter_cmd);
}

static void ProcessQuery(GLES2DecoderTestBase* test,
                         ::gl::MockGLInterface* gl,
                         GLenum target,
                         GLuint service_id) {
  if (GL_ANY_SAMPLES_PASSED_EXT == target) {
    EXPECT_CALL(
        *gl, GetQueryObjectuiv(service_id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgPointee<2>(1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, GetQueryObjectuiv(service_id, GL_QUERY_RESULT_EXT, _))
        .WillOnce(SetArgPointee<2>(1))
        .RetiresOnSaturation();
  } else if (GL_COMMANDS_COMPLETED_CHROMIUM == target) {
    EXPECT_CALL(*gl, ClientWaitSync(kGlSync, _, _))
        .WillOnce(Return(GL_ALREADY_SIGNALED))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, DeleteSync(kGlSync)).Times(1).RetiresOnSaturation();
  }

  QueryManager* query_manager = test->GetDecoder()->GetQueryManager();
  ASSERT_TRUE(nullptr != query_manager);
  query_manager->ProcessPendingQueries(false);
}

static void CheckBeginEndQueryBadMemoryFails(GLES2DecoderTestBase* test,
                                             GLuint client_id,
                                             const QueryType& query_type,
                                             int32_t shm_id,
                                             uint32_t shm_offset) {
  // We need to reset the decoder on each iteration, because we lose the
  // context every time.
  GLES2DecoderTestBase::InitState init;
  init.extensions = "GL_EXT_occlusion_query_boolean"
                    " GL_ARB_sync"
                    " GL_ARB_timer_query";
  init.gl_version = "OpenGL ES 3.0";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  test->InitDecoder(init);

  test->GenHelper<cmds::GenQueriesEXTImmediate>(client_id);

  // Test bad shared memory fails
  error::Error error = error::kNoError;
  if (query_type.is_counter) {
    cmds::QueryCounterEXT query_counter_cmd;
    query_counter_cmd.Init(client_id, query_type.type, shm_id, shm_offset, 1);
    error = test->ExecuteCmd(query_counter_cmd);
  } else {
    cmds::BeginQueryEXT begin_cmd;
    begin_cmd.Init(query_type.type, client_id, shm_id, shm_offset);
    error = test->ExecuteCmd(begin_cmd);
  }

  EXPECT_TRUE(error != error::kNoError);

  test->ResetDecoder();
}

TEST_P(GLES2DecoderManualInitTest, BeginEndQueryEXTBadMemoryIdFails) {
  for (size_t i = 0; i < base::size(kQueryTypes); ++i) {
    CheckBeginEndQueryBadMemoryFails(this, kNewClientId, kQueryTypes[i],
                                     kInvalidSharedMemoryId,
                                     kSharedMemoryOffset);
  }
}

TEST_P(GLES2DecoderManualInitTest, BeginEndQueryEXTBadMemoryOffsetFails) {
  for (size_t i = 0; i < base::size(kQueryTypes); ++i) {
    // Out-of-bounds.
    CheckBeginEndQueryBadMemoryFails(this, kNewClientId, kQueryTypes[i],
                                     shared_memory_id_,
                                     kInvalidSharedMemoryOffset);
    // Overflow.
    CheckBeginEndQueryBadMemoryFails(this, kNewClientId, kQueryTypes[i],
                                     shared_memory_id_, 0xfffffffcu);
  }
}

TEST_P(GLES2DecoderManualInitTest, QueryReuseTest) {
  for (size_t i = 0; i < base::size(kQueryTypes); ++i) {
    const QueryType& query_type = kQueryTypes[i];

    GLES2DecoderTestBase::InitState init;
    init.extensions = "GL_EXT_occlusion_query_boolean"
                      " GL_ARB_sync"
                      " GL_ARB_timer_query";
    init.gl_version = "OpenGL ES 3.0";
    init.has_alpha = true;
    init.request_alpha = true;
    init.bind_generates_resource = true;
    InitDecoder(init);
    ::testing::StrictMock<::gl::MockGLInterface>* gl = GetGLMock();
    ::gl::GPUTimingFake gpu_timing_queries;

    ExecuteGenerateQueryCmd(this, gl, query_type.type,
                            kNewClientId, kNewServiceId);

    // Query once.
    if (query_type.is_counter) {
      EXPECT_EQ(
          error::kNoError,
          ExecuteQueryCounterCmd(this, gl, &gpu_timing_queries, query_type.type,
                                 kNewClientId, kNewServiceId, shared_memory_id_,
                                 kSharedMemoryOffset, 1));
    } else {
      EXPECT_EQ(
          error::kNoError,
          ExecuteBeginQueryCmd(this, gl, &gpu_timing_queries, query_type.type,
                               kNewClientId, kNewServiceId, shared_memory_id_,
                               kSharedMemoryOffset));
      EXPECT_EQ(error::kNoError, ExecuteEndQueryCmd(this, gl,
                                                    query_type.type, 1));
    }

    ProcessQuery(this, gl, query_type.type, kNewServiceId);

    // Reuse query.
    if (query_type.is_counter) {
      EXPECT_EQ(
          error::kNoError,
          ExecuteQueryCounterCmd(this, gl, &gpu_timing_queries, query_type.type,
                                 kNewClientId, kNewServiceId, shared_memory_id_,
                                 kSharedMemoryOffset, 2));
    } else {
      EXPECT_EQ(
          error::kNoError,
          ExecuteBeginQueryCmd(this, gl, &gpu_timing_queries, query_type.type,
                               kNewClientId, kNewServiceId, shared_memory_id_,
                               kSharedMemoryOffset));
      EXPECT_EQ(error::kNoError, ExecuteEndQueryCmd(this, gl,
                                                    query_type.type, 2));
    }

    ProcessQuery(this, gl, query_type.type, kNewServiceId);

    if (GL_ANY_SAMPLES_PASSED_EXT == query_type.type)
      EXPECT_CALL(*gl, DeleteQueries(1, _)).Times(1).RetiresOnSaturation();
    ResetDecoder();
  }
}

TEST_P(GLES2DecoderTest, BeginEndQueryEXTCommandsIssuedCHROMIUM) {
  cmds::BeginQueryEXT begin_cmd;

  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  // Test valid parameters work.
  begin_cmd.Init(GL_COMMANDS_ISSUED_CHROMIUM, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());
  EXPECT_TRUE(query->IsActive());

  // Test end succeeds.
  cmds::EndQueryEXT end_cmd;
  end_cmd.Init(GL_COMMANDS_ISSUED_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(query->IsPending());
  EXPECT_FALSE(query->IsActive());
}

TEST_P(GLES2DecoderTest, QueryCounterEXTCommandsIssuedTimestampCHROMIUM) {
  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  cmds::QueryCounterEXT query_counter_cmd;
  query_counter_cmd.Init(kNewClientId, GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM,
                         shared_memory_id_, kSharedMemoryOffset, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(query_counter_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());
  EXPECT_FALSE(query->IsActive());
}

TEST_P(GLES2DecoderTest, BeginEndQueryEXTGetErrorQueryCHROMIUM) {
  cmds::BeginQueryEXT begin_cmd;

  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  // Test valid parameters work.
  begin_cmd.Init(GL_GET_ERROR_QUERY_CHROMIUM, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());
  EXPECT_TRUE(query->IsActive());

  // Test end succeeds.
  auto* sync = static_cast<QuerySync*>(shared_memory_address_);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_INVALID_VALUE))
      .RetiresOnSaturation();

  cmds::EndQueryEXT end_cmd;
  end_cmd.Init(GL_GET_ERROR_QUERY_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(query->IsPending());
  EXPECT_FALSE(query->IsActive());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE),
            static_cast<GLenum>(sync->result));
}

TEST_P(GLES2DecoderTest, SetDisjointValueSync) {
  cmds::SetDisjointValueSyncCHROMIUM cmd;

  cmd.Init(static_cast<uint32_t>(-1), 0u);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));

  cmd.Init(kInvalidSharedMemoryId, 0u);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));

  cmd.Init(shared_memory_id_, kSharedBufferSize);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));

  cmd.Init(shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  cmd.Init(shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderManualInitTest, BeginEndQueryEXTCommandsCompletedCHROMIUM) {
  InitState init;
  init.extensions = "GL_EXT_occlusion_query_boolean GL_ARB_sync";
  init.gl_version = "OpenGL ES 2.0";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  cmds::BeginQueryEXT begin_cmd;
  begin_cmd.Init(GL_COMMANDS_COMPLETED_CHROMIUM, kNewClientId,
                 shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_FALSE(query->IsPending());
  EXPECT_TRUE(query->IsActive());

  EXPECT_CALL(*gl_, Flush()).RetiresOnSaturation();
  EXPECT_CALL(*gl_, FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
      .WillOnce(Return(kGlSync))
      .RetiresOnSaturation();
#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif

  cmds::EndQueryEXT end_cmd;
  end_cmd.Init(GL_COMMANDS_COMPLETED_CHROMIUM, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(query->IsPending());
  EXPECT_FALSE(query->IsActive());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, ClientWaitSync(kGlSync, _, _))
      .WillOnce(Return(GL_TIMEOUT_EXPIRED))
      .RetiresOnSaturation();
  query_manager->ProcessPendingQueries(false);

  EXPECT_TRUE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, ClientWaitSync(kGlSync, _, _))
      .WillOnce(Return(GL_ALREADY_SIGNALED))
      .RetiresOnSaturation();
  query_manager->ProcessPendingQueries(false);

  EXPECT_FALSE(query->IsPending());

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, IsSync(kGlSync))
      .WillOnce(Return(GL_TRUE))
      .RetiresOnSaturation();
#endif
  EXPECT_CALL(*gl_, DeleteSync(kGlSync)).Times(1).RetiresOnSaturation();
  ResetDecoder();
}

TEST_P(GLES2DecoderManualInitTest, BeginInvalidTargetQueryFails) {
  InitState init;
  init.extensions = "";
  init.gl_version = "OpenGL ES 2.0";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  cmds::BeginQueryEXT begin_cmd;
  begin_cmd.Init(GL_COMMANDS_COMPLETED_CHROMIUM, kNewClientId,
                 shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  begin_cmd.Init(GL_ANY_SAMPLES_PASSED, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  begin_cmd.Init(GL_ANY_SAMPLES_PASSED_CONSERVATIVE, kNewClientId,
                 shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  begin_cmd.Init(GL_TIME_ELAPSED, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  begin_cmd.Init(0xdeadbeef, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, QueryCounterEXTTimeStamp) {
  InitState init;
  init.extensions = "GL_ARB_timer_query";
  init.gl_version = "OpenGL ES 3.0";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  EXPECT_CALL(*gl_, GenQueries(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, _))
      .WillOnce(SetArgPointee<2>(64))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, QueryCounter(kNewServiceId, GL_TIMESTAMP))
      .Times(1)
      .RetiresOnSaturation();
  cmds::QueryCounterEXT query_counter_cmd;
  query_counter_cmd.Init(kNewClientId, GL_TIMESTAMP, shared_memory_id_,
                         kSharedMemoryOffset, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(query_counter_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  QueryManager* query_manager = decoder_->GetQueryManager();
  ASSERT_TRUE(query_manager != nullptr);
  QueryManager::Query* query = query_manager->GetQuery(kNewClientId);
  ASSERT_TRUE(query != nullptr);
  EXPECT_TRUE(query->IsPending());
  EXPECT_FALSE(query->IsActive());
}

TEST_P(GLES2DecoderManualInitTest, InvalidTargetQueryCounterFails) {
  InitState init;
  init.extensions = "";
  init.gl_version = "OpenGL ES 2.0";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoder(init);

  GenHelper<cmds::GenQueriesEXTImmediate>(kNewClientId);

  cmds::QueryCounterEXT query_counter_cmd;
  query_counter_cmd.Init(kNewClientId, GL_TIMESTAMP, shared_memory_id_,
                         kSharedMemoryOffset, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(query_counter_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  query_counter_cmd.Init(kNewClientId, 0xdeadbeef, shared_memory_id_,
                         kSharedMemoryOffset, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(query_counter_cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, IsEnabledReturnsCachedValue) {
  // NOTE: There are no expectations because no GL functions should be
  // called for DEPTH_TEST or STENCIL_TEST
  static const GLenum kStates[] = {
      GL_DEPTH_TEST, GL_STENCIL_TEST,
  };
  for (size_t ii = 0; ii < base::size(kStates); ++ii) {
    cmds::Enable enable_cmd;
    GLenum state = kStates[ii];
    enable_cmd.Init(state);
    EXPECT_EQ(error::kNoError, ExecuteCmd(enable_cmd));
    auto* result =
        static_cast<cmds::IsEnabled::Result*>(shared_memory_address_);
    cmds::IsEnabled is_enabled_cmd;
    is_enabled_cmd.Init(state, shared_memory_id_, shared_memory_offset_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(is_enabled_cmd));
    EXPECT_NE(0u, *result);
    cmds::Disable disable_cmd;
    disable_cmd.Init(state);
    EXPECT_EQ(error::kNoError, ExecuteCmd(disable_cmd));
    EXPECT_EQ(error::kNoError, ExecuteCmd(is_enabled_cmd));
    EXPECT_EQ(0u, *result);
  }
}

namespace {

class SizeOnlyMemoryTracker : public MemoryTracker {
 public:
  SizeOnlyMemoryTracker() {
    // Account for the 7 default textures. 1 for TEXTURE_2D and 6 faces for
    // TEXTURE_CUBE_MAP. Each is 1x1, with 4 bytes per channel.
    pool_info_.initial_size = 28;
    pool_info_.size = 0;
  }
  ~SizeOnlyMemoryTracker() override = default;

  void TrackMemoryAllocatedChange(uint64_t delta) override {
    pool_info_.size += delta;
  }

  uint64_t GetSize() const override {
    return pool_info_.size - pool_info_.initial_size;
  }

  uint64_t ClientTracingId() const override { return 0; }
  int ClientId() const override { return 0; }
  uint64_t ContextGroupTracingId() const override { return 0; }

 private:
  struct PoolInfo {
    PoolInfo() : initial_size(0), size(0) {}
    uint64_t initial_size;
    uint64_t size;
  };
  PoolInfo pool_info_;
};

}  // anonymous namespace.

TEST_P(GLES2DecoderManualInitTest, MemoryTrackerInitialSize) {
  auto memory_tracker = std::make_unique<SizeOnlyMemoryTracker>();
  auto* memory_tracker_ptr = memory_tracker.get();
  set_memory_tracker(std::move(memory_tracker));
  InitState init;
  init.bind_generates_resource = true;
  InitDecoder(init);
  // Expect that initial size - size is 0.
  EXPECT_EQ(0u, memory_tracker_ptr->GetSize());
  EXPECT_EQ(0u, memory_tracker_ptr->GetSize());
}

TEST_P(GLES2DecoderManualInitTest, MemoryTrackerTexImage2D) {
  auto memory_tracker = std::make_unique<SizeOnlyMemoryTracker>();
  auto* memory_tracker_ptr = memory_tracker.get();
  set_memory_tracker(std::move(memory_tracker));
  InitState init;
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(128u, memory_tracker_ptr->GetSize());
  DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               shared_memory_id_, kSharedMemoryOffset);
  EXPECT_EQ(64u, memory_tracker_ptr->GetSize());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, MemoryTrackerTexStorage2DEXT) {
  auto memory_tracker = std::make_unique<SizeOnlyMemoryTracker>();
  auto* memory_tracker_ptr = memory_tracker.get();
  set_memory_tracker(std::move(memory_tracker));
  InitState init;
  init.extensions = "GL_EXT_texture_storage";
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, TexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 4))
      .Times(1)
      .RetiresOnSaturation();
  cmds::TexStorage2DEXT cmd;
  cmd.Init(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(128u, memory_tracker_ptr->GetSize());
}

TEST_P(GLES2DecoderManualInitTest, MemoryTrackerCopyTexImage2D) {
  GLenum target = GL_TEXTURE_2D;
  GLint level = 0;
  GLenum internal_format = GL_RGBA;
  GLsizei width = 4;
  GLsizei height = 8;
  GLint border = 0;
  auto memory_tracker = std::make_unique<SizeOnlyMemoryTracker>();
  auto* memory_tracker_ptr = memory_tracker.get();
  set_memory_tracker(std::move(memory_tracker));
  InitState init;
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  target, level, internal_format, 0, 0, width, height, border))
      .Times(1)
      .RetiresOnSaturation();
  cmds::CopyTexImage2D cmd;
  cmd.Init(target, level, internal_format, 0, 0, width, height);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(128u, memory_tracker_ptr->GetSize());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, MemoryTrackerRenderbufferStorage) {
  auto memory_tracker = std::make_unique<SizeOnlyMemoryTracker>();
  auto* memory_tracker_ptr = memory_tracker.get();
  set_memory_tracker(std::move(memory_tracker));
  InitState init;
  init.bind_generates_resource = true;
  InitDecoder(init);
  DoBindRenderbuffer(
      GL_RENDERBUFFER, client_renderbuffer_id_, kServiceRenderbufferId);
  EnsureRenderbufferBound(false);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(GL_RENDERBUFFER, GL_RGBA, 8, 4))
      .Times(1)
      .RetiresOnSaturation();
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 8, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(128u, memory_tracker_ptr->GetSize());
}

TEST_P(GLES2DecoderManualInitTest, MemoryTrackerBufferData) {
  auto memory_tracker = std::make_unique<SizeOnlyMemoryTracker>();
  auto* memory_tracker_ptr = memory_tracker.get();
  set_memory_tracker(std::move(memory_tracker));
  InitState init;
  init.bind_generates_resource = true;
  InitDecoder(init);
  EXPECT_EQ(0u, memory_tracker_ptr->GetSize());
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, BufferData(GL_ARRAY_BUFFER, 128, _, GL_STREAM_DRAW))
      .Times(1)
      .RetiresOnSaturation();
  cmds::BufferData cmd;
  cmd.Init(GL_ARRAY_BUFFER, 128, 0, 0, GL_STREAM_DRAW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(128u, memory_tracker_ptr->GetSize());
}

TEST_P(GLES2DecoderManualInitTest, ImmutableCopyTexImage2D) {
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLint kLevel = 0;
  const GLint kLevels = 2;
  const GLenum kInternalFormat = GL_RGBA;
  const GLenum kSizedInternalFormat = GL_RGBA8;
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 8;
  const GLint kBorder = 0;
  InitState init;
  init.extensions = "GL_EXT_texture_storage";
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  init.gl_version = "OpenGL ES 2.0";  // To avoid TexStorage emulation.
  InitDecoder(init);
  DoBindTexture(kTarget, client_texture_id_, kServiceTextureId);

  // CopyTexImage2D will call arbitrary amount of GetErrors.
  EXPECT_CALL(*gl_, GetError())
      .Times(AtLeast(1));

  EXPECT_CALL(*gl_,
              CopyTexImage2D(
                  kTarget, kLevel, kInternalFormat, 0, 0, kWidth, kHeight,
                  kBorder))
      .Times(1);

  EXPECT_CALL(*gl_,
              TexStorage2DEXT(
                  kTarget, kLevels, kSizedInternalFormat, kWidth, kHeight))
      .Times(1);
  cmds::CopyTexImage2D copy_cmd;
  copy_cmd.Init(kTarget, kLevel, kInternalFormat, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(copy_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::TexStorage2DEXT storage_cmd;
  storage_cmd.Init(kTarget, kLevels, kSizedInternalFormat, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(storage_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // This should not invoke CopyTexImage2D.
  copy_cmd.Init(kTarget, kLevel, kInternalFormat, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(copy_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderTest, LoseContextCHROMIUMGuilty) {
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kInnocent))
      .Times(1);
  cmds::LoseContextCHROMIUM cmd;
  cmd.Init(GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
  EXPECT_EQ(error::kLostContext, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
}

TEST_P(GLES2DecoderTest, LoseContextCHROMIUMUnkown) {
  EXPECT_CALL(*mock_decoder_, MarkContextLost(error::kUnknown))
      .Times(1);
  cmds::LoseContextCHROMIUM cmd;
  cmd.Init(GL_UNKNOWN_CONTEXT_RESET_ARB, GL_UNKNOWN_CONTEXT_RESET_ARB);
  EXPECT_EQ(error::kLostContext, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(decoder_->WasContextLost());
  EXPECT_TRUE(decoder_->WasContextLostByRobustnessExtension());
}

TEST_P(GLES2DecoderTest, LoseContextCHROMIUMInvalidArgs0_0) {
  EXPECT_CALL(*mock_decoder_, MarkContextLost(_))
      .Times(0);
  cmds::LoseContextCHROMIUM cmd;
  cmd.Init(GL_NONE, GL_GUILTY_CONTEXT_RESET_ARB);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest, LoseContextCHROMIUMInvalidArgs1_0) {
  EXPECT_CALL(*mock_decoder_, MarkContextLost(_))
      .Times(0);
  cmds::LoseContextCHROMIUM cmd;
  cmd.Init(GL_GUILTY_CONTEXT_RESET_ARB, GL_NONE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

class GLES2DecoderDoCommandsTest : public GLES2DecoderTest {
 public:
  GLES2DecoderDoCommandsTest() {
    for (int i = 0; i < 3; i++) {
      cmds_[i].Init(GL_BLEND);
    }
    entries_per_cmd_ = ComputeNumEntries(cmds_[0].ComputeSize());
  }

  void SetExpectationsForNCommands(int num_commands) {
    for (int i = 0; i < num_commands; i++)
      SetupExpectationsForEnableDisable(GL_BLEND, true);
  }

 protected:
  cmds::Enable cmds_[3];
  int entries_per_cmd_;
};

TEST_P(GLES3DecoderTest, BeginInvalidTargetQueryFails) {
  cmds::BeginQueryEXT begin_cmd;
  begin_cmd.Init(0xdeadbeef, kNewClientId, shared_memory_id_,
                 kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}


TEST_P(GLES3DecoderTest, BindTransformFeedbackValidArgs) {
  EXPECT_CALL(*gl_, BindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                          kServiceTransformFeedbackId));
  SpecializedSetup<cmds::BindTransformFeedback, 0>(true);
  cmds::BindTransformFeedback cmd;
  cmd.Init(GL_TRANSFORM_FEEDBACK, client_transformfeedback_id_);
  EXPECT_CALL(*gl_, BindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, DeleteTransformFeedbacksImmediateInvalidArgs) {
  cmds::DeleteTransformFeedbacksImmediate& cmd =
      *GetImmediateAs<cmds::DeleteTransformFeedbacksImmediate>();
  SpecializedSetup<cmds::DeleteTransformFeedbacksImmediate, 0>(false);
  GLuint temp = kInvalidClientId;
  cmd.Init(1, &temp);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, GetIntegeri_vValidArgs) {
  EXPECT_CALL(*gl_, GetIntegeri_v(_, _, _)).Times(0);
  auto* result =
      static_cast<cmds::GetIntegeri_v::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetIntegeri_v cmd;
  cmd.Init(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 2, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_TRANSFORM_FEEDBACK_BUFFER_BINDING),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, GetInteger64i_vValidArgs) {
  EXPECT_CALL(*gl_, GetInteger64i_v(_, _, _)).Times(0);
  auto* result =
      static_cast<cmds::GetInteger64i_v::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetInteger64i_v cmd;
  cmd.Init(GL_UNIFORM_BUFFER_SIZE, 2, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_UNIFORM_BUFFER_SIZE),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, GetSamplerBinding) {
  const GLuint kClientID = 12;
  const GLuint kServiceID = 1012;
  const GLuint kUnit = 0;
  DoCreateSampler(kClientID, kServiceID);
  DoBindSampler(kUnit, kClientID, kServiceID);

#if DCHECK_IS_ON()
  EXPECT_CALL(*gl_, GetIntegerv(GL_SAMPLER_BINDING, _))
      .WillOnce(testing::SetArgumentPointee<1>(kServiceID))
      .RetiresOnSaturation();
#endif

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  auto* result =
      static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
  cmds::GetIntegerv cmd;
  cmd.Init(GL_SAMPLER_BINDING, shared_memory_id_, shared_memory_offset_);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  EXPECT_EQ(kClientID, static_cast<GLuint>(result->GetData()[0]));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, GetTransformFeedbackBinding) {
  const GLuint kClientID = 12;
  const GLuint kServiceID = 1012;
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK;
  DoCreateTransformFeedback(kClientID, kServiceID);
  EXPECT_CALL(*gl_, BindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  DoBindTransformFeedback(kTarget, kClientID, kServiceID);

  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();

  auto* result =
      static_cast<cmds::GetIntegerv::Result*>(shared_memory_address_);
  cmds::GetIntegerv cmd;
  cmd.Init(
      GL_TRANSFORM_FEEDBACK_BINDING, shared_memory_id_, shared_memory_offset_);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  EXPECT_EQ(kClientID, static_cast<GLuint>(result->GetData()[0]));

  EXPECT_CALL(*gl_, BindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0))
      .Times(1)
      .RetiresOnSaturation();
  DoBindTransformFeedback(kTarget, 0, kServiceDefaultTransformFeedbackId);
  DoDeleteTransformFeedback(kClientID, kServiceID);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

// Test that processing with 0 entries does nothing.
TEST_P(GLES2DecoderDoCommandsTest, DoCommandsOneOfZero) {
  int num_processed = -1;
  SetExpectationsForNCommands(0);
  EXPECT_EQ(
      error::kNoError,
      decoder_->DoCommands(1, &cmds_, entries_per_cmd_ * 0, &num_processed));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(0, num_processed);
}

// Test processing at granularity of single commands.
TEST_P(GLES2DecoderDoCommandsTest, DoCommandsOneOfOne) {
  int num_processed = -1;
  SetExpectationsForNCommands(1);
  EXPECT_EQ(
      error::kNoError,
      decoder_->DoCommands(1, &cmds_, entries_per_cmd_ * 1, &num_processed));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(entries_per_cmd_, num_processed);
}

// Test processing at granularity of multiple commands.
TEST_P(GLES2DecoderDoCommandsTest, DoCommandsThreeOfThree) {
  int num_processed = -1;
  SetExpectationsForNCommands(3);
  EXPECT_EQ(
      error::kNoError,
      decoder_->DoCommands(3, &cmds_, entries_per_cmd_ * 3, &num_processed));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(entries_per_cmd_ * 3, num_processed);
}

// Test processing a request smaller than available entries.
TEST_P(GLES2DecoderDoCommandsTest, DoCommandsTwoOfThree) {
  int num_processed = -1;
  SetExpectationsForNCommands(2);
  EXPECT_EQ(
      error::kNoError,
      decoder_->DoCommands(2, &cmds_, entries_per_cmd_ * 3, &num_processed));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(entries_per_cmd_ * 2, num_processed);
}

// Test that processing stops on a command with size 0.
TEST_P(GLES2DecoderDoCommandsTest, DoCommandsZeroCmdSize) {
  cmds_[1].header.size = 0;
  int num_processed = -1;
  SetExpectationsForNCommands(1);
  EXPECT_EQ(
      error::kInvalidSize,
      decoder_->DoCommands(2, &cmds_, entries_per_cmd_ * 2, &num_processed));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(entries_per_cmd_, num_processed);
}

// Test that processing stops on a command with size greater than available.
TEST_P(GLES2DecoderDoCommandsTest, DoCommandsOutOfBounds) {
  int num_processed = -1;
  SetExpectationsForNCommands(1);
  EXPECT_EQ(error::kOutOfBounds,
            decoder_->DoCommands(
                2, &cmds_, entries_per_cmd_ * 2 - 1, &num_processed));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(entries_per_cmd_, num_processed);
}

// Test that commands with bad argument size are skipped without processing.
TEST_P(GLES2DecoderDoCommandsTest, DoCommandsBadArgSize) {
  cmds_[1].header.size += 1;
  int num_processed = -1;
  SetExpectationsForNCommands(1);
  EXPECT_EQ(error::kInvalidArguments,
            decoder_->DoCommands(
                2, &cmds_, entries_per_cmd_ * 2 + 1, &num_processed));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // gpu::CommandHeader::size is a 21-bit field, so casting it to int is safe.
  // Without the explicit cast, Visual Studio ends up promoting the left hand
  // side to unsigned, and emits a sign mismatch warning.
  EXPECT_EQ(entries_per_cmd_ + static_cast<int>(cmds_[1].header.size),
            num_processed);
}

class GLES2DecoderDescheduleUntilFinishedTest : public GLES2DecoderTest {
 public:
  GLES2DecoderDescheduleUntilFinishedTest() = default;

  void SetUp() override {
    InitState init;
    init.gl_version = "4.4";
    init.extensions += " GL_ARB_compatibility GL_ARB_sync";
    InitDecoder(init);

    EXPECT_CALL(*gl_, FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
        .Times(2)
        .WillOnce(Return(sync_service_id_))
        .WillOnce(Return(sync_service_id2_))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, IsSync(sync_service_id_)).WillRepeatedly(Return(GL_TRUE));
    EXPECT_CALL(*gl_, Flush()).Times(2).RetiresOnSaturation();
    EXPECT_CALL(*gl_, DeleteSync(sync_service_id_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, DeleteSync(sync_service_id2_))
        .Times(1)
        .RetiresOnSaturation();
  }

  void OnDescheduleUntilFinished() override {
    deschedule_until_finished_callback_count_++;
  }
  void OnRescheduleAfterFinished() override {
    reschedule_after_finished_callback_count_++;
  }

 protected:
  int deschedule_until_finished_callback_count_ = 0;
  int reschedule_after_finished_callback_count_ = 0;
  GLsync sync_service_id_ = reinterpret_cast<GLsync>(0x15);
  GLsync sync_service_id2_ = reinterpret_cast<GLsync>(0x15);
};

TEST_P(GLES2DecoderDescheduleUntilFinishedTest, AlreadySignalled) {
  EXPECT_CALL(*gl_, ClientWaitSync(sync_service_id_, 0, 0))
      .Times(1)
      .WillOnce(Return(GL_ALREADY_SIGNALED))
      .RetiresOnSaturation();

  cmds::DescheduleUntilFinishedCHROMIUM cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, deschedule_until_finished_callback_count_);
  EXPECT_EQ(0, reschedule_after_finished_callback_count_);

  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, deschedule_until_finished_callback_count_);
  EXPECT_EQ(0, reschedule_after_finished_callback_count_);
}

TEST_P(GLES2DecoderDescheduleUntilFinishedTest, NotYetSignalled) {
  EXPECT_CALL(*gl_, ClientWaitSync(sync_service_id_, 0, 0))
      .Times(1)
      .WillOnce(Return(GL_TIMEOUT_EXPIRED))
      .RetiresOnSaturation();

  cmds::DescheduleUntilFinishedCHROMIUM cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, deschedule_until_finished_callback_count_);
  EXPECT_EQ(0, reschedule_after_finished_callback_count_);

  EXPECT_EQ(error::kDeferLaterCommands, ExecuteCmd(cmd));
  EXPECT_EQ(1, deschedule_until_finished_callback_count_);
  EXPECT_EQ(0, reschedule_after_finished_callback_count_);
}

void GLES3DecoderTest::SetUp() {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoder(init);
}

void WebGL2DecoderTest::SetUp() {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_WEBGL2;
  InitDecoder(init);
}

void GLES3DecoderWithShaderTest::SetUp() {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoder(init);
  SetupDefaultProgram();
}

void GLES3DecoderRGBBackbufferTest::SetUp() {
  InitState init;
  init.gl_version = "OpenGL ES 3.0";
  init.bind_generates_resource = true;
  init.context_type = CONTEXT_TYPE_OPENGLES3;
  InitDecoder(init);
  SetupDefaultProgram();
}

INSTANTIATE_TEST_SUITE_P(Service, GLES2DecoderTest, ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderWithShaderTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderManualInitTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderRGBBackbufferTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderDoCommandsTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES2DecoderDescheduleUntilFinishedTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service, GLES3DecoderTest, ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service, WebGL2DecoderTest, ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES3DecoderWithShaderTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES3DecoderManualInitTest,
                         ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(Service,
                         GLES3DecoderRGBBackbufferTest,
                         ::testing::Bool());

}  // namespace gles2
}  // namespace gpu
