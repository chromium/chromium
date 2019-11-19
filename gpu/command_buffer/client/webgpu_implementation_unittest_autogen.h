// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by webgpu_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_UNITTEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_UNITTEST_AUTOGEN_H_

TEST_F(WebGPUImplementationTest, AssociateMailbox) {
  GLbyte data[16] = {0};
  struct Cmds {
    cmds::AssociateMailboxImmediate cmd;
    GLbyte data[16];
  };

  for (int jj = 0; jj < 16; ++jj) {
    data[jj] = static_cast<GLbyte>(jj);
  }
  Cmds expected;
  expected.cmd.Init(1, 2, 3, 4, 5, &data[0]);
  gl_->AssociateMailbox(1, 2, 3, 4, 5, &data[0]);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(WebGPUImplementationTest, DissociateMailbox) {
  struct Cmds {
    cmds::DissociateMailbox cmd;
  };
  Cmds expected;
  expected.cmd.Init(1, 2);

  gl_->DissociateMailbox(1, 2);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}

TEST_F(WebGPUImplementationTest, RequestAdapter) {
  struct Cmds {
    cmds::RequestAdapter cmd;
  };
  Cmds expected;
  expected.cmd.Init(1);

  gl_->RequestAdapter(PowerPreference::kHighPerformance);
  EXPECT_EQ(0, memcmp(&expected, commands_, sizeof(expected)));
}
#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_UNITTEST_AUTOGEN_H_
