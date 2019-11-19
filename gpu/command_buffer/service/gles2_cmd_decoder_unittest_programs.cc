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
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_surface_mock.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"

#if !defined(GL_DEPTH24_STENCIL8)
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

using ::gl::MockGLInterface;
using ::testing::_;
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

TEST_P(GLES2DecoderWithShaderTest, GetProgramInfoCHROMIUMValidArgs) {
  const uint32_t kBucketId = 123;
  cmds::GetProgramInfoCHROMIUM cmd;
  cmd.Init(client_program_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_GT(bucket->size(), 0u);
}

TEST_P(GLES2DecoderWithShaderTest, GetProgramInfoCHROMIUMInvalidArgs) {
  const uint32_t kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_TRUE(bucket == nullptr);
  cmds::GetProgramInfoCHROMIUM cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(sizeof(ProgramInfoHeader), bucket->size());
  ProgramInfoHeader* info =
      bucket->GetDataAs<ProgramInfoHeader*>(0, sizeof(ProgramInfoHeader));
  ASSERT_TRUE(info != 0);
  EXPECT_EQ(0u, info->link_status);
  EXPECT_EQ(0u, info->num_attribs);
  EXPECT_EQ(0u, info->num_uniforms);
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformBlocksCHROMIUMValidArgs) {
  const uint32_t kBucketId = 123;
  cmds::GetUniformBlocksCHROMIUM cmd;
  cmd.Init(client_program_id_, kBucketId);
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORM_BLOCKS, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_EQ(sizeof(UniformBlocksHeader), bucket->size());
  UniformBlocksHeader* header =
      bucket->GetDataAs<UniformBlocksHeader*>(0, sizeof(UniformBlocksHeader));
  EXPECT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_uniform_blocks);
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformBlocksCHROMIUMInvalidArgs) {
  const uint32_t kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_TRUE(bucket == nullptr);
  cmds::GetUniformBlocksCHROMIUM cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(sizeof(UniformBlocksHeader), bucket->size());
  UniformBlocksHeader* header =
      bucket->GetDataAs<UniformBlocksHeader*>(0, sizeof(UniformBlocksHeader));
  ASSERT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_uniform_blocks);
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformsES3CHROMIUMValidArgs) {
  const uint32_t kBucketId = 123;
  cmds::GetUniformsES3CHROMIUM cmd;
  cmd.Init(client_program_id_, kBucketId);
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_EQ(sizeof(UniformsES3Header), bucket->size());
  UniformsES3Header* header =
      bucket->GetDataAs<UniformsES3Header*>(0, sizeof(UniformsES3Header));
  EXPECT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_uniforms);
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformsES3CHROMIUMInvalidArgs) {
  const uint32_t kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_TRUE(bucket == nullptr);
  cmds::GetUniformsES3CHROMIUM cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(sizeof(UniformsES3Header), bucket->size());
  UniformsES3Header* header =
      bucket->GetDataAs<UniformsES3Header*>(0, sizeof(UniformsES3Header));
  ASSERT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_uniforms);
}

TEST_P(GLES3DecoderWithShaderTest,
       GetTransformFeedbackVaryingsCHROMIUMValidArgs) {
  const uint32_t kBucketId = 123;
  cmds::GetTransformFeedbackVaryingsCHROMIUM cmd;
  cmd.Init(client_program_id_, kBucketId);
  EXPECT_CALL(*(gl_.get()),
              GetProgramiv(kServiceProgramId,
                           GL_TRANSFORM_FEEDBACK_BUFFER_MODE,
                           _))
      .WillOnce(SetArgPointee<2>(GL_INTERLEAVED_ATTRIBS))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              GetProgramiv(
                  kServiceProgramId, GL_TRANSFORM_FEEDBACK_VARYINGS, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_EQ(sizeof(TransformFeedbackVaryingsHeader), bucket->size());
  TransformFeedbackVaryingsHeader* header =
      bucket->GetDataAs<TransformFeedbackVaryingsHeader*>(
          0, sizeof(TransformFeedbackVaryingsHeader));
  EXPECT_TRUE(header != nullptr);
  EXPECT_EQ(static_cast<uint32_t>(GL_INTERLEAVED_ATTRIBS),
            header->transform_feedback_buffer_mode);
  EXPECT_EQ(0u, header->num_transform_feedback_varyings);
}

TEST_P(GLES3DecoderWithShaderTest,
       GetTransformFeedbackVaryingsCHROMIUMInvalidArgs) {
  const uint32_t kBucketId = 123;
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  EXPECT_TRUE(bucket == nullptr);
  cmds::GetTransformFeedbackVaryingsCHROMIUM cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(sizeof(TransformFeedbackVaryingsHeader), bucket->size());
  TransformFeedbackVaryingsHeader* header =
      bucket->GetDataAs<TransformFeedbackVaryingsHeader*>(
          0, sizeof(TransformFeedbackVaryingsHeader));
  ASSERT_TRUE(header != nullptr);
  EXPECT_EQ(0u, header->num_transform_feedback_varyings);
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformivSucceeds) {
  auto* result =
      static_cast<cmds::GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(kServiceProgramId, kUniform2RealLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetElementCountForUniformType(kUniform2Type),
            static_cast<uint32_t>(result->GetNumResults()));
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformivArrayElementSucceeds) {
  auto* result =
      static_cast<cmds::GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformiv cmd;
  cmd.Init(client_program_id_, kUniform2ElementFakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformiv(kServiceProgramId, kUniform2ElementRealLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetElementCountForUniformType(kUniform2Type),
            static_cast<uint32_t>(result->GetNumResults()));
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformivBadProgramFails) {
  auto* result =
      static_cast<cmds::GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformiv cmd;
  // non-existant program
  cmd.Init(kInvalidClientId, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _)).Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
// Valid id that is not a program. The GL spec requires a different error for
// this case.
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  result->size = kInitialResult;
  cmd.Init(client_shader_id_, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
  // Unlinked program
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(kNewServiceId))
      .RetiresOnSaturation();
  cmds::CreateProgram cmd2;
  cmd2.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  result->size = kInitialResult;
  cmd.Init(kNewClientId, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformivBadLocationFails) {
  auto* result =
      static_cast<cmds::GetUniformiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformiv cmd;
  // invalid location
  cmd.Init(client_program_id_, kInvalidUniformLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _)).Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformivBadSharedMemoryFails) {
  cmds::GetUniformiv cmd;
  cmd.Init(client_program_id_,
           kUniform2FakeLocation,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformiv(_, _, _)).Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2FakeLocation, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformuivSucceeds) {
  auto* result =
      static_cast<cmds::GetUniformuiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformuiv cmd;
  cmd.Init(client_program_id_, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformuiv(kServiceProgramId, kUniform2RealLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetElementCountForUniformType(kUniform2Type),
            static_cast<uint32_t>(result->GetNumResults()));
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformuivArrayElementSucceeds) {
  auto* result =
      static_cast<cmds::GetUniformuiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformuiv cmd;
  cmd.Init(client_program_id_, kUniform2ElementFakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformuiv(kServiceProgramId, kUniform2ElementRealLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetElementCountForUniformType(kUniform2Type),
            static_cast<uint32_t>(result->GetNumResults()));
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformuivBadProgramFails) {
  auto* result =
      static_cast<cmds::GetUniformuiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformuiv cmd;
  // non-existant program
  cmd.Init(kInvalidClientId, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformuiv(_, _, _)).Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
// Valid id that is not a program. The GL spec requires a different error for
// this case.
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  result->size = kInitialResult;
  cmd.Init(client_shader_id_, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
  // Unlinked program
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(kNewServiceId))
      .RetiresOnSaturation();
  cmds::CreateProgram cmd2;
  cmd2.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  result->size = kInitialResult;
  cmd.Init(kNewClientId, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformuivBadLocationFails) {
  auto* result =
      static_cast<cmds::GetUniformuiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformuiv cmd;
  // invalid location
  cmd.Init(client_program_id_, kInvalidUniformLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformuiv(_, _, _)).Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformuivBadSharedMemoryFails) {
  cmds::GetUniformuiv cmd;
  cmd.Init(client_program_id_,
           kUniform2FakeLocation,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformuiv(_, _, _)).Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2FakeLocation, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformfvSucceeds) {
  auto* result =
      static_cast<cmds::GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(kServiceProgramId, kUniform2RealLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetElementCountForUniformType(kUniform2Type),
            static_cast<uint32_t>(result->GetNumResults()));
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformfvArrayElementSucceeds) {
  auto* result =
      static_cast<cmds::GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformfv cmd;
  cmd.Init(client_program_id_, kUniform2ElementFakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetUniformfv(kServiceProgramId, kUniform2ElementRealLocation, _))
      .Times(1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GLES2Util::GetElementCountForUniformType(kUniform2Type),
            static_cast<uint32_t>(result->GetNumResults()));
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformfvBadProgramFails) {
  auto* result =
      static_cast<cmds::GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformfv cmd;
  // non-existant program
  cmd.Init(kInvalidClientId, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _)).Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
// Valid id that is not a program. The GL spec requires a different error for
// this case.
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  result->size = kInitialResult;
  cmd.Init(client_shader_id_, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
  // Unlinked program
  EXPECT_CALL(*gl_, CreateProgram())
      .Times(1)
      .WillOnce(Return(kNewServiceId))
      .RetiresOnSaturation();
  cmds::CreateProgram cmd2;
  cmd2.Init(kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd2));
  result->size = kInitialResult;
  cmd.Init(kNewClientId, kUniform2FakeLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformfvBadLocationFails) {
  auto* result =
      static_cast<cmds::GetUniformfv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetUniformfv cmd;
  // invalid location
  cmd.Init(client_program_id_, kInvalidUniformLocation, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _)).Times(0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformfvBadSharedMemoryFails) {
  cmds::GetUniformfv cmd;
  cmd.Init(client_program_id_,
           kUniform2FakeLocation,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformfv(_, _, _)).Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kUniform2FakeLocation, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetAttachedShadersSucceeds) {
  cmds::GetAttachedShaders cmd;
  using Result = cmds::GetAttachedShaders::Result;
  auto* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  EXPECT_CALL(*gl_, GetAttachedShaders(kServiceProgramId, 1, _, _)).WillOnce(
      DoAll(SetArgPointee<2>(1), SetArgPointee<3>(kServiceShaderId)));
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1).ValueOrDie());
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(1, result->GetNumResults());
  EXPECT_EQ(client_shader_id_, result->GetData()[0]);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetAttachedShadersResultNotInitFail) {
  cmds::GetAttachedShaders cmd;
  using Result = cmds::GetAttachedShaders::Result;
  auto* result = static_cast<Result*>(shared_memory_address_);
  result->size = 1;
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _)).Times(0);
  cmd.Init(client_program_id_, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1).ValueOrDie());
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetAttachedShadersBadProgramFails) {
  cmds::GetAttachedShaders cmd;
  using Result = cmds::GetAttachedShaders::Result;
  auto* result = static_cast<Result*>(shared_memory_address_);
  result->size = 0;
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _)).Times(0);
  cmd.Init(kInvalidClientId, shared_memory_id_, shared_memory_offset_,
           Result::ComputeSize(1).ValueOrDie());
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0U, result->size);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetAttachedShadersBadSharedMemoryFails) {
  cmds::GetAttachedShaders cmd;
  using Result = cmds::GetAttachedShaders::Result;
  cmd.Init(client_program_id_, kInvalidSharedMemoryId, shared_memory_offset_,
           Result::ComputeSize(1).ValueOrDie());
  EXPECT_CALL(*gl_, GetAttachedShaders(_, _, _, _)).Times(0);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, shared_memory_id_, kInvalidSharedMemoryOffset,
           Result::ComputeSize(1).ValueOrDie());
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderManualInitTest, GetShaderPrecisionFormatSucceeds) {
  // Force ES underlying implementation to ensure we check the shader precision
  // format.
  InitState init;
  init.gl_version = "OpenGL ES 2.0";
  init.bind_generates_resource = true;
  InitDecoder(init);

  cmds::GetShaderPrecisionFormat cmd;
  auto* result = static_cast<cmds::GetShaderPrecisionFormat::Result*>(
      shared_memory_address_);
  result->success = 0;
  const GLint range[2] = {62, 62};
  const GLint precision = 16;
  EXPECT_CALL(*gl_, GetShaderPrecisionFormat(_, _, _, _))
      .WillOnce(DoAll(SetArrayArgument<2>(range, range + 2),
                      SetArgPointee<3>(precision)))
      .RetiresOnSaturation();
  cmd.Init(GL_VERTEX_SHADER,
           GL_HIGH_FLOAT,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(range[0], result->min_range);
  EXPECT_EQ(range[1], result->max_range);
  EXPECT_EQ(precision, result->precision);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatResultNotInitFails) {
  cmds::GetShaderPrecisionFormat cmd;
  auto* result = static_cast<cmds::GetShaderPrecisionFormat::Result*>(
      shared_memory_address_);
  result->success = 1;
  // NOTE: GL might not be called. There is no Desktop OpenGL equivalent
  cmd.Init(GL_VERTEX_SHADER,
           GL_HIGH_FLOAT,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetShaderPrecisionFormatBadArgsFails) {
  auto* result = static_cast<cmds::GetShaderPrecisionFormat::Result*>(
      shared_memory_address_);
  result->success = 0;
  cmds::GetShaderPrecisionFormat cmd;
  cmd.Init(
      GL_TEXTURE_2D, GL_HIGH_FLOAT, shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  result->success = 0;
  cmd.Init(GL_VERTEX_SHADER,
           GL_TEXTURE_2D,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest,
       GetShaderPrecisionFormatBadSharedMemoryFails) {
  cmds::GetShaderPrecisionFormat cmd;
  cmd.Init(GL_VERTEX_SHADER,
           GL_HIGH_FLOAT,
           kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(GL_VERTEX_SHADER,
           GL_TEXTURE_2D,
           shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveUniformSucceeds) {
  const GLuint kUniformIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniform cmd;
  auto* result =
      static_cast<cmds::GetActiveUniform::Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_,
           kUniformIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(kUniform2Size, result->size);
  EXPECT_EQ(kUniform2Type, result->type);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(
      0,
      memcmp(
          bucket->GetData(0, bucket->size()), kUniform2Name, bucket->size()));
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveUniformResultNotInitFails) {
  const GLuint kUniformIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniform cmd;
  auto* result =
      static_cast<cmds::GetActiveUniform::Result*>(shared_memory_address_);
  result->success = 1;
  cmd.Init(client_program_id_,
           kUniformIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveUniformBadProgramFails) {
  const GLuint kUniformIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniform cmd;
  auto* result =
      static_cast<cmds::GetActiveUniform::Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(kInvalidClientId,
           kUniformIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  result->success = 0;
  cmd.Init(client_shader_id_,
           kUniformIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveUniformBadIndexFails) {
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniform cmd;
  auto* result =
      static_cast<cmds::GetActiveUniform::Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_,
           kBadUniformIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveUniformBadSharedMemoryFails) {
  const GLuint kUniformIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniform cmd;
  cmd.Init(client_program_id_,
           kUniformIndex,
           kBucketId,
           kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_,
           kUniformIndex,
           kBucketId,
           shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformBlockNameSucceeds) {
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniformBlockName cmd;
  auto* result = static_cast<cmds::GetActiveUniformBlockName::Result*>(
      shared_memory_address_);
  *result = 0;
  cmd.Init(client_program_id_,
           0,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  const char kName[] = "HolyCow";
  const GLsizei kMaxLength = strlen(kName) + 1;
  EXPECT_CALL(*gl_,
              GetProgramiv(kServiceProgramId,
                           GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, _))
      .WillOnce(SetArgPointee<2>(kMaxLength))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              GetActiveUniformBlockName(kServiceProgramId, 0, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(strlen(kName)),
                      SetArrayArgument<4>(kName, kName + strlen(kName) + 1)))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, *result);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(0,
            memcmp(bucket->GetData(0, bucket->size()), kName, bucket->size()));
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformBlockNameUnlinkedProgram) {
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniformBlockName cmd;
  auto* result = static_cast<cmds::GetActiveUniformBlockName::Result*>(
      shared_memory_address_);
  *result = 0;
  cmd.Init(client_program_id_,
           0,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_FALSE))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, *result);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest,
       GetActiveUniformBlockNameResultNotInitFails) {
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniformBlockName cmd;
  auto* result = static_cast<cmds::GetActiveUniformBlockName::Result*>(
      shared_memory_address_);
  *result = 1;
  cmd.Init(client_program_id_,
           0,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformBlockNameBadProgramFails) {
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniformBlockName cmd;
  auto* result = static_cast<cmds::GetActiveUniformBlockName::Result*>(
      shared_memory_address_);
  *result = 0;
  cmd.Init(kInvalidClientId,
           0,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest,
       GetActiveUniformBlockNameBadSharedMemoryFails) {
  const uint32_t kBucketId = 123;
  cmds::GetActiveUniformBlockName cmd;
  cmd.Init(client_program_id_,
           0,
           kBucketId,
           kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_,
           0,
           kBucketId,
           shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformBlockivSucceeds) {
  cmds::GetActiveUniformBlockiv cmd;
  auto* result = static_cast<cmds::GetActiveUniformBlockiv::Result*>(
      shared_memory_address_);
  GLenum kPname[] {
    GL_UNIFORM_BLOCK_BINDING,
    GL_UNIFORM_BLOCK_DATA_SIZE,
    GL_UNIFORM_BLOCK_NAME_LENGTH,
    GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,
    GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
    GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER,
    GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER,
  };
  for (size_t ii = 0; ii < base::size(kPname); ++ii) {
    result->SetNumResults(0);
    cmd.Init(client_program_id_,
             0,
             kPname[ii],
             shared_memory_id_,
             shared_memory_offset_);
    EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
        .WillOnce(SetArgPointee<2>(GL_TRUE))
        .RetiresOnSaturation();
    if (kPname[ii] == GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES) {
      EXPECT_CALL(*gl_, GetError())
          .WillOnce(Return(GL_NO_ERROR))
          .RetiresOnSaturation();
      EXPECT_CALL(*gl_,
                  GetActiveUniformBlockiv(kServiceProgramId, 0,
                      GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, _))
          .WillOnce(SetArgPointee<3>(1))
          .RetiresOnSaturation();
    }
    EXPECT_CALL(*gl_,
                GetActiveUniformBlockiv(
                    kServiceProgramId, 0, kPname[ii], _))
        .WillOnce(SetArgPointee<3>(1976))
        .RetiresOnSaturation();
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(1, result->GetNumResults());
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_EQ(1976, result->GetData()[0]);
  }
}

TEST_P(GLES3DecoderWithShaderTest,
       GetActiveUniformBlockivSucceedsZeroUniforms) {
  cmds::GetActiveUniformBlockiv cmd;
  auto* result = static_cast<cmds::GetActiveUniformBlockiv::Result*>(
      shared_memory_address_);
  result->SetNumResults(0);
  cmd.Init(client_program_id_,
           0,
           GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              GetActiveUniformBlockiv(
                  kServiceProgramId, 0, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgPointee<3>(0))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              GetActiveUniformBlockiv(kServiceProgramId, 0,
                  GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformBlockivUnlinkedProgram) {
  cmds::GetActiveUniformBlockiv cmd;
  auto* result = static_cast<cmds::GetActiveUniformBlockiv::Result*>(
      shared_memory_address_);
  result->SetNumResults(0);
  cmd.Init(client_program_id_,
           0,
           GL_UNIFORM_BLOCK_BINDING,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_FALSE))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest,
       GetActiveUniformBlockivResultNotInitFails) {
  cmds::GetActiveUniformBlockiv cmd;
  auto* result = static_cast<cmds::GetActiveUniformBlockiv::Result*>(
      shared_memory_address_);
  result->SetNumResults(1);  // Should be initialized to 0.
  cmd.Init(client_program_id_,
           0,
           GL_UNIFORM_BLOCK_BINDING,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformBlockivBadProgramFails) {
  cmds::GetActiveUniformBlockiv cmd;
  auto* result = static_cast<cmds::GetActiveUniformBlockiv::Result*>(
      shared_memory_address_);
  result->SetNumResults(0);
  cmd.Init(kInvalidClientId,
           0,
           GL_UNIFORM_BLOCK_BINDING,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest,
       GetActiveUniformBlockivBadSharedMemoryFails) {
  cmds::GetActiveUniformBlockiv cmd;
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  cmd.Init(client_program_id_,
           0,
           GL_UNIFORM_BLOCK_BINDING,
           kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_,
           0,
           GL_UNIFORM_BLOCK_BINDING,
           shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveAttribSucceeds) {
  const GLuint kAttribIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetActiveAttrib cmd;
  auto* result =
      static_cast<cmds::GetActiveAttrib::Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_,
           kAttribIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(kAttrib2Size, result->size);
  EXPECT_EQ(kAttrib2Type, result->type);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(
      0,
      memcmp(bucket->GetData(0, bucket->size()), kAttrib2Name, bucket->size()));
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveAttribResultNotInitFails) {
  const GLuint kAttribIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetActiveAttrib cmd;
  auto* result =
      static_cast<cmds::GetActiveAttrib::Result*>(shared_memory_address_);
  result->success = 1;
  cmd.Init(client_program_id_,
           kAttribIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveAttribBadProgramFails) {
  const GLuint kAttribIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetActiveAttrib cmd;
  auto* result =
      static_cast<cmds::GetActiveAttrib::Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(kInvalidClientId,
           kAttribIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  result->success = 0;
  cmd.Init(client_shader_id_,
           kAttribIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveAttribBadIndexFails) {
  const uint32_t kBucketId = 123;
  cmds::GetActiveAttrib cmd;
  auto* result =
      static_cast<cmds::GetActiveAttrib::Result*>(shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_,
           kBadAttribIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetActiveAttribBadSharedMemoryFails) {
  const GLuint kAttribIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetActiveAttrib cmd;
  cmd.Init(client_program_id_,
           kAttribIndex,
           kBucketId,
           kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_,
           kAttribIndex,
           kBucketId,
           shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformIndicesSucceeds) {
  const uint32_t kBucketId = 123;
  const char kName0[] = "Cow";
  const char kName1[] = "Chicken";
  const char* kNames[] = { kName0, kName1 };
  const size_t kCount = base::size(kNames);
  const char kValidStrEnd = 0;
  const GLuint kIndices[] = { 1, 2 };
  SetBucketAsCStrings(kBucketId, kCount, kNames, kCount, kValidStrEnd);
  auto* result =
      static_cast<cmds::GetUniformIndices::Result*>(shared_memory_address_);
  cmds::GetUniformIndices cmd;
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformIndices(kServiceProgramId, kCount, _, _))
      .WillOnce(SetArrayArgument<3>(kIndices, kIndices + kCount))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_NO_ERROR))
      .RetiresOnSaturation();
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kCount, static_cast<size_t>(result->GetNumResults()));
  for (size_t ii = 0; ii < kCount; ++ii) {
    EXPECT_EQ(kIndices[ii], result->GetData()[ii]);
  }
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformIndicesBadProgramFails) {
  const uint32_t kBucketId = 123;
  const char kName0[] = "Cow";
  const char kName1[] = "Chicken";
  const char* kNames[] = { kName0, kName1 };
  const size_t kCount = base::size(kNames);
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kBucketId, kCount, kNames, kCount, kValidStrEnd);
  auto* result =
      static_cast<cmds::GetUniformIndices::Result*>(shared_memory_address_);
  cmds::GetUniformIndices cmd;
  // None-existant program
  cmd.Init(kInvalidClientId, kBucketId, shared_memory_id_, kSharedMemoryOffset);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Unlinked program.
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_FALSE))
      .RetiresOnSaturation();
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformIndicesBadParamsFails) {
  const uint32_t kBucketId = 123;
  const char kName0[] = "Cow";
  const char kName1[] = "Chicken";
  const char* kNames[] = { kName0, kName1 };
  const size_t kCount = base::size(kNames);
  const char kValidStrEnd = 0;
  const GLuint kIndices[] = { 1, 2 };
  SetBucketAsCStrings(kBucketId, kCount, kNames, kCount, kValidStrEnd);
  auto* result =
      static_cast<cmds::GetUniformIndices::Result*>(shared_memory_address_);
  cmds::GetUniformIndices cmd;
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformIndices(kServiceProgramId, kCount, _, _))
      .WillOnce(SetArrayArgument<3>(kIndices, kIndices + kCount))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
      .WillOnce(Return(GL_NO_ERROR))
      .WillOnce(Return(GL_INVALID_VALUE))
      .RetiresOnSaturation();
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformIndicesResultNotInitFails) {
  const uint32_t kBucketId = 123;
  const char kName0[] = "Cow";
  const char kName1[] = "Chicken";
  const char* kNames[] = { kName0, kName1 };
  const size_t kCount = base::size(kNames);
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kBucketId, kCount, kNames, kCount, kValidStrEnd);
  auto* result =
      static_cast<cmds::GetUniformIndices::Result*>(shared_memory_address_);
  cmds::GetUniformIndices cmd;
  result->size = 1976;  // Any value other than 0.
  cmd.Init(kInvalidClientId, kBucketId, shared_memory_id_, kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformIndicesBadSharedMemoryFails) {
  const uint32_t kBucketId = 123;
  const char kName0[] = "Cow";
  const char kName1[] = "Chicken";
  const char* kNames[] = { kName0, kName1 };
  const size_t kCount = base::size(kNames);
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kBucketId, kCount, kNames, kCount, kValidStrEnd);
  auto* result =
      static_cast<cmds::GetUniformIndices::Result*>(shared_memory_address_);
  cmds::GetUniformIndices cmd;
  cmd.Init(client_program_id_,
           kBucketId,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  result->size = 0;
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  result->size = 0;
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformsivSucceeds) {
  const uint32_t kBucketId = 123;
  const GLuint kIndices[] = { 1, 2 };
  const GLint kResults[] = { 1976, 321 };
  const size_t kCount = base::size(kIndices);
  SetBucketData(kBucketId, kIndices, sizeof(GLuint) * kCount);
  auto* result =
      static_cast<cmds::GetActiveUniformsiv::Result*>(shared_memory_address_);
  cmds::GetActiveUniformsiv cmd;
  cmd.Init(client_program_id_, kBucketId, GL_UNIFORM_TYPE, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_,
              GetActiveUniformsiv(
                  kServiceProgramId, kCount, _, GL_UNIFORM_TYPE, _))
      .WillOnce(SetArrayArgument<4>(kResults, kResults + kCount))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kCount, static_cast<size_t>(result->GetNumResults()));
  for (size_t ii = 0; ii < kCount; ++ii) {
    EXPECT_EQ(kResults[ii], result->GetData()[ii]);
  }
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformsivBadProgramFails) {
  const uint32_t kBucketId = 123;
  const GLuint kIndices[] = { 1, 2 };
  const size_t kCount = base::size(kIndices);
  SetBucketData(kBucketId, kIndices, sizeof(GLuint) * kCount);
  auto* result =
      static_cast<cmds::GetActiveUniformsiv::Result*>(shared_memory_address_);
  cmds::GetActiveUniformsiv cmd;
  // None-existant program
  cmd.Init(kInvalidClientId, kBucketId, GL_UNIFORM_TYPE, shared_memory_id_,
           kSharedMemoryOffset);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Unlinked program.
  cmd.Init(client_program_id_, kBucketId, GL_UNIFORM_TYPE, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_FALSE))
      .RetiresOnSaturation();
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformsivBadParamsFails) {
  const uint32_t kBucketId = 123;
  const GLuint kIndices[] = { 1, 100 };
  const size_t kCount = base::size(kIndices);
  SetBucketData(kBucketId, kIndices, sizeof(GLuint) * kCount);
  auto* result =
      static_cast<cmds::GetActiveUniformsiv::Result*>(shared_memory_address_);
  cmds::GetActiveUniformsiv cmd;
  cmd.Init(client_program_id_, kBucketId, GL_UNIFORM_TYPE, shared_memory_id_,
           kSharedMemoryOffset);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformsivBadPnameFails) {
  const uint32_t kBucketId = 123;
  const GLuint kIndices[] = { 1, 2 };
  const size_t kCount = base::size(kIndices);
  SetBucketData(kBucketId, kIndices, sizeof(GLuint) * kCount);
  auto* result =
      static_cast<cmds::GetActiveUniformsiv::Result*>(shared_memory_address_);
  cmds::GetActiveUniformsiv cmd;
  // GL_UNIFORM_BLOCK_NAME_LENGTH should not be supported.
  cmd.Init(client_program_id_, kBucketId, GL_UNIFORM_BLOCK_NAME_LENGTH,
           shared_memory_id_, kSharedMemoryOffset);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  // Invalid pname
  cmd.Init(client_program_id_, kBucketId, 1, shared_memory_id_,
           kSharedMemoryOffset);
  result->size = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->GetNumResults());
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformsivResultNotInitFails) {
  const uint32_t kBucketId = 123;
  const GLuint kIndices[] = { 1, 2 };
  const size_t kCount = base::size(kIndices);
  SetBucketData(kBucketId, kIndices, sizeof(GLuint) * kCount);
  auto* result =
      static_cast<cmds::GetActiveUniformsiv::Result*>(shared_memory_address_);
  cmds::GetActiveUniformsiv cmd;
  cmd.Init(client_program_id_, kBucketId, GL_UNIFORM_TYPE, shared_memory_id_,
           kSharedMemoryOffset);
  result->size = 1976;  // Any value other than 0.
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetActiveUniformsivBadSharedMemoryFails) {
  const uint32_t kBucketId = 123;
  const GLuint kIndices[] = { 1, 2 };
  const size_t kCount = base::size(kIndices);
  SetBucketData(kBucketId, kIndices, sizeof(GLuint) * kCount);
  auto* result =
      static_cast<cmds::GetActiveUniformsiv::Result*>(shared_memory_address_);
  cmds::GetActiveUniformsiv cmd;
  result->size = 0;
  cmd.Init(client_program_id_,
           kBucketId,
           GL_UNIFORM_TYPE,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  result->size = 0;
  cmd.Init(client_program_id_, kBucketId, GL_UNIFORM_TYPE, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetShaderInfoLogValidArgs) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "void main() { gl_Position = vec4(1.0); }";
  const char* kSource[] = {kSource0};
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kValidStrEnd);
  cmds::ShaderSourceBucket bucket_cmd;
  bucket_cmd.Init(client_shader_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bucket_cmd));
  ClearSharedMemory();

  const char* kInfo = "hello";
  cmds::CompileShader compile_cmd;
  cmds::GetShaderInfoLog cmd;
  EXPECT_CALL(*gl_, ShaderSource(kServiceShaderId, 1, _, _));
  EXPECT_CALL(*gl_, CompileShader(kServiceShaderId));
  EXPECT_CALL(*gl_, GetShaderiv(kServiceShaderId, GL_COMPILE_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_FALSE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetShaderiv(kServiceShaderId, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgPointee<2>(strlen(kInfo) + 1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetShaderInfoLog(kServiceShaderId, strlen(kInfo) + 1, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(strlen(kInfo)),
                      SetArrayArgument<3>(kInfo, kInfo + strlen(kInfo) + 1)));
  compile_cmd.Init(client_shader_id_);
  cmd.Init(client_shader_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(compile_cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(strlen(kInfo) + 1, bucket->size());
  EXPECT_EQ(0,
            memcmp(bucket->GetData(0, bucket->size()), kInfo, bucket->size()));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetShaderInfoLogInvalidArgs) {
  const uint32_t kBucketId = 123;
  cmds::GetShaderInfoLog cmd;
  cmd.Init(kInvalidClientId, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetTransformFeedbackVaryingSucceeds) {
  const GLuint kIndex = 1;
  const uint32_t kBucketId = 123;
  const char kName[] = "HolyCow";
  const GLsizei kNumVaryings = 2;
  const GLsizei kBufferSize = static_cast<GLsizei>(strlen(kName) + 1);
  const GLsizei kSize = 2;
  const GLenum kType = GL_FLOAT_VEC2;
  cmds::GetTransformFeedbackVarying cmd;
  auto* result = static_cast<cmds::GetTransformFeedbackVarying::Result*>(
      shared_memory_address_);
  result->success = 0;
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId,
                                 GL_TRANSFORM_FEEDBACK_VARYINGS, _))
      .WillOnce(SetArgPointee<2>(kNumVaryings))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId,
                                 GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, _))
      .WillOnce(SetArgPointee<2>(kBufferSize))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              GetTransformFeedbackVarying(
                  kServiceProgramId, kIndex, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(kBufferSize - 1),
                      SetArgPointee<4>(kSize),
                      SetArgPointee<5>(kType),
                      SetArrayArgument<6>(kName, kName + kBufferSize)))
      .RetiresOnSaturation();
  cmd.Init(client_program_id_,
           kIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_NE(0, result->success);
  EXPECT_EQ(kSize, static_cast<GLsizei>(result->size));
  EXPECT_EQ(kType, static_cast<GLenum>(result->type));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(
      0, memcmp(bucket->GetData(0, bucket->size()), kName, bucket->size()));
}

TEST_P(GLES3DecoderWithShaderTest, GetTransformFeedbackVaryingNotInitFails) {
  const GLuint kIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetTransformFeedbackVarying cmd;
  auto* result = static_cast<cmds::GetTransformFeedbackVarying::Result*>(
      shared_memory_address_);
  result->success = 1;
  cmd.Init(client_program_id_,
           kIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetTransformFeedbackVaryingBadProgramFails) {
  const GLuint kIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetTransformFeedbackVarying cmd;
  auto* result = static_cast<cmds::GetTransformFeedbackVarying::Result*>(
      shared_memory_address_);
  result->success = 0;
  cmd.Init(kInvalidClientId,
           kIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, GetTransformFeedbackVaryingBadParamsFails) {
  const GLuint kIndex = 1;
  const uint32_t kBucketId = 123;
  const GLsizei kNumVaryings = 1;
  cmds::GetTransformFeedbackVarying cmd;
  auto* result = static_cast<cmds::GetTransformFeedbackVarying::Result*>(
      shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_,
           kIndex,
           kBucketId,
           shared_memory_id_,
           shared_memory_offset_);
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId,
                                 GL_TRANSFORM_FEEDBACK_VARYINGS, _))
      .WillOnce(SetArgPointee<2>(kNumVaryings))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, result->success);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest,
       GetTransformFeedbackVaryingBadSharedMemoryFails) {
  const GLuint kIndex = 1;
  const uint32_t kBucketId = 123;
  cmds::GetTransformFeedbackVarying cmd;
  auto* result = static_cast<cmds::GetTransformFeedbackVarying::Result*>(
      shared_memory_address_);
  result->success = 0;
  cmd.Init(client_program_id_,
           kIndex,
           kBucketId,
           kInvalidSharedMemoryId,
           shared_memory_offset_);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_,
           kIndex,
           kBucketId,
           shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, CompileShaderValidArgs) {
  // ShaderSource should not actually call any GL calls yet.
  const uint32_t kInBucketId = 123;
  const char kSource0[] = "void main() { gl_Position = vec4(1.0); }";
  const char* kSource[] = {kSource0};
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kInBucketId, 1, kSource, 1, kValidStrEnd);
  cmds::ShaderSourceBucket bucket_cmd;
  bucket_cmd.Init(client_shader_id_, kInBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bucket_cmd));
  ClearSharedMemory();

  // Compile shader should not actually call any GL calls yet.
  cmds::CompileShader cmd;
  cmd.Init(client_shader_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  // Getting the shader compilation state should trigger the actual GL calls.
  EXPECT_CALL(*gl_, ShaderSource(kServiceShaderId, 1, _, _));
  EXPECT_CALL(*gl_, CompileShader(kServiceShaderId));
  EXPECT_CALL(*gl_, GetShaderiv(kServiceShaderId, GL_COMPILE_STATUS, _))
      .WillOnce(SetArgPointee<2>(GL_TRUE))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();

  auto* result =
      static_cast<cmds::GetShaderiv::Result*>(shared_memory_address_);
  result->size = 0;
  cmds::GetShaderiv status_cmd;
  status_cmd.Init(client_shader_id_, GL_COMPILE_STATUS, shared_memory_id_,
                  kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(status_cmd));
  EXPECT_EQ(GL_TRUE, *result->GetData());
}

TEST_P(GLES2DecoderTest, CompileShaderInvalidArgs) {
  cmds::CompileShader cmd;
  cmd.Init(kInvalidClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS
}

TEST_P(GLES2DecoderTest, ShaderSourceBucketAndGetShaderSourceValidArgs) {
  const uint32_t kInBucketId = 123;
  const uint32_t kOutBucketId = 125;
  const char kSource0[] = "hello";
  const char* kSource[] = { kSource0 };
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kInBucketId, 1, kSource, 1, kValidStrEnd);
  cmds::ShaderSourceBucket cmd;
  cmd.Init(client_shader_id_, kInBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  ClearSharedMemory();
  cmds::GetShaderSource get_cmd;
  get_cmd.Init(client_shader_id_, kOutBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(get_cmd));
  CommonDecoder::Bucket* bucket = decoder_->GetBucket(kOutBucketId);
  ASSERT_TRUE(bucket != nullptr);
  EXPECT_EQ(sizeof(kSource0), bucket->size());
  EXPECT_EQ(0, memcmp(bucket->GetData(0, bucket->size()),
                      kSource0, bucket->size()));
}

#if GLES2_TEST_SHADER_VS_PROGRAM_IDS
TEST_P(GLES2DecoderTest, ShaderSourceBucketWithProgramId) {
  const uint32_t kBucketId = 123;
  const char kSource0[] = "hello";
  const char* kSource[] = { kSource0 };
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kBucketId, 1, kSource, 1, kValidStrEnd);
  cmds::ShaderSourceBucket cmd;
  cmd.Init(client_program_id_, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}
#endif  // GLES2_TEST_SHADER_VS_PROGRAM_IDS

TEST_P(GLES2DecoderTest, ShaderSourceStripComments) {
  const uint32_t kInBucketId = 123;
  const char kSource0[] = "hello/*te\ast*/world//a\ab";
  const char* kSource[] = { kSource0 };
  const char kValidStrEnd = 0;
  SetBucketAsCStrings(kInBucketId, 1, kSource, 1, kValidStrEnd);
  cmds::ShaderSourceBucket cmd;
  cmd.Init(client_shader_id_, kInBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, Uniform1iValidArgs) {
  EXPECT_CALL(*gl_, Uniform1i(kUniform1RealLocation, 2));
  cmds::Uniform1i cmd;
  cmd.Init(kUniform1FakeLocation, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, Uniform1uiValidArgs) {
  EXPECT_CALL(*gl_, Uniform1uiv(kUniform4RealLocation, 1, _));
  cmds::Uniform1ui cmd;
  cmd.Init(kUniform4FakeLocation, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, Uniform1ivImmediateValidArgs) {
  auto& cmd = *GetImmediateAs<cmds::Uniform1ivImmediate>();
  GLint temp[1] = {
      0,
  };
  EXPECT_CALL(*gl_,
              Uniform1iv(kUniform1RealLocation, 1, PointsToArray(temp, 1)));
  cmd.Init(kUniform1FakeLocation, 1, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
}

TEST_P(GLES2DecoderWithShaderTest, Uniform1ivImmediateInvalidValidArgs) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  auto& cmd = *GetImmediateAs<cmds::Uniform1ivImmediate>();
  GLint temp[1 * 2] = {
      0,
  };
  cmd.Init(kUniform1FakeLocation, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, Uniform1ivZeroCount) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  auto& cmd = *GetImmediateAs<cmds::Uniform1ivImmediate>();
  GLint temp = 0;
  cmd.Init(kUniform1FakeLocation, 0, &temp);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, Uniform1iSamplerIsLmited) {
  EXPECT_CALL(*gl_, Uniform1i(_, _)).Times(0);
  cmds::Uniform1i cmd;
  cmd.Init(kUniform1FakeLocation, kNumTextureUnits);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, Uniform1ivSamplerIsLimited) {
  EXPECT_CALL(*gl_, Uniform1iv(_, _, _)).Times(0);
  auto& cmd = *GetImmediateAs<cmds::Uniform1ivImmediate>();
  GLint temp[] = {kNumTextureUnits};
  cmd.Init(kUniform1FakeLocation, 1, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, Uniform1ivArray) {
  auto& cmd = *GetImmediateAs<cmds::Uniform1ivImmediate>();
  GLint temp[3] = {
      0, 1, 2,
  };
  EXPECT_CALL(*gl_,
              Uniform1iv(kUniform8RealLocation, 2, PointsToArray(temp, 2)));
  cmd.Init(kUniform8FakeLocation, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  EXPECT_CALL(*gl_,
              Uniform1iv(kUniform8RealLocation, 2, PointsToArray(temp, 2)));
  cmd.Init(kUniform8FakeLocation, 3, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}


TEST_P(GLES3DecoderWithShaderTest, Uniform1uivImmediateValidArgs) {
  auto& cmd = *GetImmediateAs<cmds::Uniform1uivImmediate>();
  GLuint temp[1] = {
      0,
  };
  EXPECT_CALL(*gl_,
              Uniform1uiv(kUniform4RealLocation, 1, PointsToArray(temp, 1)));
  cmd.Init(kUniform4FakeLocation, 1, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, Uniform1uivImmediateInvalidType) {
  EXPECT_CALL(*gl_, Uniform1uiv(_, _, _)).Times(0);
  auto& cmd = *GetImmediateAs<cmds::Uniform1uivImmediate>();
  GLuint temp[1 * 2] = {
      0,
  };
  // uniform1 is SAMPLER type.
  cmd.Init(kUniform1FakeLocation, 1, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, Uniform1uivZeroCount) {
  EXPECT_CALL(*gl_, Uniform1uiv(_, _, _)).Times(0);
  auto& cmd = *GetImmediateAs<cmds::Uniform1uivImmediate>();
  GLuint temp = 0;
  cmd.Init(kUniform4FakeLocation, 0, &temp);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, Uniform2uiValidArgs) {
  EXPECT_CALL(*gl_, Uniform2uiv(kUniform5RealLocation, 1, _));
  cmds::Uniform2ui cmd;
  cmd.Init(kUniform5FakeLocation, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, Uniform2uivImmediateValidArgs) {
  auto& cmd = *GetImmediateAs<cmds::Uniform2uivImmediate>();
  GLuint temp[2 * 1] = {
      0,
  };
  EXPECT_CALL(*gl_,
              Uniform2uiv(kUniform5RealLocation, 1, PointsToArray(temp, 2)));
  cmd.Init(kUniform5FakeLocation, 1, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, Uniform3uiValidArgs) {
  EXPECT_CALL(*gl_, Uniform3uiv(kUniform6RealLocation, 1, _));
  cmds::Uniform3ui cmd;
  cmd.Init(kUniform6FakeLocation, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, Uniform3uivImmediateValidArgs) {
  auto& cmd = *GetImmediateAs<cmds::Uniform3uivImmediate>();
  GLuint temp[3 * 1] = {
      0,
  };
  EXPECT_CALL(*gl_,
              Uniform3uiv(kUniform6RealLocation, 1, PointsToArray(temp, 3)));
  cmd.Init(kUniform6FakeLocation, 1, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, Uniform4uiValidArgs) {
  EXPECT_CALL(*gl_, Uniform4uiv(kUniform7RealLocation, 1, _));
  cmds::Uniform4ui cmd;
  cmd.Init(kUniform7FakeLocation, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderWithShaderTest, Uniform4uivImmediateValidArgs) {
  auto& cmd = *GetImmediateAs<cmds::Uniform4uivImmediate>();
  GLuint temp[4 * 1] = {
      0,
  };
  EXPECT_CALL(*gl_,
              Uniform4uiv(kUniform7RealLocation, 1, PointsToArray(temp, 4)));
  cmd.Init(kUniform7FakeLocation, 1, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest, BindAttribLocationBucket) {
  const uint32_t kBucketId = 123;
  const GLint kLocation = 2;
  const char* kName = "testing";
  SetBucketAsCString(kBucketId, kName);
  cmds::BindAttribLocationBucket cmd;
  cmd.Init(client_program_id_, kLocation, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderTest, BindAttribLocationBucketInvalidArgs) {
  const uint32_t kBucketId = 123;
  const GLint kLocation = 2;
  const char* kName = "testing";
  EXPECT_CALL(*gl_, BindAttribLocation(_, _, _)).Times(0);
  cmds::BindAttribLocationBucket cmd;
  // check bucket does not exist.
  cmd.Init(client_program_id_, kLocation, kBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  // check bucket is empty.
  SetBucketAsCString(kBucketId, nullptr);
  cmd.Init(client_program_id_, kLocation, kBucketId);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  // Check bad program id
  SetBucketAsCString(kBucketId, kName);
  cmd.Init(kInvalidClientId, kLocation, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, GetAttribLocation) {
  const uint32_t kBucketId = 123;
  const char* kNonExistentName = "foobar";
  auto* result = GetSharedMemoryAs<cmds::GetAttribLocation::Result*>();
  SetBucketAsCString(kBucketId, kAttrib2Name);
  *result = -1;
  cmds::GetAttribLocation cmd;
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kAttrib2Location, *result);
  SetBucketAsCString(kBucketId, kNonExistentName);
  *result = -1;
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
}

TEST_P(GLES2DecoderWithShaderTest, GetAttribLocationInvalidArgs) {
  const uint32_t kBucketId = 123;
  auto* result = GetSharedMemoryAs<cmds::GetAttribLocation::Result*>();
  *result = -1;
  cmds::GetAttribLocation cmd;
  // Check no bucket
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  // Check bad program id.
  SetBucketAsCString(kBucketId, kAttrib2Name);
  cmd.Init(kInvalidClientId, kBucketId, shared_memory_id_, kSharedMemoryOffset);
  *result = -1;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Check bad memory
  cmd.Init(client_program_id_,
           kBucketId,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetFragDataLocation) {
  const uint32_t kBucketId = 123;
  auto* result = GetSharedMemoryAs<cmds::GetFragDataLocation::Result*>();
  SetBucketAsCString(kBucketId, kOutputVariable1NameESSL3);
  *result = -1;
  cmds::GetFragDataLocation cmd;
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(static_cast<GLint>(kOutputVariable1ColorName), *result);
}

TEST_P(GLES3DecoderWithShaderTest, GetFragDataLocationInvalidArgs) {
  const uint32_t kBucketId = 123;
  auto* result = GetSharedMemoryAs<cmds::GetFragDataLocation::Result*>();
  *result = -1;
  cmds::GetFragDataLocation cmd;
  // Check no bucket
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  // Check bad program id.
  const char* kName = "color";
  SetBucketAsCString(kBucketId, kName);
  cmd.Init(kInvalidClientId, kBucketId, shared_memory_id_, kSharedMemoryOffset);
  *result = -1;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Check bad memory
  cmd.Init(client_program_id_,
           kBucketId,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformBlockIndex) {
  const uint32_t kBucketId = 123;
  const GLuint kIndex = 10;
  const char* kName = "color";
  auto* result = GetSharedMemoryAs<cmds::GetUniformBlockIndex::Result*>();
  SetBucketAsCString(kBucketId, kName);
  *result = GL_INVALID_INDEX;
  cmds::GetUniformBlockIndex cmd;
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_CALL(*gl_, GetUniformBlockIndex(kServiceProgramId, StrEq(kName)))
      .WillOnce(Return(kIndex))
      .RetiresOnSaturation();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kIndex, *result);
}

TEST_P(GLES3DecoderWithShaderTest, GetUniformBlockIndexInvalidArgs) {
  const uint32_t kBucketId = 123;
  auto* result = GetSharedMemoryAs<cmds::GetUniformBlockIndex::Result*>();
  *result = GL_INVALID_INDEX;
  cmds::GetUniformBlockIndex cmd;
  // Check no bucket
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_INDEX, *result);
  // Check bad program id.
  const char* kName = "color";
  SetBucketAsCString(kBucketId, kName);
  cmd.Init(kInvalidClientId, kBucketId, shared_memory_id_, kSharedMemoryOffset);
  *result = GL_INVALID_INDEX;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_INDEX, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Check bad memory
  cmd.Init(client_program_id_,
           kBucketId,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformLocation) {
  const uint32_t kBucketId = 123;
  const char* kNonExistentName = "foobar";
  auto* result = GetSharedMemoryAs<cmds::GetUniformLocation::Result*>();
  SetBucketAsCString(kBucketId, kUniform2Name);
  *result = -1;
  cmds::GetUniformLocation cmd;
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(kUniform2FakeLocation, *result);
  SetBucketAsCString(kBucketId, kNonExistentName);
  *result = -1;
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
}

TEST_P(GLES2DecoderWithShaderTest, GetUniformLocationInvalidArgs) {
  const uint32_t kBucketId = 123;
  auto* result = GetSharedMemoryAs<cmds::GetUniformLocation::Result*>();
  *result = -1;
  cmds::GetUniformLocation cmd;
  // Check no bucket
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  // Check bad program id.
  SetBucketAsCString(kBucketId, kUniform2Name);
  cmd.Init(kInvalidClientId, kBucketId, shared_memory_id_, kSharedMemoryOffset);
  *result = -1;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(-1, *result);
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // Check bad memory
  cmd.Init(client_program_id_,
           kBucketId,
           kInvalidSharedMemoryId,
           kSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(client_program_id_, kBucketId, shared_memory_id_,
           kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderWithShaderTest, Basic) {
  // Make sure the setup is correct for ES3.
  EXPECT_TRUE(feature_info()->IsWebGL2OrES3Context());
  EXPECT_TRUE(feature_info()->validators()->texture_bind_target.IsValid(
      GL_TEXTURE_3D));
}

TEST_P(GLES3DecoderWithShaderTest, UniformBlockBindingValidArgs) {
  EXPECT_CALL(*gl_, UniformBlockBinding(kServiceProgramId, 1, 3));
  SpecializedSetup<cmds::UniformBlockBinding, 0>(true);
  cmds::UniformBlockBinding cmd;
  cmd.Init(client_program_id_, 1, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderWithShaderTest, BindUniformLocationCHROMIUMBucket) {
  const uint32_t kBucketId = 123;
  const GLint kLocation = 2;
  const char* kName = "testing";
  const char* kBadName1 = "gl_testing";
  const char* kBadName2 = "testing[1]";

  SetBucketAsCString(kBucketId, kName);
  cmds::BindUniformLocationCHROMIUMBucket cmd;
  cmd.Init(client_program_id_,
           kLocation,
           kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // check negative location
  SetBucketAsCString(kBucketId, kName);
  cmd.Init(client_program_id_, -1, kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // check highest location
  SetBucketAsCString(kBucketId, kName);
  GLint kMaxLocation =
      (kMaxFragmentUniformVectors + kMaxVertexUniformVectors) * 4 - 1;
  cmd.Init(client_program_id_,
           kMaxLocation,
           kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // check too high location
  SetBucketAsCString(kBucketId, kName);
  cmd.Init(client_program_id_,
           kMaxLocation + 1,
           kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
  // check bad name "gl_..."
  SetBucketAsCString(kBucketId, kBadName1);
  cmd.Init(client_program_id_,
           kLocation,
           kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  // check bad name "name[1]" non zero
  SetBucketAsCString(kBucketId, kBadName2);
  cmd.Init(client_program_id_,
           kLocation,
           kBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderManualInitTest, ClearUniformsBeforeFirstProgramUse) {
  gpu::GpuDriverBugWorkarounds workarounds;
  workarounds.clear_uniforms_before_first_program_use = true;
  InitState init;
  init.has_alpha = true;
  init.request_alpha = true;
  init.bind_generates_resource = true;
  InitDecoderWithWorkarounds(init, workarounds);
  {
    static AttribInfo attribs[] = {
        {
         kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location,
        },
        {
         kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location,
        },
        {
         kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location,
        },
    };
    static UniformInfo uniforms[] = {
        {kUniform1Name, kUniform1Size, kUniform1Type, kUniform1FakeLocation,
         kUniform1RealLocation, kUniform1DesiredLocation},
        {kUniform2Name, kUniform2Size, kUniform2Type, kUniform2FakeLocation,
         kUniform2RealLocation, kUniform2DesiredLocation},
        {kUniform3Name, kUniform3Size, kUniform3Type, kUniform3FakeLocation,
         kUniform3RealLocation, kUniform3DesiredLocation},
    };
    SetupShader(attribs, base::size(attribs), uniforms, base::size(uniforms),
                client_program_id_, kServiceProgramId, client_vertex_shader_id_,
                kServiceVertexShaderId, client_fragment_shader_id_,
                kServiceFragmentShaderId);
    TestHelper::SetupExpectationsForClearingUniforms(gl_.get(), uniforms,
                                                     base::size(uniforms));
  }

  {
    EXPECT_CALL(*gl_, UseProgram(kServiceProgramId))
        .Times(1)
        .RetiresOnSaturation();
    cmds::UseProgram cmd;
    cmd.Init(client_program_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
}

TEST_P(GLES2DecoderWithShaderTest, UseDeletedProgram) {
  DoDeleteProgram(client_program_id_, kServiceProgramId);
  {
    cmds::UseProgram cmd;
    cmd.Init(client_program_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId)).Times(1);
}

TEST_P(GLES2DecoderWithShaderTest, DetachDeletedShader) {
  DoDeleteShader(client_fragment_shader_id_, kServiceFragmentShaderId);
  {
    EXPECT_CALL(*gl_, DetachShader(kServiceProgramId, kServiceFragmentShaderId))
        .Times(1)
        .RetiresOnSaturation();
    cmds::DetachShader cmd;
    cmd.Init(client_program_id_, client_fragment_shader_id_);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
}

// TODO(gman): DeleteProgram

// TODO(gman): UseProgram

// TODO(gman): DeleteShader

}  // namespace gles2
}  // namespace gpu
