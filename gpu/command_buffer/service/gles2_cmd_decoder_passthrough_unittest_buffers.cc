// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

namespace gpu {
namespace gles2 {

TEST_F(GLES3DecoderPassthroughTest, BindBufferBaseValidArgs) {
  GenHelper<cmds::GenBuffersImmediate>(kClientBufferId);
  cmds::BindBufferBase bind_cmd;
  bind_cmd.Init(GL_TRANSFORM_FEEDBACK_BUFFER, 2, kClientBufferId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bind_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES3DecoderPassthroughTest, BindBufferRangeValidArgs) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLintptr kRangeOffset = 4;
  const GLsizeiptr kRangeSize = 8;
  const GLsizeiptr kBufferSize = kRangeOffset + kRangeSize;

  GenHelper<cmds::GenBuffersImmediate>(kClientBufferId);
  cmds::BindBuffer bind_cmd;
  bind_cmd.Init(kTarget, kClientBufferId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bind_cmd));

  cmds::BufferData buffer_data_cmd;
  buffer_data_cmd.Init(kTarget, kBufferSize, 0, 0, GL_STREAM_DRAW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(buffer_data_cmd));

  cmds::BindBufferRange bind_buffer_range_cmd;
  bind_buffer_range_cmd.Init(kTarget, 2, kClientBufferId, kRangeOffset,
                             kRangeSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bind_buffer_range_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES3DecoderPassthroughTest, BindBufferRangeValidArgsWithNoData) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLintptr kRangeOffset = 4;
  const GLsizeiptr kRangeSize = 8;
  GenHelper<cmds::GenBuffersImmediate>(kClientBufferId);
  DoBindBuffer(kTarget, kClientBufferId);
  cmds::BindBufferRange cmd;
  cmd.Init(kTarget, 2, kClientBufferId, kRangeOffset, kRangeSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES3DecoderPassthroughTest, BindBufferRangeValidArgsWithLessData) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLintptr kRangeOffset = 4;
  const GLsizeiptr kRangeSize = 8;
  const GLsizeiptr kBufferSize = kRangeOffset + kRangeSize - 4;
  GenHelper<cmds::GenBuffersImmediate>(kClientBufferId);
  DoBindBuffer(kTarget, kClientBufferId);
  DoBufferData(kTarget, kBufferSize, nullptr, GL_STREAM_DRAW);
  cmds::BindBufferRange cmd;
  cmd.Init(kTarget, 2, kClientBufferId, kRangeOffset, kRangeSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES3DecoderPassthroughTest, CopyBufferSubDataValidArgs) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLsizeiptr kSize = 64;
  const GLsizeiptr kHalfSize = kSize / 2;
  const GLintptr kReadOffset = 0;
  const GLintptr kWriteOffset = kHalfSize;
  const GLsizeiptr kCopySize = 5;
  const char kValue0 = 3;
  const char kValue1 = 21;

  // Set up the buffer so first half is kValue0 and second half is kValue1.
  GenHelper<cmds::GenBuffersImmediate>(kClientBufferId);
  DoBindBuffer(kTarget, kClientBufferId);
  DoBufferData(kTarget, kSize, nullptr, GL_STREAM_DRAW);
  base::HeapArray<char> data = base::HeapArray<char>::Uninit(kHalfSize);
  UNSAFE_TODO(memset(data.data(), kValue0, kHalfSize));
  DoBufferSubData(kTarget, 0, kHalfSize, data.data());
  UNSAFE_TODO(memset(data.data(), kValue1, kHalfSize));
  DoBufferSubData(kTarget, kHalfSize, kHalfSize, data.data());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::CopyBufferSubData cmd;
  cmd.Init(kTarget, kTarget, kReadOffset, kWriteOffset, kCopySize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

}  // namespace gles2
}  // namespace gpu
