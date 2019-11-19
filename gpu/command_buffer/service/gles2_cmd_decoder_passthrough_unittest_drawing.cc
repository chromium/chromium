// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

namespace gpu {
namespace gles2 {

TEST_F(GLES2WebGLDecoderPassthroughTest, DrawArraysInstancedANGLEEnablement) {
  cmds::DrawArraysInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 0, 3, 1);
  EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));

  DoRequestExtension("GL_ANGLE_instanced_arrays");
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2WebGLDecoderPassthroughTest, VertexAttribDivisorANGLEEnablement) {
  cmds::VertexAttribDivisorANGLE cmd;
  cmd.Init(0, 1);
  EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));

  DoRequestExtension("GL_ANGLE_instanced_arrays");
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_F(GLES2WebGLDecoderPassthroughTest, DrawElementsInstancedANGLEEnablement) {
  cmds::DrawElementsInstancedANGLE cmd;
  cmd.Init(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0, 1);
  EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));

  DoRequestExtension("GL_ANGLE_instanced_arrays");
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

}  // namespace gles2
}  // namespace gpu
