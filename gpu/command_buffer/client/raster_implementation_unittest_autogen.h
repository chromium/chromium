// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by raster_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_UNITTEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_UNITTEST_AUTOGEN_H_

TEST_F(RasterImplementationTest, DeleteTextures) {
  GLuint ids[2] = {kTexturesStartId, kTexturesStartId + 1};
  struct Cmds {
    cmds::DeleteTexturesImmediate del;
    GLuint data[2];
  };
  Cmds expected;
  expected.del.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kTexturesStartId;
  expected.data[1] = kTexturesStartId + 1;
  gl_->DeleteTextures(arraysize(ids), &ids[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(RasterImplementationTest, Flush) {
  struct Cmds {
    cmds::Flush cmd;
  };
  Cmds expected;
  expected.cmd.Init();

  gl_->Flush();
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(RasterImplementationTest, GetIntegerv) {
  struct Cmds {
    cmds::GetIntegerv cmd;
  };
  typedef cmds::GetIntegerv::Result::Type ResultType;
  ResultType result = 0;
  Cmds expected;
  ExpectedMemoryInfo result1 =
      GetExpectedResultMemory(sizeof(uint32_t) + sizeof(ResultType));
  expected.cmd.Init(123, result1.id, result1.offset);
  EXPECT_CALL(*command_buffer(), OnFlush())
      .WillOnce(SetMemory(result1.ptr, SizedResultHelper<ResultType>(1)))
      .RetiresOnSaturation();
  gl_->GetIntegerv(123, &result);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
  EXPECT_EQ(static_cast<ResultType>(1), result);
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
  expected.gen.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kQueriesStartId;
  expected.data[1] = kQueriesStartId + 1;
  gl_->GenQueriesEXT(arraysize(ids), &ids[0]);
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
  expected.del.Init(arraysize(ids), &ids[0]);
  expected.data[0] = kQueriesStartId;
  expected.data[1] = kQueriesStartId + 1;
  gl_->DeleteQueriesEXT(arraysize(ids), &ids[0]);
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

TEST_F(RasterImplementationTest, TexParameteri) {
  struct Cmds {
    cmds::TexParameteri cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  gl_->TexParameteri(1, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(RasterImplementationTest, BindTexImage2DCHROMIUM) {
  struct Cmds {
    cmds::BindTexImage2DCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->BindTexImage2DCHROMIUM(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(RasterImplementationTest, ReleaseTexImage2DCHROMIUM) {
  struct Cmds {
    cmds::ReleaseTexImage2DCHROMIUM cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->ReleaseTexImage2DCHROMIUM(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(RasterImplementationTest, TexStorage2D) {
  struct Cmds {
    cmds::TexStorage2D cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3);

  gl_->TexStorage2D(1, 2, 3);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(RasterImplementationTest, CopySubTexture) {
  struct Cmds {
    cmds::CopySubTexture cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5, 6, 7, 8);

  gl_->CopySubTexture(1, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_UNITTEST_AUTOGEN_H_
