// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file is included by raster_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_UNITTEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_UNITTEST_AUTOGEN_H_

TEST_F(RasterImplementationTest, Flush) {
  struct Cmds {
    cmds::Flush cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->Flush();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(RasterImplementationTest, GenQueriesEXT) {
  GLuint ids[2] = {
      0,
  };
  struct Cmds {
    cmds::GenQueriesEXTImmediate gen;
    GLuint data[2];
  };
  Cmds expected;
  expected.gen.Init(std::size(ids), &ids[0]);
  expected.data[0] = kQueriesStartId;
  expected.data[1] = kQueriesStartId + 1;
  gl_->GenQueriesEXT(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(kQueriesStartId, ids[0]);
  EXPECT_EQ(kQueriesStartId + 1, ids[1]);
}

TEST_F(RasterImplementationTest, DeleteQueriesEXT) {
  GLuint ids[2] = {kQueriesStartId, kQueriesStartId + 1};
  struct Cmds {
    cmds::DeleteQueriesEXTImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(std::size(ids), &ids[0]);
  expected.data[0] = kQueriesStartId;
  expected.data[1] = kQueriesStartId + 1;
  gl_->DeleteQueriesEXT(std::size(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(RasterImplementationTest, LoseContextCHROMIUM) {
  struct Cmds {
    cmds::LoseContextCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(GL_GUILTY_CONTEXT_RESET_ARB, GL_GUILTY_CONTEXT_RESET_ARB);

  gl_->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                           GL_GUILTY_CONTEXT_RESET_ARB);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_UNITTEST_AUTOGEN_H_
