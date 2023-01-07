// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#include <stdint.h>

#include "base/command_line.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_base.h"
#include "gpu/command_buffer/service/program_manager.h"
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

class GLES2DecoderTest3 : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest3() = default;
};

class GLES3DecoderTest3 : public GLES2DecoderTest3 {
 public:
  GLES3DecoderTest3() { shader_language_version_ = 300; }
 protected:
  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 3.0";
    init.bind_generates_resource = true;
    init.context_type = CONTEXT_TYPE_OPENGLES3;
    InitDecoder(init);
  }
};

INSTANTIATE_TEST_SUITE_P(Service, GLES2DecoderTest3, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(Service, GLES3DecoderTest3, ::testing::Bool());

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4f, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC4);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC4);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4i, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC4);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4ivImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC4);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix3fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT3);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix4fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT4);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix2x3fvImmediate,
                                            0>(bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT2x3);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix2x4fvImmediate,
                                            0>(bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT2x4);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix3x2fvImmediate,
                                            0>(bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT3x2);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix3x4fvImmediate,
                                            0>(bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT3x4);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix4x2fvImmediate,
                                            0>(bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT4x2);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix4x3fvImmediate,
                                            0>(bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT4x3);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UseProgram, 0>(
    bool /* valid */) {
  // Needs the same setup as LinkProgram.
  SpecializedSetup<cmds::LinkProgram, 0>(false);

  EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();

  cmds::LinkProgram link_cmd;
  link_cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(link_cmd));
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::ValidateProgram, 0>(
    bool /* valid */) {
  // Needs the same setup as LinkProgram.
  SpecializedSetup<cmds::LinkProgram, 0>(false);

  EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();

  cmds::LinkProgram link_cmd;
  link_cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(link_cmd));

  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
}

TEST_P(GLES2DecoderTest3, TraceBeginCHROMIUM) {
  const uint32_t kCategoryBucketId = 123;
  const uint32_t kNameBucketId = 234;

  const char kCategory[] = "test_category";
  const char kName[] = "test_command";
  SetBucketAsCString(kCategoryBucketId, kCategory);
  SetBucketAsCString(kNameBucketId, kName);

  cmds::TraceBeginCHROMIUM begin_cmd;
  begin_cmd.Init(kCategoryBucketId, kNameBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));
}

TEST_P(GLES2DecoderTest3, TraceEndCHROMIUM) {
  // Test end fails if no begin.
  cmds::TraceEndCHROMIUM end_cmd;
  end_cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  const uint32_t kCategoryBucketId = 123;
  const uint32_t kNameBucketId = 234;

  const char kCategory[] = "test_category";
  const char kName[] = "test_command";
  SetBucketAsCString(kCategoryBucketId, kCategory);
  SetBucketAsCString(kNameBucketId, kName);

  cmds::TraceBeginCHROMIUM begin_cmd;
  begin_cmd.Init(kCategoryBucketId, kNameBucketId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(begin_cmd));

  end_cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(end_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_3_autogen.h"

}  // namespace gles2
}  // namespace gpu
