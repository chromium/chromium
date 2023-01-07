// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_3.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_3_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_3_AUTOGEN_H_

TEST_P(GLES2DecoderTest3, Uniform4fValidArgs) {
  EXPECT_CALL(*gl_, Uniform4fv(1, 1, _));
  SpecializedSetup<cmds::Uniform4f, 0>(true);
  cmds::Uniform4f cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, Uniform4fvImmediateValidArgs) {
  cmds::Uniform4fvImmediate& cmd = *GetImmediateAs<cmds::Uniform4fvImmediate>();
  SpecializedSetup<cmds::Uniform4fvImmediate, 0>(true);
  GLfloat temp[4 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, Uniform4fv(1, 2, PointsToArray(temp, 4)));
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, Uniform4iValidArgs) {
  EXPECT_CALL(*gl_, Uniform4iv(1, 1, _));
  SpecializedSetup<cmds::Uniform4i, 0>(true);
  cmds::Uniform4i cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, Uniform4ivImmediateValidArgs) {
  cmds::Uniform4ivImmediate& cmd = *GetImmediateAs<cmds::Uniform4ivImmediate>();
  SpecializedSetup<cmds::Uniform4ivImmediate, 0>(true);
  GLint temp[4 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, Uniform4iv(1, 2, PointsToArray(temp, 4)));
  cmd.Init(1, 2, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, UniformMatrix2x3fvImmediateValidArgs) {
  cmds::UniformMatrix2x3fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix2x3fvImmediate>();
  SpecializedSetup<cmds::UniformMatrix2x3fvImmediate, 0>(true);
  GLfloat temp[6 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, UniformMatrix2x3fv(1, 2, true, PointsToArray(temp, 6)));
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, UniformMatrix2x4fvImmediateValidArgs) {
  cmds::UniformMatrix2x4fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix2x4fvImmediate>();
  SpecializedSetup<cmds::UniformMatrix2x4fvImmediate, 0>(true);
  GLfloat temp[8 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, UniformMatrix2x4fv(1, 2, true, PointsToArray(temp, 8)));
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, UniformMatrix3x2fvImmediateValidArgs) {
  cmds::UniformMatrix3x2fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix3x2fvImmediate>();
  SpecializedSetup<cmds::UniformMatrix3x2fvImmediate, 0>(true);
  GLfloat temp[6 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, UniformMatrix3x2fv(1, 2, true, PointsToArray(temp, 6)));
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, UniformMatrix3x4fvImmediateValidArgs) {
  cmds::UniformMatrix3x4fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix3x4fvImmediate>();
  SpecializedSetup<cmds::UniformMatrix3x4fvImmediate, 0>(true);
  GLfloat temp[12 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, UniformMatrix3x4fv(1, 2, true, PointsToArray(temp, 12)));
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, UniformMatrix4x2fvImmediateValidArgs) {
  cmds::UniformMatrix4x2fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix4x2fvImmediate>();
  SpecializedSetup<cmds::UniformMatrix4x2fvImmediate, 0>(true);
  GLfloat temp[8 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, UniformMatrix4x2fv(1, 2, true, PointsToArray(temp, 8)));
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, UniformMatrix4x3fvImmediateValidArgs) {
  cmds::UniformMatrix4x3fvImmediate& cmd =
      *GetImmediateAs<cmds::UniformMatrix4x3fvImmediate>();
  SpecializedSetup<cmds::UniformMatrix4x3fvImmediate, 0>(true);
  GLfloat temp[12 * 2] = {
      0,
  };
  EXPECT_CALL(*gl_, UniformMatrix4x3fv(1, 2, true, PointsToArray(temp, 12)));
  cmd.Init(1, 2, true, &temp[0]);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, UseProgramValidArgs) {
  EXPECT_CALL(*gl_, UseProgram(kServiceProgramId));
  SpecializedSetup<cmds::UseProgram, 0>(true);
  cmds::UseProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, UseProgramInvalidArgs0_0) {
  EXPECT_CALL(*gl_, UseProgram(_)).Times(0);
  SpecializedSetup<cmds::UseProgram, 0>(false);
  cmds::UseProgram cmd;
  cmd.Init(kInvalidClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest3, ValidateProgramValidArgs) {
  EXPECT_CALL(*gl_, ValidateProgram(kServiceProgramId));
  SpecializedSetup<cmds::ValidateProgram, 0>(true);
  cmds::ValidateProgram cmd;
  cmd.Init(client_program_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, VertexAttrib1fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib1f(1, 2));
  SpecializedSetup<cmds::VertexAttrib1f, 0>(true);
  cmds::VertexAttrib1f cmd;
  cmd.Init(1, 2);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, VertexAttrib1fvImmediateValidArgs) {
  cmds::VertexAttrib1fvImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttrib1fvImmediate>();
  SpecializedSetup<cmds::VertexAttrib1fvImmediate, 0>(true);
  GLfloat temp[1] = {
      0,
  };
  cmd.Init(1, &temp[0]);
  EXPECT_CALL(*gl_, VertexAttrib1fv(1, PointsToArray(temp, 1)));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, VertexAttrib2fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib2f(1, 2, 3));
  SpecializedSetup<cmds::VertexAttrib2f, 0>(true);
  cmds::VertexAttrib2f cmd;
  cmd.Init(1, 2, 3);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, VertexAttrib2fvImmediateValidArgs) {
  cmds::VertexAttrib2fvImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttrib2fvImmediate>();
  SpecializedSetup<cmds::VertexAttrib2fvImmediate, 0>(true);
  GLfloat temp[2] = {
      0,
  };
  cmd.Init(1, &temp[0]);
  EXPECT_CALL(*gl_, VertexAttrib2fv(1, PointsToArray(temp, 2)));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, VertexAttrib3fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib3f(1, 2, 3, 4));
  SpecializedSetup<cmds::VertexAttrib3f, 0>(true);
  cmds::VertexAttrib3f cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, VertexAttrib3fvImmediateValidArgs) {
  cmds::VertexAttrib3fvImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttrib3fvImmediate>();
  SpecializedSetup<cmds::VertexAttrib3fvImmediate, 0>(true);
  GLfloat temp[3] = {
      0,
  };
  cmd.Init(1, &temp[0]);
  EXPECT_CALL(*gl_, VertexAttrib3fv(1, PointsToArray(temp, 3)));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, VertexAttrib4fValidArgs) {
  EXPECT_CALL(*gl_, VertexAttrib4f(1, 2, 3, 4, 5));
  SpecializedSetup<cmds::VertexAttrib4f, 0>(true);
  cmds::VertexAttrib4f cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, VertexAttrib4fvImmediateValidArgs) {
  cmds::VertexAttrib4fvImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttrib4fvImmediate>();
  SpecializedSetup<cmds::VertexAttrib4fvImmediate, 0>(true);
  GLfloat temp[4] = {
      0,
  };
  cmd.Init(1, &temp[0]);
  EXPECT_CALL(*gl_, VertexAttrib4fv(1, PointsToArray(temp, 4)));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, VertexAttribI4iValidArgs) {
  EXPECT_CALL(*gl_, VertexAttribI4i(1, 2, 3, 4, 5));
  SpecializedSetup<cmds::VertexAttribI4i, 0>(true);
  cmds::VertexAttribI4i cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, VertexAttribI4ivImmediateValidArgs) {
  cmds::VertexAttribI4ivImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttribI4ivImmediate>();
  SpecializedSetup<cmds::VertexAttribI4ivImmediate, 0>(true);
  GLint temp[4] = {
      0,
  };
  cmd.Init(1, &temp[0]);
  EXPECT_CALL(*gl_, VertexAttribI4iv(1, PointsToArray(temp, 4)));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, VertexAttribI4uiValidArgs) {
  EXPECT_CALL(*gl_, VertexAttribI4ui(1, 2, 3, 4, 5));
  SpecializedSetup<cmds::VertexAttribI4ui, 0>(true);
  cmds::VertexAttribI4ui cmd;
  cmd.Init(1, 2, 3, 4, 5);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest3, VertexAttribI4uivImmediateValidArgs) {
  cmds::VertexAttribI4uivImmediate& cmd =
      *GetImmediateAs<cmds::VertexAttribI4uivImmediate>();
  SpecializedSetup<cmds::VertexAttribI4uivImmediate, 0>(true);
  GLuint temp[4] = {
      0,
  };
  cmd.Init(1, &temp[0]);
  EXPECT_CALL(*gl_, VertexAttribI4uiv(1, PointsToArray(temp, 4)));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(temp)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, ViewportValidArgs) {
  EXPECT_CALL(*gl_, Viewport(1, 2, 3, 4));
  SpecializedSetup<cmds::Viewport, 0>(true);
  cmds::Viewport cmd;
  cmd.Init(1, 2, 3, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, ViewportInvalidArgs2_0) {
  EXPECT_CALL(*gl_, Viewport(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::Viewport, 0>(false);
  cmds::Viewport cmd;
  cmd.Init(1, 2, -1, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest3, ViewportInvalidArgs3_0) {
  EXPECT_CALL(*gl_, Viewport(_, _, _, _)).Times(0);
  SpecializedSetup<cmds::Viewport, 0>(false);
  cmds::Viewport cmd;
  cmd.Init(1, 2, 3, -1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_VALUE, GetGLError());
}

TEST_P(GLES2DecoderTest3, PopGroupMarkerEXTValidArgs) {
  SpecializedSetup<cmds::PopGroupMarkerEXT, 0>(true);
  cmds::PopGroupMarkerEXT cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderTest3, SwapBuffersValidArgs) {
  SpecializedSetup<cmds::SwapBuffers, 0>(true);
  cmds::SwapBuffers cmd;
  cmd.Init(1, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_3_AUTOGEN_H_
