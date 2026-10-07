// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/config/gpu_finch_features.h"

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

// GL_TEXTURE_RECTANGLE_ANGLE is only for internal use and should not be
// reachable through the command buffer.
TEST_F(GLES2WebGLDecoderPassthroughTest, EnableDisableTextureRectangle) {
  {
    cmds::Enable cmd;
    cmd.Init(GL_TEXTURE_RECTANGLE_ANGLE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  }
  {
    cmds::Disable cmd;
    cmd.Init(GL_TEXTURE_RECTANGLE_ANGLE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  }
}

TEST_F(GLES2WebGLDecoderPassthroughTest, ContextVisibilityHintCHROMIUM) {
  {
    cmds::ContextVisibilityHintCHROMIUM cmd;
    cmd.Init(GL_FALSE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
  {
    cmds::ContextVisibilityHintCHROMIUM cmd;
    cmd.Init(GL_TRUE);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }
}

class GLES2WebGLDecoderPassthroughIdleTrimBaseTest
    : public GLES2WebGLDecoderPassthroughTest {
 public:
  void SetUp() override {
    GLES2WebGLDecoderPassthroughTest::SetUp();
    trim_count_ = 0;
    last_trim_level_ = 0;
    prev_get_string_fn_ = gl::g_current_gl_driver->fn.glGetStringFn;
    gl::g_current_gl_driver->fn.glGetStringFn =
        &GLES2WebGLDecoderPassthroughIdleTrimBaseTest::OnGetString;
    prev_trim_memory_fn_ = gl::g_current_gl_driver->fn.glTrimMemoryANGLEFn;
    gl::g_current_gl_driver->fn.glTrimMemoryANGLEFn =
        &GLES2WebGLDecoderPassthroughIdleTrimBaseTest::OnTrimMemoryANGLE;
    GetDecoder()->ForceReinitializeFeatureInfoForTesting();
  }

  void TearDown() override {
    gl::g_current_gl_driver->fn.glGetStringFn = prev_get_string_fn_;
    gl::g_current_gl_driver->fn.glTrimMemoryANGLEFn = prev_trim_memory_fn_;
    prev_get_string_fn_ = nullptr;
    prev_trim_memory_fn_ = nullptr;
    GLES2WebGLDecoderPassthroughTest::TearDown();
  }

 protected:
  static const GLubyte* GL_BINDING_CALL OnGetString(GLenum name) {
    const GLubyte* str = prev_get_string_fn_(name);
    if (name == GL_EXTENSIONS) {
      static base::NoDestructor<std::string> extensions;
      *extensions = str ? reinterpret_cast<const char*>(str) : "";
      *extensions += " GL_ANGLE_trim_memory";
      return reinterpret_cast<const GLubyte*>(extensions->c_str());
    }
    return str;
  }

  static void GL_BINDING_CALL OnTrimMemoryANGLE(GLenum trim_level) {
    trim_count_++;
    last_trim_level_ = trim_level;
  }

  inline static gl::glGetStringProc prev_get_string_fn_ = nullptr;
  inline static int trim_count_ = 0;
  inline static GLenum last_trim_level_ = 0;

 private:
  gl::glTrimMemoryANGLEProc prev_trim_memory_fn_ = nullptr;
};

class GLES2WebGLDecoderPassthroughIdleTrimTest
    : public GLES2WebGLDecoderPassthroughIdleTrimBaseTest {
 public:
  GLES2WebGLDecoderPassthroughIdleTrimTest() {
    feature_list_.InitAndEnableFeature(features::kANGLETrimMemoryOnIdle);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimBaseTest,
       IdleTrimDisabledByDefault) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  task_environment_.FastForwardBy(kIdleTrimDelay);
  EXPECT_EQ(0, trim_count_);
}

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimTest,
       IdleTrimsMemoryAfterOneSecond) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(kIdleTrimDelay / 2);
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(kIdleTrimDelay / 2);
  EXPECT_EQ(1, trim_count_);
  EXPECT_EQ(static_cast<GLenum>(GL_MEMORY_TRIM_HIGH_ANGLE), last_trim_level_);

  task_environment_.FastForwardBy(5 * kIdleTrimDelay);
  EXPECT_EQ(1, trim_count_);
}

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimTest, ActivityReschedulesIdleTrim) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  task_environment_.FastForwardBy(base::Milliseconds(600));
  EXPECT_EQ(0, trim_count_);

  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  task_environment_.FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(base::Milliseconds(899));
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(1, trim_count_);
  EXPECT_EQ(static_cast<GLenum>(GL_MEMORY_TRIM_HIGH_ANGLE), last_trim_level_);
}

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimTest,
       MultipleCommandsInSameFrameTrimAfterOneSecond) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  task_environment_.FastForwardBy(kIdleTrimDelay - base::Milliseconds(1));
  EXPECT_EQ(0, trim_count_);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(1, trim_count_);
}

TEST_F(GLES2WebGLDecoderPassthroughIdleTrimTest, ContextLossCancelsIdleTrim) {
  cmds::Finish cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

  GetDecoder()->MarkContextLost(error::kUnknown);

  task_environment_.FastForwardBy(kIdleTrimDelay);
  EXPECT_EQ(0, trim_count_);
}

}  // namespace gles2
}  // namespace gpu
