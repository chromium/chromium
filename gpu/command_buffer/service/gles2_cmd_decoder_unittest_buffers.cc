// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

using ::gl::MockGLInterface;
using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gpu {
namespace gles2 {

namespace {

}  // namespace anonymous

TEST_P(GLES3DecoderTest, BindBufferBaseValidArgs) {
  EXPECT_CALL(
      *gl_, BindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 2, kServiceBufferId));
  SpecializedSetup<cmds::BindBufferBase, 0>(true);
  cmds::BindBufferBase cmd;
  cmd.Init(GL_TRANSFORM_FEEDBACK_BUFFER, 2, client_buffer_id_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, BindBufferRangeValidArgs) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLintptr kRangeOffset = 4;
  const GLsizeiptr kRangeSize = 8;
  const GLsizeiptr kBufferSize = kRangeOffset + kRangeSize;
  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kBufferSize);
  EXPECT_CALL(*gl_, BindBufferRange(kTarget, 2, kServiceBufferId,
                                    kRangeOffset, kRangeSize));
  SpecializedSetup<cmds::BindBufferRange, 0>(true);
  cmds::BindBufferRange cmd;
  cmd.Init(kTarget, 2, client_buffer_id_, kRangeOffset, kRangeSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, BindBufferRangeValidArgsWithNoData) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLintptr kRangeOffset = 4;
  const GLsizeiptr kRangeSize = 8;
  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  EXPECT_CALL(*gl_, BindBufferBase(kTarget, 2, kServiceBufferId));
  SpecializedSetup<cmds::BindBufferRange, 0>(true);
  cmds::BindBufferRange cmd;
  cmd.Init(kTarget, 2, client_buffer_id_, kRangeOffset, kRangeSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, BindBufferRangeValidArgsWithLessData) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLintptr kRangeOffset = 4;
  const GLsizeiptr kRangeSize = 8;
  const GLsizeiptr kBufferSize = kRangeOffset + kRangeSize - 4;
  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kBufferSize);
  EXPECT_CALL(*gl_, BindBufferRange(kTarget, 2, kServiceBufferId,
                                    kRangeOffset, kRangeSize - 4));
  SpecializedSetup<cmds::BindBufferRange, 0>(true);
  cmds::BindBufferRange cmd;
  cmd.Init(kTarget, 2, client_buffer_id_, kRangeOffset, kRangeSize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, CopyBufferSubDataValidArgs) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLsizeiptr kSize = 64;
  const GLsizeiptr kHalfSize = kSize / 2;
  const GLintptr kReadOffset = 0;
  const GLintptr kWriteOffset = kHalfSize;
  const GLsizeiptr kCopySize = 5;
  const char kValue0 = 3;
  const char kValue1 = 21;

  // Set up the buffer so first half is kValue0 and second half is kValue1.
  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kSize);
  auto data = base::HeapArray<char>::Uninit(kHalfSize);
  UNSAFE_TODO(memset(data.data(), kValue0, kHalfSize));
  DoBufferSubData(kTarget, 0, kHalfSize, data.data());
  UNSAFE_TODO(memset(data.data(), kValue1, kHalfSize));
  DoBufferSubData(kTarget, kHalfSize, kHalfSize, data.data());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  Buffer* buffer = GetBuffer(client_buffer_id_);
  EXPECT_TRUE(buffer);
  const char* shadow_data = reinterpret_cast<const char*>(
      buffer->GetRange(0, kSize));
  EXPECT_TRUE(shadow_data);
  // Verify the shadow data is initialized.
  for (GLsizeiptr ii = 0; ii < kHalfSize; ++ii) {
    UNSAFE_TODO(EXPECT_EQ(kValue0, shadow_data[ii]));
  }
  for (GLsizeiptr ii = kHalfSize; ii < kSize; ++ii) {
    UNSAFE_TODO(EXPECT_EQ(kValue1, shadow_data[ii]));
  }

  EXPECT_CALL(*gl_, CopyBufferSubData(kTarget, kTarget,
                                      kReadOffset, kWriteOffset, kCopySize));
  cmds::CopyBufferSubData cmd;
  cmd.Init(kTarget, kTarget, kReadOffset, kWriteOffset, kCopySize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Verify the shadow data is updated.
  for (GLsizeiptr ii = 0; ii < kHalfSize; ++ii) {
    UNSAFE_TODO(EXPECT_EQ(kValue0, shadow_data[ii]));
  }
  for (GLsizeiptr ii = kHalfSize; ii < kSize; ++ii) {
    if (ii >= kWriteOffset && ii < kWriteOffset + kCopySize) {
      UNSAFE_TODO(EXPECT_EQ(kValue0, shadow_data[ii]));
    } else {
      UNSAFE_TODO(EXPECT_EQ(kValue1, shadow_data[ii]));
    }
  }
}

}  // namespace gles2
}  // namespace gpu
