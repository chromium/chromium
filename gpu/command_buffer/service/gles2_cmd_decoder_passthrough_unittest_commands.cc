// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

namespace gpu {
namespace gles2 {

template <typename T>
class GLES2DecoderPassthroughFixedCommandTest
    : public GLES2DecoderPassthroughTest {};
TYPED_TEST_SUITE_P(GLES2DecoderPassthroughFixedCommandTest);

TYPED_TEST_P(GLES2DecoderPassthroughFixedCommandTest, InvalidCommand) {
  TypeParam cmd;
  cmd.SetHeader();
  EXPECT_EQ(error::kUnknownCommand, this->ExecuteCmd(cmd));
}
REGISTER_TYPED_TEST_SUITE_P(GLES2DecoderPassthroughFixedCommandTest,
                            InvalidCommand);

template <typename T>
class GLES2DecoderPassthroughImmediateNoArgCommandTest
    : public GLES2DecoderPassthroughTest {};
TYPED_TEST_SUITE_P(GLES2DecoderPassthroughImmediateNoArgCommandTest);

TYPED_TEST_P(GLES2DecoderPassthroughImmediateNoArgCommandTest, InvalidCommand) {
  auto& cmd = *(this->template GetImmediateAs<TypeParam>());
  cmd.SetHeader();
  EXPECT_EQ(error::kUnknownCommand, this->ExecuteImmediateCmd(cmd, 64));
}
REGISTER_TYPED_TEST_SUITE_P(GLES2DecoderPassthroughImmediateNoArgCommandTest,
                            InvalidCommand);

template <typename T>
class GLES2DecoderPassthroughImmediateSizeArgCommandTest
    : public GLES2DecoderPassthroughTest {};
TYPED_TEST_SUITE_P(GLES2DecoderPassthroughImmediateSizeArgCommandTest);

TYPED_TEST_P(GLES2DecoderPassthroughImmediateSizeArgCommandTest,
             InvalidCommand) {
  auto& cmd = *(this->template GetImmediateAs<TypeParam>());
  cmd.SetHeader(0);
  EXPECT_EQ(error::kUnknownCommand, this->ExecuteImmediateCmd(cmd, 0));
}
REGISTER_TYPED_TEST_SUITE_P(GLES2DecoderPassthroughImmediateSizeArgCommandTest,
                            InvalidCommand);

using ES3FixedCommandTypes0 =
    ::testing::Types<cmds::BindBufferBase,
                     cmds::BindBufferRange,
                     cmds::BindSampler,
                     cmds::BindTransformFeedback,
                     cmds::ClearBufferfi,
                     cmds::ClientWaitSync,
                     cmds::CopyBufferSubData,
                     cmds::CompressedTexImage3D,
                     cmds::CompressedTexImage3DBucket,
                     cmds::CompressedTexSubImage3D,
                     cmds::CompressedTexSubImage3DBucket,
                     cmds::CopyTexSubImage3D,
                     cmds::DeleteSync,
                     cmds::FenceSync,
                     cmds::FlushMappedBufferRange,
                     cmds::FramebufferTextureLayer,
                     cmds::GetActiveUniformBlockiv,
                     cmds::GetActiveUniformBlockName,
                     cmds::GetActiveUniformsiv,
                     cmds::GetFragDataLocation,
                     cmds::GetBufferParameteri64v,
                     cmds::GetInteger64v,
                     cmds::GetInteger64i_v,
                     cmds::GetIntegeri_v,
                     cmds::GetInternalformativ,
                     cmds::GetSamplerParameterfv,
                     cmds::GetSamplerParameteriv,
                     cmds::GetSynciv,
                     cmds::GetUniformBlockIndex,
                     cmds::GetUniformBlocksCHROMIUM,
                     cmds::GetUniformsES3CHROMIUM,
                     cmds::GetTransformFeedbackVarying,
                     cmds::GetTransformFeedbackVaryingsCHROMIUM,
                     cmds::GetUniformuiv,
                     cmds::GetUniformIndices,
                     cmds::GetVertexAttribIiv,
                     cmds::GetVertexAttribIuiv,
                     cmds::IsSampler,
                     cmds::IsSync,
                     cmds::IsTransformFeedback,
                     cmds::MapBufferRange,
                     cmds::PauseTransformFeedback,
                     cmds::ReadBuffer,
                     cmds::ResumeTransformFeedback,
                     cmds::SamplerParameterf,
                     cmds::SamplerParameteri,
                     cmds::TexImage3D,
                     cmds::TexStorage3D,
                     cmds::TexSubImage3D>;

using ES3FixedCommandTypes1 =
    ::testing::Types<cmds::TransformFeedbackVaryingsBucket,
                     cmds::Uniform1ui,
                     cmds::Uniform2ui,
                     cmds::Uniform3ui,
                     cmds::Uniform4ui,
                     cmds::UniformBlockBinding,
                     cmds::UnmapBuffer,
                     cmds::VertexAttribI4i,
                     cmds::VertexAttribI4ui,
                     cmds::VertexAttribIPointer,
                     cmds::WaitSync,
                     cmds::BeginTransformFeedback,
                     cmds::EndTransformFeedback>;

using ES3ImmediateNoArgCommandTypes0 =
    ::testing::Types<cmds::ClearBufferivImmediate,
                     cmds::ClearBufferuivImmediate,
                     cmds::ClearBufferfvImmediate,
                     cmds::SamplerParameterfvImmediate,
                     cmds::SamplerParameterfvImmediate,
                     cmds::VertexAttribI4ivImmediate,
                     cmds::VertexAttribI4uivImmediate>;

using ES3ImmediateSizeArgCommandTypes0 =
    ::testing::Types<cmds::DeleteSamplersImmediate,
                     cmds::DeleteTransformFeedbacksImmediate,
                     cmds::GenTransformFeedbacksImmediate,
                     cmds::InvalidateFramebufferImmediate,
                     cmds::InvalidateSubFramebufferImmediate,
                     cmds::Uniform1uivImmediate,
                     cmds::Uniform2uivImmediate,
                     cmds::Uniform3uivImmediate,
                     cmds::Uniform4uivImmediate,
                     cmds::UniformMatrix2x3fvImmediate,
                     cmds::UniformMatrix2x4fvImmediate,
                     cmds::UniformMatrix3x2fvImmediate,
                     cmds::UniformMatrix3x4fvImmediate,
                     cmds::UniformMatrix4x2fvImmediate,
                     cmds::UniformMatrix4x3fvImmediate>;

INSTANTIATE_TYPED_TEST_SUITE_P(0,
                               GLES2DecoderPassthroughFixedCommandTest,
                               ES3FixedCommandTypes0);
INSTANTIATE_TYPED_TEST_SUITE_P(1,
                               GLES2DecoderPassthroughFixedCommandTest,
                               ES3FixedCommandTypes1);
INSTANTIATE_TYPED_TEST_SUITE_P(0,
                               GLES2DecoderPassthroughImmediateNoArgCommandTest,
                               ES3ImmediateNoArgCommandTypes0);
INSTANTIATE_TYPED_TEST_SUITE_P(
    0,
    GLES2DecoderPassthroughImmediateSizeArgCommandTest,
    ES3ImmediateSizeArgCommandTypes0);

}  // namespace gles2
}  // namespace gpu
