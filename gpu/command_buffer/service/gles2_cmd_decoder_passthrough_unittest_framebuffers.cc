// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"

namespace gpu {
namespace gles2 {

TEST_F(GLES3DecoderPassthroughTest, ReadPixelsBufferBound) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  GLint size = kWidth * kHeight * kBytesPerPixel;
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, kClientBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size, nullptr, GL_STATIC_DRAW);

  cmds::ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels_shm_id,
           pixels_shm_offset, result_shm_id, result_shm_offset, false);
  result->success = 0;
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES3DecoderPassthroughTest, ReadPixels2PixelPackBufferNoBufferBound) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;

  cmds::ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0, 0, 0, false);
  EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES3DecoderPassthroughTest, ReadPixels2PixelPackBuffer) {
  const GLsizei kWidth = 5;
  const GLsizei kHeight = 3;
  const GLint kBytesPerPixel = 4;
  GLint size = kWidth * kHeight * kBytesPerPixel;

  DoBindBuffer(GL_PIXEL_PACK_BUFFER, kClientBufferId);
  DoBufferData(GL_PIXEL_PACK_BUFFER, size, nullptr, GL_STATIC_DRAW);

  cmds::ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0, 0, 0, false);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_F(GLES2DecoderPassthroughTest, DiscardFramebufferEXTUnsupported) {
  const GLenum target = GL_FRAMEBUFFER;
  const GLsizei count = 1;
  const GLenum attachments[] = {GL_COLOR_EXT};
  auto& cmd = *GetImmediateAs<cmds::DiscardFramebufferEXTImmediate>();
  cmd.Init(target, count, attachments);
  EXPECT_EQ(error::kUnknownCommand,
            ExecuteImmediateCmd(cmd, sizeof(attachments)));
}

TEST_F(GLES2DecoderPassthroughTest, ReadPixelsOutOfRange) {
  const GLint kWidth = 5;
  const GLint kHeight = 3;
  const GLenum kFormat = GL_RGBA;

  // Set up GL objects for the read pixels with a known framebuffer size
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  DoTexImage2D(GL_TEXTURE_2D, 0, kFormat, kWidth, kHeight, 0, kFormat,
               GL_UNSIGNED_BYTE, 0, 0);
  DoBindFramebuffer(GL_FRAMEBUFFER, kClientFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         kClientTextureId, 0);

  // Put the resulting pixels and the result in shared memory
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);

  uint8_t* dest = reinterpret_cast<uint8_t*>(&result[1]);

  // The test cases
  static struct {
    GLint x, y, w, h;
  } tests[] = {
      {
          -2, -1, 9, 5,
      },  // out of range on all sides
      {
          2, 1, 9, 5,
      },  // out of range on right, bottom
      {
          -7, -4, 9, 5,
      },  // out of range on left, top
      {
          0, -5, 9, 5,
      },  // completely off top
      {
          0, 3, 9, 5,
      },  // completely off bottom
      {
          -9, 0, 9, 5,
      },  // completely off left
      {
          5, 0, 9, 5,
      },  // completely off right
  };

  for (auto test : tests) {
    // Clear the readpixels buffer so that we can see which pixels have been
    // written
    memset(dest, 0, 4 * test.w * test.h);

    cmds::ReadPixels cmd;
    cmd.Init(test.x, test.y, test.w, test.h, kFormat, GL_UNSIGNED_BYTE,
             pixels_shm_id, pixels_shm_offset, result_shm_id, result_shm_offset,
             false);
    result->success = 0;
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));

    EXPECT_TRUE(result->success);

    // Check the Result has the correct metadata for what was read.
    GLint startx = std::max(test.x, 0);
    GLint endx = std::min(test.x + test.w, kWidth);
    EXPECT_EQ(result->row_length, endx - startx);

    GLint starty = std::max(test.y, 0);
    GLint endy = std::min(test.y + test.h, kHeight);
    EXPECT_EQ(result->num_rows, endy - starty);

    // Check each pixel and expect them to be non-zero if they were written. The
    // non-zero values are written by ANGLE's NULL backend to simulate the
    // memory that would be modified by the call.
    for (GLint dx = 0; dx < test.w; ++dx) {
      GLint x = test.x + dx;
      for (GLint dy = 0; dy < test.h; ++dy) {
        GLint y = test.y + dy;

        bool expect_written = 0 <= x && x < kWidth && 0 <= y && y < kHeight;
        for (GLint component = 0; component < 4; ++component) {
          uint8_t value = dest[4 * (dy * test.w + dx) + component];
          EXPECT_EQ(expect_written, value != 0)
              << x << " " << y << " " << value;
        }
      }
    }
  }
}

TEST_F(GLES2DecoderPassthroughTest, ReadPixelsAsync) {
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);

  cmds::ReadPixels read_pixels_cmd;
  read_pixels_cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                       pixels_shm_id, pixels_shm_offset, result_shm_id,
                       result_shm_offset, true);
  result->success = 0;

  EXPECT_EQ(error::kNoError, ExecuteCmd(read_pixels_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetDecoder()->HasMoreIdleWork());

  {
    // Verify internals of pending read pixels
    const auto& all_pending_read_pixels = GetPendingReadPixels();
    EXPECT_EQ(1u, all_pending_read_pixels.size());

    const auto& pending_read_pixels = all_pending_read_pixels.front();
    EXPECT_NE(nullptr, pending_read_pixels.fence);
    EXPECT_EQ(pixels_shm_id, pending_read_pixels.pixels_shm_id);
    EXPECT_EQ(pixels_shm_offset, pending_read_pixels.pixels_shm_offset);
    EXPECT_EQ(result_shm_offset, pending_read_pixels.result_shm_offset);
    EXPECT_EQ(result_shm_id, pending_read_pixels.result_shm_id);
    EXPECT_NE(0u, pending_read_pixels.buffer_service_id);
    EXPECT_TRUE(pending_read_pixels.waiting_async_pack_queries.empty());
  }

  cmds::Finish finish_cmd;
  finish_cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(finish_cmd));
  EXPECT_FALSE(GetDecoder()->HasMoreIdleWork());
  EXPECT_TRUE(GetPendingReadPixels().empty());
}

TEST_F(GLES3DecoderPassthroughTest, ReadPixelsAsyncSkippedIfPBOBound) {
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;

  cmds::BindBuffer bind_cmd;
  bind_cmd.Init(GL_PIXEL_PACK_BUFFER, kClientBufferId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bind_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::BufferData buffer_data_cmd;
  size_t read_pixels_result_size = kWidth * kHeight * 4;
  buffer_data_cmd.Init(GL_PIXEL_PACK_BUFFER, read_pixels_result_size, 0, 0,
                       GL_STREAM_DRAW);
  EXPECT_EQ(error::kNoError, ExecuteCmd(buffer_data_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Check that there is no idle work to do when a PBO is already bound and that
  // the ReadPixel succeeded
  cmds::ReadPixels read_pixels_cmd;
  read_pixels_cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0,
                       result_shm_id, result_shm_offset, true);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(read_pixels_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_FALSE(GetDecoder()->HasMoreIdleWork());
}

TEST_F(GLES2DecoderPassthroughTest, ReadPixelsAsyncModifyCommand) {
  size_t shm_size = 0;
  auto* result =
      GetSharedMemoryAsWithSize<cmds::ReadPixels::Result*>(&shm_size);
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);

  size_t pixels_memory_size = shm_size - 1;
  char* pixels = reinterpret_cast<char*>(result + 1);

  constexpr char kDummyValue = 11;
  size_t read_pixels_result_size = kWidth * kHeight * 4;
  EXPECT_GT(pixels_memory_size, read_pixels_result_size);
  memset(pixels, kDummyValue, pixels_memory_size);

  cmds::ReadPixels read_pixels_cmd;
  read_pixels_cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                       pixels_shm_id, pixels_shm_offset, result_shm_id,
                       result_shm_offset, true);
  result->success = 0;

  EXPECT_EQ(error::kNoError, ExecuteCmd(read_pixels_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetDecoder()->HasMoreIdleWork());

  // Change command after ReadPixels issued, but before we finish the read,
  // should have no impact.
  read_pixels_cmd.Init(1, 2, 6, 7, GL_RGB, GL_UNSIGNED_SHORT, pixels_shm_id,
                       pixels_shm_offset, result_shm_id, result_shm_offset,
                       false);

  cmds::Finish finish_cmd;
  finish_cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(finish_cmd));
  EXPECT_FALSE(GetDecoder()->HasMoreIdleWork());
  EXPECT_TRUE(GetPendingReadPixels().empty());

  // Validate that only the correct bytes of pixels have been written to.
  for (size_t i = 0; i < pixels_memory_size; i++) {
    // ANGLE's null context always returns 42 for all pixel bytes for ReadPixels
    // calls.
    constexpr char kReadPixelsValue = 42;
    char expected_value =
        i < read_pixels_result_size ? kReadPixelsValue : kDummyValue;
    EXPECT_EQ(expected_value, pixels[i]);
  }
}

TEST_F(GLES2DecoderPassthroughTest, ReadPixelsAsyncChangePackAlignment) {
  size_t shm_size = 0;
  auto* result =
      GetSharedMemoryAsWithSize<cmds::ReadPixels::Result*>(&shm_size);
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);

  size_t pixels_memory_size = shm_size - 1;
  char* pixels = reinterpret_cast<char*>(result + 1);

  constexpr char kDummyValue = 11;
  size_t read_pixels_result_size = kWidth * kHeight * 4;
  EXPECT_GT(pixels_memory_size, read_pixels_result_size);
  memset(pixels, kDummyValue, pixels_memory_size);

  cmds::ReadPixels read_pixels_cmd;
  read_pixels_cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                       pixels_shm_id, pixels_shm_offset, result_shm_id,
                       result_shm_offset, true);
  result->success = 0;

  EXPECT_EQ(error::kNoError, ExecuteCmd(read_pixels_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(GetDecoder()->HasMoreIdleWork());

  cmds::PixelStorei pixel_store_i_cmd;
  pixel_store_i_cmd.Init(GL_PACK_ALIGNMENT, 8);
  EXPECT_EQ(error::kNoError, ExecuteCmd(pixel_store_i_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::Finish finish_cmd;
  finish_cmd.Init();
  EXPECT_EQ(error::kNoError, ExecuteCmd(finish_cmd));
  EXPECT_FALSE(GetDecoder()->HasMoreIdleWork());
  EXPECT_TRUE(GetPendingReadPixels().empty());

  // Validate that only the correct bytes of pixels have been written to.
  for (size_t i = 0; i < pixels_memory_size; i++) {
    // ANGLE's null context always returns 42 for all pixel bytes for ReadPixels
    // calls.
    constexpr char kReadPixelsValue = 42;
    char expected_value =
        i < read_pixels_result_size ? kReadPixelsValue : kDummyValue;
    EXPECT_EQ(expected_value, pixels[i]);
  }
}

TEST_F(GLES2DecoderPassthroughTest, ReadPixelsAsyncError) {
  auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
  const GLsizei kWidth = 4;
  const GLsizei kHeight = 4;
  uint32_t result_shm_id = shared_memory_id_;
  uint32_t result_shm_offset = kSharedMemoryOffset;
  uint32_t pixels_shm_id = shared_memory_id_;
  uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);

  // Provide parameters that will cause glReadPixels to fail with
  // GL_INVALID_OPERATION
  cmds::ReadPixels cmd;
  cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_SHORT, pixels_shm_id,
           pixels_shm_offset, result_shm_id, result_shm_offset, true);
  result->success = 0;
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  EXPECT_FALSE(GetDecoder()->HasMoreIdleWork());
  EXPECT_TRUE(GetPendingReadPixels().empty());
}

TEST_F(GLES2DecoderPassthroughTest,
       RenderbufferStorageMultisampleEXTNotSupported) {
  DoBindRenderbuffer(GL_RENDERBUFFER, kClientRenderbufferId);
  // GL_EXT_framebuffer_multisample uses RenderbufferStorageMultisampleCHROMIUM.
  cmds::RenderbufferStorageMultisampleEXT cmd;
  cmd.Init(GL_RENDERBUFFER, 1, GL_RGBA4, 1, 1);
  EXPECT_EQ(error::kUnknownCommand, ExecuteCmd(cmd));
}

TEST_F(GLES2DecoderPassthroughTest,
       GetFramebufferAttachmentParameterivWithRenderbuffer) {
  DoBindFramebuffer(GL_FRAMEBUFFER, kClientFramebufferId);
  DoBindRenderbuffer(GL_RENDERBUFFER, kClientRenderbufferId);
  DoFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, kClientRenderbufferId);

  auto* result =
      static_cast<cmds::GetFramebufferAttachmentParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  const GLint* result_value = result->GetData();

  cmds::GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
           GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(kClientRenderbufferId, static_cast<GLuint>(*result_value));
}

TEST_F(GLES2DecoderPassthroughTest,
       GetFramebufferAttachmentParameterivWithTexture) {
  DoBindFramebuffer(GL_FRAMEBUFFER, kClientFramebufferId);
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         kClientTextureId, 0);

  auto* result =
      static_cast<cmds::GetFramebufferAttachmentParameteriv::Result*>(
          shared_memory_address_);
  result->size = 0;
  const GLint* result_value = result->GetData();

  cmds::GetFramebufferAttachmentParameteriv cmd;
  cmd.Init(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
           GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, shared_memory_id_,
           shared_memory_offset_);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(kClientTextureId, static_cast<GLuint>(*result_value));
}

}  // namespace gles2
}  // namespace gpu
