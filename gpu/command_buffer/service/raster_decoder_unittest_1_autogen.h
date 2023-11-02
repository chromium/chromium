// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by raster_cmd_decoder_unittest_1.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_UNITTEST_1_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_UNITTEST_1_AUTOGEN_H_

TEST_P(RasterDecoderTest1, GetErrorValidArgs) {
  EXPECT_CALL(*gl_, GetError());
  SpecializedSetup<cmds::GetError, 0>(true);
  cmds::GetError cmd;
  cmd.Init(shared_memory_id_, shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(RasterDecoderTest1, GetErrorInvalidArgsBadSharedMemoryId) {
  EXPECT_CALL(*gl_, GetError()).Times(0);
  SpecializedSetup<cmds::GetError, 0>(false);
  cmds::GetError cmd;
  cmd.Init(kInvalidSharedMemoryId, shared_memory_offset_);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
  cmd.Init(shared_memory_id_, kInvalidSharedMemoryOffset);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_UNITTEST_1_AUTOGEN_H_
