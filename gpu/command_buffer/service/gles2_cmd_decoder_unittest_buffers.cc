// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

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

TEST_P(GLES3DecoderTest, BindBufferBaseValidArgsNewId) {
  EXPECT_CALL(*gl_,
              BindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 2, kNewServiceId));
  EXPECT_CALL(*gl_, GenBuffersARB(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId));
  SpecializedSetup<cmds::BindBufferBase, 0>(true);
  cmds::BindBufferBase cmd;
  cmd.Init(GL_TRANSFORM_FEEDBACK_BUFFER, 2, kNewClientId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetBuffer(kNewClientId) != nullptr);
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

TEST_P(GLES3DecoderTest, BindBufferRangeValidArgsNewId) {
  EXPECT_CALL(*gl_, BindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 2,
                                    kNewServiceId));
  EXPECT_CALL(*gl_, GenBuffersARB(1, _))
      .WillOnce(SetArgPointee<1>(kNewServiceId));
  SpecializedSetup<cmds::BindBufferRange, 0>(true);
  cmds::BindBufferRange cmd;
  cmd.Init(GL_TRANSFORM_FEEDBACK_BUFFER, 2, kNewClientId, 4, 4);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetBuffer(kNewClientId) != nullptr);
}

TEST_P(GLES3DecoderTest, MapBufferRangeUnmapBufferReadSucceeds) {
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLbitfield kAccess = GL_MAP_READ_BIT;

  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  // uint32_t is Result for both MapBufferRange and UnmapBuffer commands.
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(uint32_t);

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kSize + kOffset);

  std::vector<int8_t> data(kSize);
  for (GLsizeiptr ii = 0; ii < kSize; ++ii) {
    data[ii] = static_cast<int8_t>(ii % 255);
  }

  {  // MapBufferRange
    EXPECT_CALL(*gl_,
                MapBufferRange(kTarget, kOffset, kSize, kAccess))
        .WillOnce(Return(&data[0]))
        .RetiresOnSaturation();

    auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();

    cmds::MapBufferRange cmd;
    cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
             result_shm_id, result_shm_offset);
    *result = 0;
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    int8_t* mem = reinterpret_cast<int8_t*>(&result[1]);
    EXPECT_EQ(0, memcmp(&data[0], mem, kSize));
    EXPECT_EQ(1u, *result);
  }

  {  // UnmapBuffer
    EXPECT_CALL(*gl_, UnmapBuffer(kTarget))
        .WillOnce(Return(GL_TRUE))
        .RetiresOnSaturation();

    cmds::UnmapBuffer cmd;
    cmd.Init(kTarget);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, MapBufferRangeUnmapBufferWriteSucceeds) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLsizeiptr kTotalSize = kOffset + kSize;
  const GLbitfield kAccess = GL_MAP_WRITE_BIT;
  const GLbitfield kMappedAccess = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;

  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  // uint32_t is Result for both MapBufferRange and UnmapBuffer commands.
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(uint32_t);

  auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();
  int8_t* client_data = GetSharedMemoryAs<int8_t*>() + sizeof(uint32_t);

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  Buffer* buffer = GetBuffer(client_buffer_id_);
  EXPECT_TRUE(buffer != nullptr);
  DoBufferData(kTarget, kTotalSize);
  std::vector<int8_t> gpu_data(kTotalSize);
  for (GLsizeiptr ii = 0; ii < kTotalSize; ++ii) {
    gpu_data[ii] = static_cast<int8_t>(ii % 128);
  }
  DoBufferSubData(kTarget, 0, kTotalSize, &gpu_data[0]);

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(buffer->shadowed());
  const int8_t* shadow_data = reinterpret_cast<const int8_t*>(
      buffer->GetRange(0, kTotalSize));
  EXPECT_TRUE(shadow_data);
  // Verify the shadow data is initialized.
  for (GLsizeiptr ii = 0; ii < kTotalSize; ++ii) {
    EXPECT_EQ(static_cast<int8_t>(ii % 128), shadow_data[ii]);
  }

  {  // MapBufferRange succeeds
    EXPECT_CALL(*gl_,
                MapBufferRange(kTarget, kOffset, kSize, kMappedAccess))
        .WillOnce(Return(&gpu_data[kOffset]))
        .RetiresOnSaturation();

    cmds::MapBufferRange cmd;
    cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
             result_shm_id, result_shm_offset);
    *result = 0;
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(1u, *result);
    // Verify the buffer range from GPU is copied to client mem.
    EXPECT_EQ(0, memcmp(&gpu_data[kOffset], client_data, kSize));
  }

  // Update the client mem.
  const int8_t kValue0 = 21;
  memset(client_data, kValue0, kSize);

  {  // UnmapBuffer succeeds
    EXPECT_CALL(*gl_, UnmapBuffer(kTarget))
        .WillOnce(Return(GL_TRUE))
        .RetiresOnSaturation();

    cmds::UnmapBuffer cmd;
    cmd.Init(kTarget);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

    // Verify the GPU mem and shadow data are both updated
    for (GLsizeiptr ii = 0; ii < kTotalSize; ++ii) {
      if (ii < kOffset) {
        EXPECT_EQ(static_cast<int8_t>(ii % 128), gpu_data[ii]);
        EXPECT_EQ(static_cast<int8_t>(ii % 128), shadow_data[ii]);
      } else {
        EXPECT_EQ(kValue0, gpu_data[ii]);
        EXPECT_EQ(kValue0, shadow_data[ii]);
      }
    }
  }

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}


TEST_P(GLES3DecoderTest, FlushMappedBufferRangeSucceeds) {
  const GLenum kTarget = GL_ELEMENT_ARRAY_BUFFER;
  const GLintptr kMappedOffset = 10;
  const GLsizeiptr kMappedSize = 64;
  const GLintptr kFlushRangeOffset = 5;
  const GLsizeiptr kFlushRangeSize = 32;
  const GLsizeiptr kTotalSize = kMappedOffset + kMappedSize;
  const GLbitfield kAccess = GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
  const GLbitfield kMappedAccess = kAccess | GL_MAP_READ_BIT;

  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  // uint32_t is Result for both MapBufferRange and UnmapBuffer commands.
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(uint32_t);

  auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();
  int8_t* client_data = GetSharedMemoryAs<int8_t*>() + sizeof(uint32_t);

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  Buffer* buffer = GetBuffer(client_buffer_id_);
  EXPECT_TRUE(buffer != nullptr);
  DoBufferData(kTarget, kTotalSize);
  std::vector<int8_t> gpu_data(kTotalSize);
  for (GLsizeiptr ii = 0; ii < kTotalSize; ++ii) {
    gpu_data[ii] = static_cast<int8_t>(ii % 128);
  }
  DoBufferSubData(kTarget, 0, kTotalSize, &gpu_data[0]);

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(buffer->shadowed());
  const int8_t* shadow_data = reinterpret_cast<const int8_t*>(
      buffer->GetRange(0, kTotalSize));
  EXPECT_TRUE(shadow_data);
  // Verify the shadow data is initialized.
  for (GLsizeiptr ii = 0; ii < kTotalSize; ++ii) {
    EXPECT_EQ(static_cast<int8_t>(ii % 128), shadow_data[ii]);
  }

  {  // MapBufferRange succeeds
    EXPECT_CALL(*gl_, MapBufferRange(kTarget, kMappedOffset, kMappedSize,
                                     kMappedAccess))
        .WillOnce(Return(&gpu_data[kMappedOffset]))
        .RetiresOnSaturation();

    cmds::MapBufferRange cmd;
    cmd.Init(kTarget, kMappedOffset, kMappedSize, kAccess,
             data_shm_id, data_shm_offset,
             result_shm_id, result_shm_offset);
    *result = 0;
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(1u, *result);
    // Verify the buffer range from GPU is copied to client mem.
    EXPECT_EQ(0, memcmp(&gpu_data[kMappedOffset], client_data, kMappedSize));
  }

  // Update the client mem, including data within and outside the flush range.
  const int8_t kValue0 = 21;
  memset(client_data, kValue0, kTotalSize);

  {  // FlushMappedBufferRange succeeds
    EXPECT_CALL(*gl_, FlushMappedBufferRange(kTarget, kFlushRangeOffset,
                                             kFlushRangeSize))
        .Times(1)
        .RetiresOnSaturation();

    cmds::FlushMappedBufferRange cmd;
    cmd.Init(kTarget, kFlushRangeOffset, kFlushRangeSize);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

    // Verify the GPU mem and shadow data are both updated, but only within
    // the flushed range.
    for (GLsizeiptr ii = 0; ii < kTotalSize; ++ii) {
      if (ii >= kMappedOffset + kFlushRangeOffset &&
          ii < kMappedOffset + kFlushRangeOffset + kFlushRangeSize) {
        EXPECT_EQ(kValue0, gpu_data[ii]);
        EXPECT_EQ(kValue0, shadow_data[ii]);
      } else {
        EXPECT_EQ(static_cast<int8_t>(ii % 128), gpu_data[ii]);
        EXPECT_EQ(static_cast<int8_t>(ii % 128), shadow_data[ii]);
      }
    }
  }

  {  // UnmapBuffer succeeds
    EXPECT_CALL(*gl_, UnmapBuffer(kTarget))
        .WillOnce(Return(GL_TRUE))
        .RetiresOnSaturation();

    cmds::UnmapBuffer cmd;
    cmd.Init(kTarget);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

    // Verify no further update to the GPU mem and shadow data.
    for (GLsizeiptr ii = 0; ii < kTotalSize; ++ii) {
      if (ii >= kMappedOffset + kFlushRangeOffset &&
          ii < kMappedOffset + kFlushRangeOffset + kFlushRangeSize) {
        EXPECT_EQ(kValue0, gpu_data[ii]);
        EXPECT_EQ(kValue0, shadow_data[ii]);
      } else {
        EXPECT_EQ(static_cast<int8_t>(ii % 128), gpu_data[ii]);
        EXPECT_EQ(static_cast<int8_t>(ii % 128), shadow_data[ii]);
      }
    }
  }

  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES3DecoderTest, MapBufferRangeNotInitFails) {
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLbitfield kAccess = GL_MAP_READ_BIT;
  std::vector<int8_t> data(kSize);

  auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();
  *result = 1;  // Any value other than 0.
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(*result);

  cmds::MapBufferRange cmd;
  cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest, MapBufferRangeWriteInvalidateRangeSucceeds) {
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  // With MAP_INVALIDATE_RANGE_BIT, no need to append MAP_READ_BIT.
  const GLbitfield kAccess = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT;

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kSize + kOffset);

  std::vector<int8_t> data(kSize);
  for (GLsizeiptr ii = 0; ii < kSize; ++ii) {
    data[ii] = static_cast<int8_t>(ii % 255);
  }
  EXPECT_CALL(*gl_,
              MapBufferRange(kTarget, kOffset, kSize, kAccess))
      .WillOnce(Return(&data[0]))
      .RetiresOnSaturation();

  auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();
  *result = 0;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(*result);

  int8_t* mem = reinterpret_cast<int8_t*>(&result[1]);
  memset(mem, 72, kSize);  // Init to a random value other than 0.

  cmds::MapBufferRange cmd;
  cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest, MapBufferRangeWriteInvalidateBufferSucceeds) {
  // Test INVALIDATE_BUFFER_BIT is mapped to INVALIDATE_RANGE_BIT.
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLbitfield kAccess = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT;
  // With MAP_INVALIDATE_BUFFER_BIT, no need to append MAP_READ_BIT.
  const GLbitfield kFilteredAccess =
      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT;

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kSize + kOffset);

  std::vector<int8_t> data(kSize);
  for (GLsizeiptr ii = 0; ii < kSize; ++ii) {
    data[ii] = static_cast<int8_t>(ii % 255);
  }
  EXPECT_CALL(*gl_,
              MapBufferRange(kTarget, kOffset, kSize, kFilteredAccess))
      .WillOnce(Return(&data[0]))
      .RetiresOnSaturation();

  auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();
  *result = 0;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(*result);

  int8_t* mem = reinterpret_cast<int8_t*>(&result[1]);
  memset(mem, 72, kSize);  // Init to a random value other than 0.

  cmds::MapBufferRange cmd;
  cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest, MapBufferRangeWriteUnsynchronizedBit) {
  // Test UNSYNCHRONIZED_BIT is filtered out.
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLbitfield kAccess = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
  const GLbitfield kFilteredAccess = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kSize + kOffset);

  std::vector<int8_t> data(kSize);
  for (GLsizeiptr ii = 0; ii < kSize; ++ii) {
    data[ii] = static_cast<int8_t>(ii % 255);
  }
  EXPECT_CALL(*gl_,
              MapBufferRange(kTarget, kOffset, kSize, kFilteredAccess))
      .WillOnce(Return(&data[0]))
      .RetiresOnSaturation();

  auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();
  *result = 0;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(*result);

  int8_t* mem = reinterpret_cast<int8_t*>(&result[1]);
  memset(mem, 72, kSize);  // Init to a random value other than 0.

  cmds::MapBufferRange cmd;
  cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(0, memcmp(&data[0], mem, kSize));
}

TEST_P(GLES3DecoderTest, MapBufferRangeWithError) {
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLbitfield kAccess = GL_MAP_READ_BIT;
  std::vector<int8_t> data(kSize);
  for (GLsizeiptr ii = 0; ii < kSize; ++ii) {
    data[ii] = static_cast<int8_t>(ii % 255);
  }

  auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();
  *result = 0;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(*result);

  int8_t* mem = reinterpret_cast<int8_t*>(&result[1]);
  memset(mem, 72, kSize);  // Init to a random value other than 0.

  cmds::MapBufferRange cmd;
  cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  memset(&data[0], 72, kSize);
  // Mem is untouched.
  EXPECT_EQ(0, memcmp(&data[0], mem, kSize));
  EXPECT_EQ(0u, *result);
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, MapBufferRangeBadSharedMemoryFails) {
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLbitfield kAccess = GL_MAP_READ_BIT;
  std::vector<int8_t> data(kSize);
  for (GLsizeiptr ii = 0; ii < kSize; ++ii) {
    data[ii] = static_cast<int8_t>(ii % 255);
  }

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kOffset + kSize);

  auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();
  *result = 0;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(*result);

  cmds::MapBufferRange cmd;
  cmd.Init(kTarget, kOffset, kSize, kAccess,
           kInvalidSharedMemoryId, data_shm_offset,
           result_shm_id, result_shm_offset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kTarget, kOffset, kSize, kAccess,
           data_shm_id, data_shm_offset,
           kInvalidSharedMemoryId, result_shm_offset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kTarget, kOffset, kSize, kAccess,
           data_shm_id, kInvalidSharedMemoryOffset,
           result_shm_id, result_shm_offset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
  cmd.Init(kTarget, kOffset, kSize, kAccess,
           data_shm_id, data_shm_offset,
           result_shm_id, kInvalidSharedMemoryOffset);
  EXPECT_NE(error::kNoError, ExecuteCmd(cmd));
}

TEST_P(GLES3DecoderTest, UnmapBufferWriteNotMappedFails) {
  const GLenum kTarget = GL_ARRAY_BUFFER;

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);

  cmds::UnmapBuffer cmd;
  cmd.Init(kTarget);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, UnmapBufferWriteNoBoundBufferFails) {
  const GLenum kTarget = GL_ARRAY_BUFFER;

  cmds::UnmapBuffer cmd;
  cmd.Init(kTarget);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_P(GLES3DecoderTest, BufferDataDestroysDataStore) {
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLbitfield kAccess = GL_MAP_WRITE_BIT;
  const GLbitfield kFilteredAccess = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;

  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  // uint32_t is Result for both MapBufferRange and UnmapBuffer commands.
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(uint32_t);

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kSize + kOffset);

  std::vector<int8_t> data(kSize);

  {  // MapBufferRange succeeds
    EXPECT_CALL(*gl_,
                MapBufferRange(kTarget, kOffset, kSize, kFilteredAccess))
        .WillOnce(Return(&data[0]))
        .RetiresOnSaturation();

    auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();

    cmds::MapBufferRange cmd;
    cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
             result_shm_id, result_shm_offset);
    *result = 0;
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(1u, *result);
  }

  {  // BufferData unmaps the data store.
    DoBufferData(kTarget, kSize * 2);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  {  // UnmapBuffer fails.
    cmds::UnmapBuffer cmd;
    cmd.Init(kTarget);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }
}

TEST_P(GLES3DecoderTest, DeleteBuffersDestroysDataStore) {
  const GLenum kTarget = GL_ARRAY_BUFFER;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLbitfield kAccess = GL_MAP_WRITE_BIT;
  const GLbitfield kFilteredAccess = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;

  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  // uint32_t is Result for both MapBufferRange and UnmapBuffer commands.
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(uint32_t);

  DoBindBuffer(kTarget, client_buffer_id_, kServiceBufferId);
  DoBufferData(kTarget, kSize + kOffset);

  std::vector<int8_t> data(kSize);

  {  // MapBufferRange succeeds
    EXPECT_CALL(*gl_,
                MapBufferRange(kTarget, kOffset, kSize, kFilteredAccess))
        .WillOnce(Return(&data[0]))
        .RetiresOnSaturation();

    auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();

    cmds::MapBufferRange cmd;
    cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
             result_shm_id, result_shm_offset);
    *result = 0;
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(1u, *result);
  }

  {  // DeleteBuffers unmaps the data store.
    EXPECT_CALL(*gl_, BindBuffer(kTarget, 0)).Times(1).RetiresOnSaturation();
    EXPECT_CALL(*gl_, UnmapBuffer(kTarget))
        .WillOnce(Return(GL_TRUE))
        .RetiresOnSaturation();
    DoDeleteBuffer(client_buffer_id_, kServiceBufferId);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  {  // UnmapBuffer fails.
    cmds::UnmapBuffer cmd;
    cmd.Init(kTarget);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }
}

TEST_P(GLES3DecoderTest, MapUnmapBufferInvalidTarget) {
  const GLenum kTarget = GL_TEXTURE_2D;
  const GLintptr kOffset = 10;
  const GLsizeiptr kSize = 64;
  const GLbitfield kAccess = GL_MAP_WRITE_BIT;

  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t data_shm_id = shared_memory_id_;
  // uint32_t is Result for both MapBufferRange and UnmapBuffer commands.
  uint32_t data_shm_offset = kSharedMemoryOffset + sizeof(uint32_t);

  auto* result = GetSharedMemoryAs<cmds::MapBufferRange::Result*>();

  {
    cmds::MapBufferRange cmd;
    cmd.Init(kTarget, kOffset, kSize, kAccess, data_shm_id, data_shm_offset,
             result_shm_id, result_shm_offset);
    *result = 0;
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(0u, *result);
    EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  }

  {
    cmds::UnmapBuffer cmd;
    cmd.Init(kTarget);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  }
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
  memset(data.data(), kValue0, kHalfSize);
  DoBufferSubData(kTarget, 0, kHalfSize, data.data());
  memset(data.data(), kValue1, kHalfSize);
  DoBufferSubData(kTarget, kHalfSize, kHalfSize, data.data());
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  Buffer* buffer = GetBuffer(client_buffer_id_);
  EXPECT_TRUE(buffer);
  const char* shadow_data = reinterpret_cast<const char*>(
      buffer->GetRange(0, kSize));
  EXPECT_TRUE(shadow_data);
  // Verify the shadow data is initialized.
  for (GLsizeiptr ii = 0; ii < kHalfSize; ++ii) {
    EXPECT_EQ(kValue0, shadow_data[ii]);
  }
  for (GLsizeiptr ii = kHalfSize; ii < kSize; ++ii) {
    EXPECT_EQ(kValue1, shadow_data[ii]);
  }

  EXPECT_CALL(*gl_, CopyBufferSubData(kTarget, kTarget,
                                      kReadOffset, kWriteOffset, kCopySize));
  cmds::CopyBufferSubData cmd;
  cmd.Init(kTarget, kTarget, kReadOffset, kWriteOffset, kCopySize);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  // Verify the shadow data is updated.
  for (GLsizeiptr ii = 0; ii < kHalfSize; ++ii) {
    EXPECT_EQ(kValue0, shadow_data[ii]);
  }
  for (GLsizeiptr ii = kHalfSize; ii < kSize; ++ii) {
    if (ii >= kWriteOffset && ii < kWriteOffset + kCopySize) {
      EXPECT_EQ(kValue0, shadow_data[ii]);
    } else {
      EXPECT_EQ(kValue1, shadow_data[ii]);
    }
  }
}

}  // namespace gles2
}  // namespace gpu
