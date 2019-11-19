// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_2.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_2_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_2_AUTOGEN_H_

TEST_P(GLES2DecoderTest2, GetProgramInfoLogValidArgs) {
  const char* kInfo = "hello";
  const uint32_t kBucketId = 123;
  SpecializedSetup<cmds::GetProgramInfoLog, 0>(true);

  cmds::GetProgramInfoLog cmd;
  cmd.Init(client_program_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(strlen(kInfo) + 1, bucket->size());
  EXPECT_EQ(0,
            memcmp(bucket->GetData(0, bucket->size()), kInfo, bucket->size()));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetProgramInfoLogInvalidArgs) {
  const uint32_t kBucketId = 123;
  cmds::GetProgramInfoLog cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetRenderbufferParameterivValidArgs) {
  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  SpecializedSetup<cmds::GetRenderbufferParameteriv, 0>(true);
  typedef cmds::GetRenderbufferParameteriv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(
      *gl_, GetRenderbufferParameterivEXT(
                GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE, result->GetData()));
  result->size = 0;
  cmds::GetRenderbufferParameteriv cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_RENDERBUFFER_RED_SIZE),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetRenderbufferParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetRenderbufferParameteriv, 0>(false);
  cmds::GetRenderbufferParameteriv::Result* result =
      static_cast<cmds::GetRenderbufferParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  cmds::GetRenderbufferParameteriv cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_RENDERBUFFER_RED_SIZE, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetRenderbufferParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetRenderbufferParameteriv, 0>(false);
  cmds::GetRenderbufferParameteriv::Result* result =
      static_cast<cmds::GetRenderbufferParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  cmds::GetRenderbufferParameteriv cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE, kInvalidSharedMemoryId,
           0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES2DecoderTest2, GetRenderbufferParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetRenderbufferParameterivEXT(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetRenderbufferParameteriv, 0>(false);
  cmds::GetRenderbufferParameteriv::Result* result =
      static_cast<cmds::GetRenderbufferParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  cmds::GetRenderbufferParameteriv cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES3DecoderTest2, GetSamplerParameterfvValidArgs) {
  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  SpecializedSetup<cmds::GetSamplerParameterfv, 0>(true);
  typedef cmds::GetSamplerParameterfv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_,
              GetSamplerParameterfv(kServiceSamplerId, GL_TEXTURE_MAG_FILTER,
                                    result->GetData()));
  result->size = 0;
  cmds::GetSamplerParameterfv cmd;
  cmd.Init(client_sampler_id_, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_TEXTURE_MAG_FILTER),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, GetSamplerParameterivValidArgs) {
  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  SpecializedSetup<cmds::GetSamplerParameteriv, 0>(true);
  typedef cmds::GetSamplerParameteriv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_,
              GetSamplerParameteriv(kServiceSamplerId, GL_TEXTURE_MAG_FILTER,
                                    result->GetData()));
  result->size = 0;
  cmds::GetSamplerParameteriv cmd;
  cmd.Init(client_sampler_id_, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_TEXTURE_MAG_FILTER),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetShaderivValidArgs) {
  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  SpecializedSetup<cmds::GetShaderiv, 0>(true);
  typedef cmds::GetShaderiv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_,
              GetShaderiv(kServiceShaderId, GL_SHADER_TYPE, result->GetData()));
  result->size = 0;
  cmds::GetShaderiv cmd;
  cmd.Init(client_shader_id_, GL_SHADER_TYPE, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_SHADER_TYPE),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetShaderivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetShaderiv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetShaderiv, 0>(false);
  cmds::GetShaderiv::Result* result =
      static_cast<cmds::GetShaderiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetShaderiv cmd;
  cmd.Init(client_shader_id_, GL_SHADER_TYPE, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES2DecoderTest2, GetShaderivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetShaderiv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetShaderiv, 0>(false);
  cmds::GetShaderiv::Result* result =
      static_cast<cmds::GetShaderiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetShaderiv cmd;
  cmd.Init(client_shader_id_, GL_SHADER_TYPE, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES3DecoderTest2, GetSyncivValidArgs) {
  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  SpecializedSetup<cmds::GetSynciv, 0>(true);
  typedef cmds::GetSynciv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(
      *gl_, GetSynciv(reinterpret_cast<GLsync>(kServiceSyncId), GL_SYNC_STATUS,
                      decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                          GL_SYNC_STATUS),
                      nullptr, result->GetData()));
  result->size = 0;
  cmds::GetSynciv cmd;
  cmd.Init(client_sync_id_, GL_SYNC_STATUS, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_SYNC_STATUS),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetTexParameterfvValidArgs) {
  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  SpecializedSetup<cmds::GetTexParameterfv, 0>(true);
  typedef cmds::GetTexParameterfv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                      result->GetData()));
  result->size = 0;
  cmds::GetTexParameterfv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_TEXTURE_MAG_FILTER),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetTexParameterfvInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetTexParameterfv, 0>(false);
  cmds::GetTexParameterfv::Result* result =
      static_cast<cmds::GetTexParameterfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetTexParameterfv cmd;
  cmd.Init(GL_PROXY_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetTexParameterfvInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetTexParameterfv, 0>(false);
  cmds::GetTexParameterfv::Result* result =
      static_cast<cmds::GetTexParameterfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetTexParameterfv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetTexParameterfvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetTexParameterfv, 0>(false);
  cmds::GetTexParameterfv::Result* result =
      static_cast<cmds::GetTexParameterfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetTexParameterfv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES2DecoderTest2, GetTexParameterfvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetTexParameterfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetTexParameterfv, 0>(false);
  cmds::GetTexParameterfv::Result* result =
      static_cast<cmds::GetTexParameterfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetTexParameterfv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES2DecoderTest2, GetTexParameterivValidArgs) {
  EXPECT_CALL(*gl_, GetError()).WillRepeatedly(Return(GL_NO_ERROR));
  SpecializedSetup<cmds::GetTexParameteriv, 0>(true);
  typedef cmds::GetTexParameteriv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  EXPECT_CALL(*gl_, GetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                      result->GetData()));
  result->size = 0;
  cmds::GetTexParameteriv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(
      decoder_->GetGLES2Util()->GLGetNumValuesReturned(GL_TEXTURE_MAG_FILTER),
      result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetTexParameterivInvalidArgs0_0) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetTexParameteriv, 0>(false);
  cmds::GetTexParameteriv::Result* result =
      static_cast<cmds::GetTexParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetTexParameteriv cmd;
  cmd.Init(GL_PROXY_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetTexParameterivInvalidArgs1_0) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetTexParameteriv, 0>(false);
  cmds::GetTexParameteriv::Result* result =
      static_cast<cmds::GetTexParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetTexParameteriv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetTexParameterivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetTexParameteriv, 0>(false);
  cmds::GetTexParameteriv::Result* result =
      static_cast<cmds::GetTexParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetTexParameteriv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES2DecoderTest2, GetTexParameterivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetTexParameteriv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetTexParameteriv, 0>(false);
  cmds::GetTexParameteriv::Result* result =
      static_cast<cmds::GetTexParameteriv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetTexParameteriv cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES2DecoderTest2, GetVertexAttribfvValidArgs) {
  SpecializedSetup<cmds::GetVertexAttribfv, 0>(true);
  typedef cmds::GetVertexAttribfv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribfv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_VERTEX_ATTRIB_ARRAY_NORMALIZED),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetVertexAttribfvInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetVertexAttribfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetVertexAttribfv, 0>(false);
  cmds::GetVertexAttribfv::Result* result =
      static_cast<cmds::GetVertexAttribfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribfv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES2DecoderTest2, GetVertexAttribfvInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetVertexAttribfv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetVertexAttribfv, 0>(false);
  cmds::GetVertexAttribfv::Result* result =
      static_cast<cmds::GetVertexAttribfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribfv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES2DecoderTest2, GetVertexAttribivValidArgs) {
  SpecializedSetup<cmds::GetVertexAttribiv, 0>(true);
  typedef cmds::GetVertexAttribiv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribiv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_VERTEX_ATTRIB_ARRAY_NORMALIZED),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, GetVertexAttribivInvalidArgs2_0) {
  EXPECT_CALL(*gl_, GetVertexAttribiv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetVertexAttribiv, 0>(false);
  cmds::GetVertexAttribiv::Result* result =
      static_cast<cmds::GetVertexAttribiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribiv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, kInvalidSharedMemoryId, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES2DecoderTest2, GetVertexAttribivInvalidArgs2_1) {
  EXPECT_CALL(*gl_, GetVertexAttribiv(_, _, _)).Times(0);
  SpecializedSetup<cmds::GetVertexAttribiv, 0>(false);
  cmds::GetVertexAttribiv::Result* result =
      static_cast<cmds::GetVertexAttribiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribiv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  EXPECT_EQ(0u, result->size);
}

TEST_P(GLES3DecoderTest2, GetVertexAttribIivValidArgs) {
  SpecializedSetup<cmds::GetVertexAttribIiv, 0>(true);
  typedef cmds::GetVertexAttribIiv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribIiv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_VERTEX_ATTRIB_ARRAY_NORMALIZED),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, GetVertexAttribIuivValidArgs) {
  SpecializedSetup<cmds::GetVertexAttribIuiv, 0>(true);
  typedef cmds::GetVertexAttribIuiv::Result Result;
  Result* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetVertexAttribIuiv cmd;
  cmd.Init(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(decoder_->GetGLES2Util()->GLGetNumValuesReturned(
                GL_VERTEX_ATTRIB_ARRAY_NORMALIZED),
            result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, HintValidArgs) {
  EXPECT_CALL(*gl_, Hint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST));
  SpecializedSetup<cmds::Hint, 0>(true);
  cmds::Hint cmd;
  cmd.Init(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, HintInvalidArgs0_0) {
  EXPECT_CALL(*gl_, Hint(_, _)).Times(0);
  SpecializedSetup<cmds::Hint, 0>(false);
  cmds::Hint cmd;
  cmd.Init(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsBufferValidArgs) {
  SpecializedSetup<cmds::IsBuffer, 0>(true);
  cmds::IsBuffer cmd;
  cmd.Init(client_buffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsBufferInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsBuffer, 0>(false);
  cmds::IsBuffer cmd;
  cmd.Init(client_buffer_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_buffer_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, IsEnabledValidArgs) {
  SpecializedSetup<cmds::IsEnabled, 0>(true);
  cmds::IsEnabled cmd;
  cmd.Init(GL_BLEND, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsEnabledInvalidArgs0_0) {
  EXPECT_CALL(*gl_, IsEnabled(_)).Times(0);
  SpecializedSetup<cmds::IsEnabled, 0>(false);
  cmds::IsEnabled cmd;
  cmd.Init(GL_CLIP_PLANE0, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsEnabledInvalidArgs0_1) {
  EXPECT_CALL(*gl_, IsEnabled(_)).Times(0);
  SpecializedSetup<cmds::IsEnabled, 0>(false);
  cmds::IsEnabled cmd;
  cmd.Init(GL_POINT_SPRITE, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsEnabledInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsEnabled, 0>(false);
  cmds::IsEnabled cmd;
  cmd.Init(GL_BLEND, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(GL_BLEND, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, IsFramebufferValidArgs) {
  SpecializedSetup<cmds::IsFramebuffer, 0>(true);
  cmds::IsFramebuffer cmd;
  cmd.Init(client_framebuffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsFramebufferInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsFramebuffer, 0>(false);
  cmds::IsFramebuffer cmd;
  cmd.Init(client_framebuffer_id_, kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_framebuffer_id_, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, IsProgramValidArgs) {
  SpecializedSetup<cmds::IsProgram, 0>(true);
  cmds::IsProgram cmd;
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsProgramInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsProgram, 0>(false);
  cmds::IsProgram cmd;
  cmd.Init(client_program_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, IsRenderbufferValidArgs) {
  SpecializedSetup<cmds::IsRenderbuffer, 0>(true);
  cmds::IsRenderbuffer cmd;
  cmd.Init(client_renderbuffer_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsRenderbufferInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsRenderbuffer, 0>(false);
  cmds::IsRenderbuffer cmd;
  cmd.Init(client_renderbuffer_id_, kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_renderbuffer_id_, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest2, IsSamplerValidArgs) {
  SpecializedSetup<cmds::IsSampler, 0>(true);
  cmds::IsSampler cmd;
  cmd.Init(client_sampler_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, IsSamplerInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsSampler, 0>(false);
  cmds::IsSampler cmd;
  cmd.Init(client_sampler_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_sampler_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, IsShaderValidArgs) {
  SpecializedSetup<cmds::IsShader, 0>(true);
  cmds::IsShader cmd;
  cmd.Init(client_shader_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsShaderInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsShader, 0>(false);
  cmds::IsShader cmd;
  cmd.Init(client_shader_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_shader_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest2, IsSyncValidArgs) {
  SpecializedSetup<cmds::IsSync, 0>(true);
  cmds::IsSync cmd;
  cmd.Init(client_sync_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, IsSyncInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsSync, 0>(false);
  cmds::IsSync cmd;
  cmd.Init(client_sync_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_sync_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, IsTextureValidArgs) {
  SpecializedSetup<cmds::IsTexture, 0>(true);
  cmds::IsTexture cmd;
  cmd.Init(client_texture_id_, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, IsTextureInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsTexture, 0>(false);
  cmds::IsTexture cmd;
  cmd.Init(client_texture_id_, kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_texture_id_, shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest2, IsTransformFeedbackValidArgs) {
  SpecializedSetup<cmds::IsTransformFeedback, 0>(true);
  cmds::IsTransformFeedback cmd;
  cmd.Init(client_transformfeedback_id_, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, IsTransformFeedbackInvalidArgsBadSharedMemoryId) {
  SpecializedSetup<cmds::IsTransformFeedback, 0>(false);
  cmds::IsTransformFeedback cmd;
  cmd.Init(client_transformfeedback_id_, kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(client_transformfeedback_id_, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, LineWidthValidArgs) {
  EXPECT_CALL(*gl_, LineWidth(2.0f));
  SpecializedSetup<cmds::LineWidth, 0>(true);
  cmds::LineWidth cmd;
  cmd.Init(2.0f);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, LineWidthInvalidValue0_0) {
  SpecializedSetup<cmds::LineWidth, 0>(false);
  cmds::LineWidth cmd;
  cmd.Init(0.0f);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest2, LineWidthNaNValue0) {
  SpecializedSetup<cmds::LineWidth, 0>(false);
  cmds::LineWidth cmd;
  cmd.Init(nanf(""));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest2, LinkProgramValidArgs) {
  EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId));
  SpecializedSetup<cmds::LinkProgram, 0>(true);
  cmds::LinkProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, PolygonOffsetValidArgs) {
  EXPECT_CALL(*gl_, PolygonOffset(1, 2));
  SpecializedSetup<cmds::PolygonOffset, 0>(true);
  cmds::PolygonOffset cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, ReadBufferValidArgs) {
  EXPECT_CALL(*gl_, ReadBuffer(GL_NONE));
  SpecializedSetup<cmds::ReadBuffer, 0>(true);
  cmds::ReadBuffer cmd;
  cmd.Init(GL_NONE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, RenderbufferStorageValidArgs) {
  SpecializedSetup<cmds::RenderbufferStorage, 0>(true);
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, RenderbufferStorageInvalidArgs0_0) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::RenderbufferStorage, 0>(false);
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_RGBA4, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, RenderbufferStorageInvalidArgs2_0) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::RenderbufferStorage, 0>(false);
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, -1, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest2, RenderbufferStorageInvalidArgs3_0) {
  EXPECT_CALL(*gl_, RenderbufferStorageEXT(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::RenderbufferStorage, 0>(false);
  cmds::RenderbufferStorage cmd;
  cmd.Init(GL_RENDERBUFFER, GL_RGBA4, 3, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest2, SampleCoverageValidArgs) {
  EXPECT_CALL(*gl_, SampleCoverage(1, true));
  SpecializedSetup<cmds::SampleCoverage, 0>(true);
  cmds::SampleCoverage cmd;
  cmd.Init(1, true);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, SamplerParameterfValidArgs) {
  EXPECT_CALL(*gl_, SamplerParameterf(kServiceSamplerId, GL_TEXTURE_MAG_FILTER,
                                      GL_NEAREST));
  SpecializedSetup<cmds::SamplerParameterf, 0>(true);
  cmds::SamplerParameterf cmd;
  cmd.Init(client_sampler_id_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, SamplerParameterfvImmediateValidArgs) {
  cmds::SamplerParameterfvImmediate& cmd =
      *GetImmediateAs<cmds::SamplerParameterfvImmediate>();
  SpecializedSetup<cmds::SamplerParameterfvImmediate, 0>(true);
  GLfloat temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(client_sampler_id_, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_CALL(*gl_, SamplerParameterf(kServiceSamplerId, GL_TEXTURE_MAG_FILTER,
                                      temp[0]));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, SamplerParameteriValidArgs) {
  EXPECT_CALL(*gl_, SamplerParameteri(kServiceSamplerId, GL_TEXTURE_MAG_FILTER,
                                      GL_NEAREST));
  SpecializedSetup<cmds::SamplerParameteri, 0>(true);
  cmds::SamplerParameteri cmd;
  cmd.Init(client_sampler_id_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest2, SamplerParameterivImmediateValidArgs) {
  cmds::SamplerParameterivImmediate& cmd =
      *GetImmediateAs<cmds::SamplerParameterivImmediate>();
  SpecializedSetup<cmds::SamplerParameterivImmediate, 0>(true);
  GLint temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(client_sampler_id_, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_CALL(*gl_, SamplerParameteri(kServiceSamplerId, GL_TEXTURE_MAG_FILTER,
                                      temp[0]));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, ScissorValidArgs) {
  EXPECT_CALL(*gl_, Scissor(1, 2, 3, 4));
  SpecializedSetup<cmds::Scissor, 0>(true);
  cmds::Scissor cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, ScissorInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Scissor(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::Scissor, 0>(false);
  cmds::Scissor cmd;
  cmd.Init(1, 2, -1, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest2, ScissorInvalidArgs3_0) {
  EXPECT_CALL(*gl_, Scissor(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::Scissor, 0>(false);
  cmds::Scissor cmd;
  cmd.Init(1, 2, 3, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest2, ShaderSourceBucketValidArgs) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = {kSource0};
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kValidStrEnd);
  cmds::ShaderSourceBucket cmd;
  cmd.Init(client_shader_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, ShaderSourceBucketInvalidArgs) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = {kSource0};
  const char kValidStrEnd = 0;
  cmds::ShaderSourceBucket cmd;
  // Test no bucket.
  cmd.Init(client_shader_id_, kBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  // Test invalid client.
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kValidStrEnd);
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest2, ShaderSourceBucketInvalidHeader) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = {kSource0};
  const char kValidStrEnd = 0;
  const GLsizei kCount = static_cast<GLsizei>(base::size(kSource));
  const GLsizei kTests[] = {
      kCount + 1,
      0,
      std::numeric_limits<GLsizei>::max(),
      -1,
  };
  for (size_t ii = 0; ii < base::size(kTests); ++ii) {
    SetBucketAsCStrings(kBucketId, 1, kSource, kTests[ii], kValidStrEnd);
    cmds::ShaderSourceBucket cmd;
    cmd.Init(client_shader_id_, kBucketId);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  }
}

TEST_P(GLES2DecoderTest2, ShaderSourceBucketInvalidStringEnding) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = {kSource0};
  const char kInvalidStrEnd = '*';
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kInvalidStrEnd);
  cmds::ShaderSourceBucket cmd;
  cmd.Init(client_shader_id_, kBucketId);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, StencilFuncValidArgs) {
  EXPECT_CALL(*gl_, StencilFunc(GL_NEVER, 2, 3));
  SpecializedSetup<cmds::StencilFunc, 0>(true);
  cmds::StencilFunc cmd;
  cmd.Init(GL_NEVER, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, StencilFuncSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilFuncSeparate(GL_FRONT, GL_NEVER, 3, 4));
  SpecializedSetup<cmds::StencilFuncSeparate, 0>(true);
  cmds::StencilFuncSeparate cmd;
  cmd.Init(GL_FRONT, GL_NEVER, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, StencilMaskValidArgs) {
  SpecializedSetup<cmds::StencilMask, 0>(true);
  cmds::StencilMask cmd;
  cmd.Init(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, StencilMaskSeparateValidArgs) {
  SpecializedSetup<cmds::StencilMaskSeparate, 0>(true);
  cmds::StencilMaskSeparate cmd;
  cmd.Init(GL_FRONT, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, StencilOpValidArgs) {
  EXPECT_CALL(*gl_, StencilOp(GL_KEEP, GL_INCR, GL_KEEP));
  SpecializedSetup<cmds::StencilOp, 0>(true);
  cmds::StencilOp cmd;
  cmd.Init(GL_KEEP, GL_INCR, GL_KEEP);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, StencilOpSeparateValidArgs) {
  EXPECT_CALL(*gl_, StencilOpSeparate(GL_FRONT, GL_INCR, GL_KEEP, GL_KEEP));
  SpecializedSetup<cmds::StencilOpSeparate, 0>(true);
  cmds::StencilOpSeparate cmd;
  cmd.Init(GL_FRONT, GL_INCR, GL_KEEP, GL_KEEP);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterfValidArgs) {
  EXPECT_CALL(*gl_,
              TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  SpecializedSetup<cmds::TexParameterf, 0>(true);
  cmds::TexParameterf cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterfInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterf, 0>(false);
  cmds::TexParameterf cmd;
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterfInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterf, 0>(false);
  cmds::TexParameterf cmd;
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterfInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameterf, 0>(false);
  cmds::TexParameterf cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterfvImmediateValidArgs) {
  cmds::TexParameterfvImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterfvImmediate>();
  SpecializedSetup<cmds::TexParameterfvImmediate, 0>(true);
  GLfloat temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_CALL(*gl_,
              TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, temp[0]));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterfvImmediateInvalidArgs0_0) {
  cmds::TexParameterfvImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);

  SpecializedSetup<cmds::TexParameterfvImmediate, 0>(false);
  GLfloat temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));

  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterfvImmediateInvalidArgs0_1) {
  cmds::TexParameterfvImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);

  SpecializedSetup<cmds::TexParameterfvImmediate, 0>(false);
  GLfloat temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));

  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterfvImmediateInvalidArgs1_0) {
  cmds::TexParameterfvImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterfvImmediate>();
  EXPECT_CALL(*gl_, TexParameterf(_, _, _)).Times(0);

  SpecializedSetup<cmds::TexParameterfvImmediate, 0>(false);
  GLfloat temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));

  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameteriValidArgs) {
  EXPECT_CALL(*gl_,
              TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  SpecializedSetup<cmds::TexParameteri, 0>(true);
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameteriInvalidArgs0_0) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteri, 0>(false);
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameteriInvalidArgs0_1) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteri, 0>(false);
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameteriInvalidArgs1_0) {
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);
  SpecializedSetup<cmds::TexParameteri, 0>(false);
  cmds::TexParameteri cmd;
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterivImmediateValidArgs) {
  cmds::TexParameterivImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterivImmediate>();
  SpecializedSetup<cmds::TexParameterivImmediate, 0>(true);
  GLint temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_CALL(*gl_,
              TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, temp[0]));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterivImmediateInvalidArgs0_0) {
  cmds::TexParameterivImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);

  SpecializedSetup<cmds::TexParameterivImmediate, 0>(false);
  GLint temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));

  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterivImmediateInvalidArgs0_1) {
  cmds::TexParameterivImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);

  SpecializedSetup<cmds::TexParameterivImmediate, 0>(false);
  GLint temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));

  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderTest2, TexParameterivImmediateInvalidArgs1_0) {
  cmds::TexParameterivImmediate& cmd =
      *GetImmediateAs<cmds::TexParameterivImmediate>();
  EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(0);

  SpecializedSetup<cmds::TexParameterivImmediate, 0>(false);
  GLint temp[1] = {
      GL_NEAREST,
  };
  cmd.Init(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));

  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES3DecoderTest2, TransformFeedbackVaryingsBucketValidArgs) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = {kSource0};
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kValidStrEnd);
  cmds::TransformFeedbackVaryingsBucket cmd;
  cmd.Init(client_program_id_, kBucketId, GL_INTERLEAVED_ATTRIBS);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest2, TransformFeedbackVaryingsBucketInvalidArgs) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = {kSource0};
  const char kValidStrEnd = 0;
  cmds::TransformFeedbackVaryingsBucket cmd;
  // Test no bucket.
  cmd.Init(client_program_id_, kBucketId, GL_INTERLEAVED_ATTRIBS);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  // Test invalid client.
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kValidStrEnd);
  cmd.Init(kInvalidClientId, kBucketId, GL_INTERLEAVED_ATTRIBS);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderTest2, TransformFeedbackVaryingsBucketInvalidHeader) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = {kSource0};
  const char kValidStrEnd = 0;
  const GLsizei kCount = static_cast<GLsizei>(base::size(kSource));
  const GLsizei kTests[] = {
      kCount + 1,
      0,
      std::numeric_limits<GLsizei>::max(),
      -1,
  };
  for (size_t ii = 0; ii < base::size(kTests); ++ii) {
    SetBucketAsCStrings(kBucketId, 1, kSource, kTests[ii], kValidStrEnd);
    cmds::TransformFeedbackVaryingsBucket cmd;
    cmd.Init(client_program_id_, kBucketId, GL_INTERLEAVED_ATTRIBS);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  }
}

TEST_P(GLES3DecoderTest2, TransformFeedbackVaryingsBucketInvalidStringEnding) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = {kSource0};
  const char kInvalidStrEnd = '*';
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kInvalidStrEnd);
  cmds::TransformFeedbackVaryingsBucket cmd;
  cmd.Init(client_program_id_, kBucketId, GL_INTERLEAVED_ATTRIBS);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest2, Uniform1fValidArgs) {
  EXPECT_CALL(*gl_, Uniform1fv(1, 1, _));
  SpecializedSetup<cmds::Uniform1f, 0>(true);
  cmds::Uniform1f cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform1fvImmediateValidArgs) {
  cmds::Uniform1fvImmediate& cmd = *GetImmediateAs<cmds::Uniform1fvImmediate>();
  SpecializedSetup<cmds::Uniform1fvImmediate, 0>(true);
  GLfloat temp[1 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, Uniform1fv(1, 2, PointsToArray(temp, 1)));
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform2fValidArgs) {
  EXPECT_CALL(*gl_, Uniform2fv(1, 1, _));
  SpecializedSetup<cmds::Uniform2f, 0>(true);
  cmds::Uniform2f cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform2fvImmediateValidArgs) {
  cmds::Uniform2fvImmediate& cmd = *GetImmediateAs<cmds::Uniform2fvImmediate>();
  SpecializedSetup<cmds::Uniform2fvImmediate, 0>(true);
  GLfloat temp[2 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, Uniform2fv(1, 2, PointsToArray(temp, 2)));
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform2iValidArgs) {
  EXPECT_CALL(*gl_, Uniform2iv(1, 1, _));
  SpecializedSetup<cmds::Uniform2i, 0>(true);
  cmds::Uniform2i cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform2ivImmediateValidArgs) {
  cmds::Uniform2ivImmediate& cmd = *GetImmediateAs<cmds::Uniform2ivImmediate>();
  SpecializedSetup<cmds::Uniform2ivImmediate, 0>(true);
  GLint temp[2 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, Uniform2iv(1, 2, PointsToArray(temp, 2)));
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform3fValidArgs) {
  EXPECT_CALL(*gl_, Uniform3fv(1, 1, _));
  SpecializedSetup<cmds::Uniform3f, 0>(true);
  cmds::Uniform3f cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform3fvImmediateValidArgs) {
  cmds::Uniform3fvImmediate& cmd = *GetImmediateAs<cmds::Uniform3fvImmediate>();
  SpecializedSetup<cmds::Uniform3fvImmediate, 0>(true);
  GLfloat temp[3 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, Uniform3fv(1, 2, PointsToArray(temp, 3)));
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform3iValidArgs) {
  EXPECT_CALL(*gl_, Uniform3iv(1, 1, _));
  SpecializedSetup<cmds::Uniform3i, 0>(true);
  cmds::Uniform3i cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform3ivImmediateValidArgs) {
  cmds::Uniform3ivImmediate& cmd = *GetImmediateAs<cmds::Uniform3ivImmediate>();
  SpecializedSetup<cmds::Uniform3ivImmediate, 0>(true);
  GLint temp[3 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, Uniform3iv(1, 2, PointsToArray(temp, 3)));
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest2, Uniform4fValidArgs) {
  EXPECT_CALL(*gl_, Uniform4fv(1, 1, _));
  SpecializedSetup<cmds::Uniform4f, 0>(true);
  cmds::Uniform4f cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_2_AUTOGEN_H_
