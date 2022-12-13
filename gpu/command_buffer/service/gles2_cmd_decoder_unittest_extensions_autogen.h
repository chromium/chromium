// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by gles2_cmd_decoder_unittest_extensions.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_EXTENSIONS_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_EXTENSIONS_AUTOGEN_H_

TEST_P(GLES2DecoderTestWithBlendEquationAdvanced, BlendBarrierKHRValidArgs) {
  EXPECT_CALL(*gl_, BlendBarrierKHR());
  SpecializedSetup<cmds::BlendBarrierKHR, 0>(true);
  cmds::BlendBarrierKHR cmd;
  cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_UNITTEST_EXTENSIONS_AUTOGEN_H_
