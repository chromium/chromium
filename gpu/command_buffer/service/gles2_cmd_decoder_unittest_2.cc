// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::AnyNumber;
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

class GLES2DecoderTest2 : public GLES2DecoderTestBase {
 public:
  GLES2DecoderTest2() = default;

  void TestAcceptedUniform(GLenum uniform_type,
                           UniformApiType accepts_apis,
                           bool es3_enabled) {
    SetupShaderForUniform(uniform_type);
    bool valid_uniform = false;

    EXPECT_CALL(*gl_, Uniform1i(1, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform1iv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform2iv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform3iv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform4iv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform1f(1, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform1fv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform2fv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform3fv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, Uniform4fv(1, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, UniformMatrix2fv(1, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, UniformMatrix3fv(1, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(*gl_, UniformMatrix4fv(1, _, _, _)).Times(AnyNumber());
    if (es3_enabled) {
      EXPECT_CALL(*gl_, Uniform1uiv(1, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, Uniform2uiv(1, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, Uniform3uiv(1, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, Uniform4uiv(1, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, UniformMatrix2fv(1, _, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, UniformMatrix3fv(1, _, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, UniformMatrix4fv(1, _, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, UniformMatrix2x3fv(1, _, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, UniformMatrix2x4fv(1, _, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, UniformMatrix3x2fv(1, _, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, UniformMatrix3x4fv(1, _, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, UniformMatrix4x2fv(1, _, _, _)).Times(AnyNumber());
      EXPECT_CALL(*gl_, UniformMatrix4x3fv(1, _, _, _)).Times(AnyNumber());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform1i) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform1i cmd;
      cmd.Init(1, 2);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform1i) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform1ivImmediate& cmd =
          *GetImmediateAs<cmds::Uniform1ivImmediate>();
      GLint data[2][1] = {{0}};
      cmd.Init(1, 2, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform2i) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform2i cmd;
      cmd.Init(1, 2, 3);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform2i) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform2ivImmediate& cmd =
          *GetImmediateAs<cmds::Uniform2ivImmediate>();
      GLint data[2][2] = {{0}};
      cmd.Init(1, 2, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform3i) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform3i cmd;
      cmd.Init(1, 2, 3, 4);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform3i) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform3ivImmediate& cmd =
          *GetImmediateAs<cmds::Uniform3ivImmediate>();
      GLint data[2][3] = {{0}};
      cmd.Init(1, 2, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform4i) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform4i cmd;
      cmd.Init(1, 2, 3, 4, 5);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform4i) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform4ivImmediate& cmd =
          *GetImmediateAs<cmds::Uniform4ivImmediate>();
      GLint data[2][4] = {{0}};
      cmd.Init(1, 2, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    ////////////////////

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform1f) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform1f cmd;
      cmd.Init(1, 2);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform1f) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform1fvImmediate& cmd =
          *GetImmediateAs<cmds::Uniform1fvImmediate>();
      GLfloat data[2][1] = {{0.0f}};
      cmd.Init(1, 2, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform2f) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform2f cmd;
      cmd.Init(1, 2, 3);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform2f) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform2fvImmediate& cmd =
          *GetImmediateAs<cmds::Uniform2fvImmediate>();
      GLfloat data[2][2] = {{0.0f}};
      cmd.Init(1, 2, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform3f) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform3f cmd;
      cmd.Init(1, 2, 3, 4);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform3f) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform3fvImmediate& cmd =
          *GetImmediateAs<cmds::Uniform3fvImmediate>();
      GLfloat data[2][3] = {{0.0f}};
      cmd.Init(1, 2, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform4f) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform4f cmd;
      cmd.Init(1, 2, 3, 4, 5);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniform4f) !=
                      UniformApiType::kUniformNone;
      cmds::Uniform4fvImmediate& cmd =
          *GetImmediateAs<cmds::Uniform4fvImmediate>();
      GLfloat data[2][4] = {{0.0f}};
      cmd.Init(1, 2, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix2f) !=
                      UniformApiType::kUniformNone;
      cmds::UniformMatrix2fvImmediate& cmd =
          *GetImmediateAs<cmds::UniformMatrix2fvImmediate>();
      GLfloat data[2][2 * 2] = {{0.0f}};

      cmd.Init(1, 2, false, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix3f) !=
                      UniformApiType::kUniformNone;
      cmds::UniformMatrix3fvImmediate& cmd =
          *GetImmediateAs<cmds::UniformMatrix3fvImmediate>();
      GLfloat data[2][3 * 3] = {{0.0f}};
      cmd.Init(1, 2, false, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    {
      valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix4f) !=
                      UniformApiType::kUniformNone;
      cmds::UniformMatrix4fvImmediate& cmd =
          *GetImmediateAs<cmds::UniformMatrix4fvImmediate>();
      GLfloat data[2][4 * 4] = {{0.0f}};
      cmd.Init(1, 2, false, &data[0][0]);
      EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
      EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                GetGLError());
    }

    if (!es3_enabled) {
      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix2f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix2fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix2fvImmediate>();
        GLfloat data[2][2 * 2] = {{0.0f}};

        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix3f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix3fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix3fvImmediate>();
        GLfloat data[2][3 * 3] = {{0.0f}};
        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix4f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix4fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix4fvImmediate>();
        GLfloat data[2][4 * 4] = {{0.0f}};
        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
      }
    }

    if (es3_enabled) {
      {
        valid_uniform = (accepts_apis & UniformApiType::kUniform1ui) !=
                        UniformApiType::kUniformNone;
        cmds::Uniform1ui cmd;
        cmd.Init(1, 2);
        EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniform1ui) !=
                        UniformApiType::kUniformNone;
        cmds::Uniform1uivImmediate& cmd =
            *GetImmediateAs<cmds::Uniform1uivImmediate>();
        GLuint data[2][1] = {{0}};
        cmd.Init(1, 2, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniform2ui) !=
                        UniformApiType::kUniformNone;
        cmds::Uniform2ui cmd;
        cmd.Init(1, 2, 3);
        EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniform2ui) !=
                        UniformApiType::kUniformNone;
        cmds::Uniform2uivImmediate& cmd =
            *GetImmediateAs<cmds::Uniform2uivImmediate>();
        GLuint data[2][2] = {{0}};
        cmd.Init(1, 2, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniform3ui) !=
                        UniformApiType::kUniformNone;
        cmds::Uniform3ui cmd;
        cmd.Init(1, 2, 3, 4);
        EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniform3ui) !=
                        UniformApiType::kUniformNone;
        cmds::Uniform3uivImmediate& cmd =
            *GetImmediateAs<cmds::Uniform3uivImmediate>();
        GLuint data[2][3] = {{0}};
        cmd.Init(1, 2, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniform4ui) !=
                        UniformApiType::kUniformNone;
        cmds::Uniform4ui cmd;
        cmd.Init(1, 2, 3, 4, 5);
        EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniform4ui) !=
                        UniformApiType::kUniformNone;
        cmds::Uniform4uivImmediate& cmd =
            *GetImmediateAs<cmds::Uniform4uivImmediate>();
        GLuint data[2][4] = {{0}};
        cmd.Init(1, 2, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix2x3f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix2x3fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix2x3fvImmediate>();
        GLfloat data[2][2 * 3] = {{0.0f}};

        cmd.Init(1, 2, false, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix2x4f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix2x4fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix2x4fvImmediate>();
        GLfloat data[2][2 * 4] = {{0.0f}};

        cmd.Init(1, 2, false, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix3x2f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix3x2fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix3x2fvImmediate>();
        GLfloat data[2][3 * 2] = {{0.0f}};

        cmd.Init(1, 2, false, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix3x4f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix3x4fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix3x4fvImmediate>();
        GLfloat data[2][3 * 4] = {{0.0f}};

        cmd.Init(1, 2, false, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix4x2f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix4x2fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix4x2fvImmediate>();
        GLfloat data[2][4 * 2] = {{0.0f}};

        cmd.Init(1, 2, false, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix4x3f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix4x3fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix4x3fvImmediate>();
        GLfloat data[2][4 * 3] = {{0.0f}};

        cmd.Init(1, 2, false, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix2f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix2fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix2fvImmediate>();
        GLfloat data[2][2 * 2] = {{0.0f}};

        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix3f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix3fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix3fvImmediate>();
        GLfloat data[2][3 * 3] = {{0.0f}};
        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix4f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix4fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix4fvImmediate>();
        GLfloat data[2][4 * 4] = {{0.0f}};
        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix2x3f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix2x3fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix2x3fvImmediate>();
        GLfloat data[2][2 * 3] = {{0.0f}};

        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix2x4f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix2x4fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix2x4fvImmediate>();
        GLfloat data[2][2 * 4] = {{0.0f}};

        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix3x2f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix3x2fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix3x2fvImmediate>();
        GLfloat data[2][3 * 2] = {{0.0f}};

        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix3x4f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix3x4fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix3x4fvImmediate>();
        GLfloat data[2][3 * 4] = {{0.0f}};

        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix4x2f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix4x2fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix4x2fvImmediate>();
        GLfloat data[2][4 * 2] = {{0.0f}};

        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }

      {
        valid_uniform = (accepts_apis & UniformApiType::kUniformMatrix4x3f) !=
                        UniformApiType::kUniformNone;
        cmds::UniformMatrix4x3fvImmediate& cmd =
            *GetImmediateAs<cmds::UniformMatrix4x3fvImmediate>();
        GLfloat data[2][4 * 3] = {{0.0f}};

        cmd.Init(1, 2, true, &data[0][0]);
        EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(data)));
        EXPECT_EQ(valid_uniform ? GL_NO_ERROR : GL_INVALID_OPERATION,
                  GetGLError());
      }
    }
  }
};

class GLES3DecoderTest2 : public GLES2DecoderTest2 {
 public:
  GLES3DecoderTest2() { shader_language_version_ = 300; }
 protected:
  void SetUp() override {
    InitState init;
    init.gl_version = "OpenGL ES 3.0";
    init.bind_generates_resource = true;
    init.context_type = CONTEXT_TYPE_OPENGLES3;
    InitDecoder(init);
  }
};

INSTANTIATE_TEST_SUITE_P(Service, GLES2DecoderTest2, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(Service, GLES3DecoderTest2, ::testing::Bool());

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::GetProgramInfoLog, 0>(
    bool /* valid */) {
  const GLuint kClientVertexShaderId = 5001;
  const GLuint kServiceVertexShaderId = 6001;
  const GLuint kClientFragmentShaderId = 5002;
  const GLuint kServiceFragmentShaderId = 6002;
  const char* log = "hello";  // Matches auto-generated unit test.
  DoCreateShader(
      GL_VERTEX_SHADER, kClientVertexShaderId, kServiceVertexShaderId);
  DoCreateShader(
      GL_FRAGMENT_SHADER, kClientFragmentShaderId, kServiceFragmentShaderId);

  TestHelper::SetShaderStates(
      gl_.get(), GetShader(kClientVertexShaderId), true);
  TestHelper::SetShaderStates(
      gl_.get(), GetShader(kClientFragmentShaderId), true);

  InSequence dummy;
  EXPECT_CALL(*gl_,
              AttachShader(kServiceProgramId, kServiceVertexShaderId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              AttachShader(kServiceProgramId, kServiceFragmentShaderId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, LinkProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(1));
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgPointee<2>(strlen(log) + 1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramInfoLog(kServiceProgramId, strlen(log) + 1, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(strlen(log)),
                      SetArrayArgument<3>(log, log + strlen(log) + 1)))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTES, _))
      .WillOnce(SetArgPointee<2>(0));
  EXPECT_CALL(
      *gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
      .WillOnce(SetArgPointee<2>(0));
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgPointee<2>(0));

  Program* program = GetProgram(client_program_id_);
  ASSERT_TRUE(program != nullptr);

  cmds::AttachShader attach_cmd;
  attach_cmd.Init(client_program_id_, kClientVertexShaderId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));

  attach_cmd.Init(client_program_id_, kClientFragmentShaderId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));

  program->Link(nullptr, Program::kCountOnlyStaticallyUsed, this);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<
    cmds::GetRenderbufferParameteriv, 0>(
        bool /* valid */) {
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::RenderbufferStorage, 0>(
    bool valid) {
  DoBindRenderbuffer(GL_RENDERBUFFER, client_renderbuffer_id_,
                    kServiceRenderbufferId);
  if (valid) {
    EnsureRenderbufferBound(false);
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
                RenderbufferStorageEXT(GL_RENDERBUFFER, _, 3, 4))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::GenQueriesEXTImmediate, 0>(
    bool valid) {
  if (!valid) {
    // Make the client_query_id_ so that trying to make it again
    // will fail.
    cmds::GenQueriesEXTImmediate& cmd =
        *GetImmediateAs<cmds::GenQueriesEXTImmediate>();
    cmd.Init(1, &client_query_id_);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(client_query_id_)));
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::DeleteQueriesEXTImmediate, 0>(
    bool valid) {
  if (valid) {
    // Make the client_query_id_ so that trying to delete it will succeed.
    cmds::GenQueriesEXTImmediate& cmd =
        *GetImmediateAs<cmds::GenQueriesEXTImmediate>();
    cmd.Init(1, &client_query_id_);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd, sizeof(client_query_id_)));
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::LinkProgram, 0>(
    bool /* valid */) {
  const GLuint kClientVertexShaderId = 5001;
  const GLuint kServiceVertexShaderId = 6001;
  const GLuint kClientFragmentShaderId = 5002;
  const GLuint kServiceFragmentShaderId = 6002;
  DoCreateShader(
      GL_VERTEX_SHADER, kClientVertexShaderId, kServiceVertexShaderId);
  DoCreateShader(
      GL_FRAGMENT_SHADER, kClientFragmentShaderId, kServiceFragmentShaderId);

  TestHelper::SetShaderStates(
      gl_.get(), GetShader(kClientVertexShaderId), true);
  TestHelper::SetShaderStates(
      gl_.get(), GetShader(kClientFragmentShaderId), true);

  InSequence dummy;
  EXPECT_CALL(*gl_,
              AttachShader(kServiceProgramId, kServiceVertexShaderId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
              AttachShader(kServiceProgramId, kServiceFragmentShaderId))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_LINK_STATUS, _))
      .WillOnce(SetArgPointee<2>(1));
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTES, _))
      .WillOnce(SetArgPointee<2>(0));
  EXPECT_CALL(
      *gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
      .WillOnce(SetArgPointee<2>(0));
  EXPECT_CALL(*gl_, GetProgramiv(kServiceProgramId, GL_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgPointee<2>(0));

  cmds::AttachShader attach_cmd;
  attach_cmd.Init(client_program_id_, kClientVertexShaderId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));

  attach_cmd.Init(client_program_id_, kClientFragmentShaderId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(attach_cmd));
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform1f, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform1fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform1ivImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2f, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC2);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2i, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC2);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC2);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform2ivImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC2);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3f, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC3);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3i, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC3);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC3);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform3ivImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_INT_VEC3);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::Uniform4f, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_VEC4);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::UniformMatrix2fvImmediate, 0>(
    bool /* valid */) {
  SetupShaderForUniform(GL_FLOAT_MAT2);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameterf, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameteri, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameterfvImmediate, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::TexParameterivImmediate, 0>(
    bool /* valid */) {
  DoBindTexture(GL_TEXTURE_2D, client_texture_id_, kServiceTextureId);
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::GetVertexAttribiv, 0>(
    bool valid) {
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  DoVertexAttribPointer(1, 1, GL_FLOAT, 0, 0);
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::GetVertexAttribfv, 0>(
    bool valid) {
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  DoVertexAttribPointer(1, 1, GL_FLOAT, 0, 0);
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::GetVertexAttribIiv, 0>(
    bool valid) {
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  DoVertexAttribPointer(1, 1, GL_FLOAT, 0, 0);
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
}

template <>
void GLES2DecoderTestBase::SpecializedSetup<cmds::GetVertexAttribIuiv, 0>(
    bool valid) {
  DoBindBuffer(GL_ARRAY_BUFFER, client_buffer_id_, kServiceBufferId);
  DoVertexAttribPointer(1, 1, GL_FLOAT, 0, 0);
  if (valid) {
    EXPECT_CALL(*gl_, GetError())
        .WillOnce(Return(GL_NO_ERROR))
        .WillOnce(Return(GL_NO_ERROR))
        .RetiresOnSaturation();
  }
}

#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest_2_autogen.h"

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_INT) {
  TestAcceptedUniform(GL_INT, UniformApiType::kUniform1i, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_INT_VEC2) {
  TestAcceptedUniform(GL_INT_VEC2, UniformApiType::kUniform2i, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_INT_VEC3) {
  TestAcceptedUniform(GL_INT_VEC3, UniformApiType::kUniform3i, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_INT_VEC4) {
  TestAcceptedUniform(GL_INT_VEC4, UniformApiType::kUniform4i, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_BOOL) {
  TestAcceptedUniform(
      GL_BOOL, UniformApiType::kUniform1i | UniformApiType::kUniform1f, false);
}

TEST_P(GLES3DecoderTest2, AcceptsUniformES3_GL_BOOL) {
  TestAcceptedUniform(GL_BOOL,
                      UniformApiType::kUniform1i | UniformApiType::kUniform1f |
                          UniformApiType::kUniform1ui,
                      true);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_BOOL_VEC2) {
  TestAcceptedUniform(GL_BOOL_VEC2,
                      UniformApiType::kUniform2i | UniformApiType::kUniform2f,
                      false);
}

TEST_P(GLES3DecoderTest2, AcceptsUniformES3_GL_BOOL_VEC2) {
  TestAcceptedUniform(GL_BOOL_VEC2,
                      UniformApiType::kUniform2i | UniformApiType::kUniform2f |
                          UniformApiType::kUniform2ui,
                      true);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_BOOL_VEC3) {
  TestAcceptedUniform(GL_BOOL_VEC3,
                      UniformApiType::kUniform3i | UniformApiType::kUniform3f,
                      false);
}

TEST_P(GLES3DecoderTest2, AcceptsUniformES3_GL_BOOL_VEC3) {
  TestAcceptedUniform(GL_BOOL_VEC3,
                      UniformApiType::kUniform3i | UniformApiType::kUniform3f |
                          UniformApiType::kUniform3ui,
                      true);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_BOOL_VEC4) {
  TestAcceptedUniform(GL_BOOL_VEC4,
                      UniformApiType::kUniform4i | UniformApiType::kUniform4f,
                      false);
}

TEST_P(GLES3DecoderTest2, AcceptsUniformES3_GL_BOOL_VEC4) {
  TestAcceptedUniform(GL_BOOL_VEC4,
                      UniformApiType::kUniform4i | UniformApiType::kUniform4f |
                          UniformApiType::kUniform4ui,
                      true);
}

TEST_P(GLES2DecoderTest2, AcceptsUniformTypeFLOAT) {
  TestAcceptedUniform(GL_FLOAT, UniformApiType::kUniform1f, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_VEC2) {
  TestAcceptedUniform(GL_FLOAT_VEC2, UniformApiType::kUniform2f, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_VEC3) {
  TestAcceptedUniform(GL_FLOAT_VEC3, UniformApiType::kUniform3f, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_VEC4) {
  TestAcceptedUniform(GL_FLOAT_VEC4, UniformApiType::kUniform4f, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_MAT2) {
  TestAcceptedUniform(GL_FLOAT_MAT2, UniformApiType::kUniformMatrix2f, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_MAT3) {
  TestAcceptedUniform(GL_FLOAT_MAT3, UniformApiType::kUniformMatrix3f, false);
}

TEST_P(GLES2DecoderTest2, AcceptsUniform_GL_FLOAT_MAT4) {
  TestAcceptedUniform(GL_FLOAT_MAT4, UniformApiType::kUniformMatrix4f, false);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_UNSIGNED_INT) {
  TestAcceptedUniform(GL_UNSIGNED_INT, UniformApiType::kUniform1ui, true);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_UNSIGNED_INT_VEC2) {
  TestAcceptedUniform(GL_UNSIGNED_INT_VEC2, UniformApiType::kUniform2ui, true);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_UNSIGNED_INT_VEC3) {
  TestAcceptedUniform(GL_UNSIGNED_INT_VEC3, UniformApiType::kUniform3ui, true);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_UNSIGNED_INT_VEC4) {
  TestAcceptedUniform(GL_UNSIGNED_INT_VEC4, UniformApiType::kUniform4ui, true);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_FLOAT_MAT2x3) {
  TestAcceptedUniform(GL_FLOAT_MAT2x3, UniformApiType::kUniformMatrix2x3f,
                      true);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_FLOAT_MAT2x4) {
  TestAcceptedUniform(GL_FLOAT_MAT2x4, UniformApiType::kUniformMatrix2x4f,
                      true);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_FLOAT_MAT3x2) {
  TestAcceptedUniform(GL_FLOAT_MAT3x2, UniformApiType::kUniformMatrix3x2f,
                      true);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_FLOAT_MAT3x4) {
  TestAcceptedUniform(GL_FLOAT_MAT3x4, UniformApiType::kUniformMatrix3x4f,
                      true);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_FLOAT_MAT4x2) {
  TestAcceptedUniform(GL_FLOAT_MAT4x2, UniformApiType::kUniformMatrix4x2f,
                      true);
}

TEST_P(GLES3DecoderTest2, AcceptsUniform_GL_FLOAT_MAT4x3) {
  TestAcceptedUniform(GL_FLOAT_MAT4x3, UniformApiType::kUniformMatrix4x3f,
                      true);
}

}  // namespace gles2
}  // namespace gpu
