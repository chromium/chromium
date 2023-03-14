// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file contains unit tests for raster commands
// It is included by raster_cmd_format_test.cc

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_TEST_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_TEST_AUTOGEN_H_

TEST_F(RasterFormatTest, Finish) {
  cmds::Finish& cmd = *GetBufferAs<cmds::Finish>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Finish::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, Flush) {
  cmds::Flush& cmd = *GetBufferAs<cmds::Flush>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::Flush::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, GetError) {
  cmds::GetError& cmd = *GetBufferAs<cmds::GetError>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<uint32_t>(11), static_cast<uint32_t>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetError::kCmdId), cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<uint32_t>(11), cmd.result_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.result_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, GenQueriesEXTImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::GenQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::GenQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(RasterFormatTest, DeleteQueriesEXTImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeleteQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::DeleteQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(RasterFormatTest, QueryCounterEXT) {
  cmds::QueryCounterEXT& cmd = *GetBufferAs<cmds::QueryCounterEXT>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLenum>(12), static_cast<uint32_t>(13),
                           static_cast<uint32_t>(14), static_cast<GLuint>(15));
  EXPECT_EQ(static_cast<uint32_t>(cmds::QueryCounterEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.id);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.target);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.sync_data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.sync_data_shm_offset);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.submit_count);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, BeginQueryEXT) {
  cmds::BeginQueryEXT& cmd = *GetBufferAs<cmds::BeginQueryEXT>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12),
              static_cast<uint32_t>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BeginQueryEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.sync_data_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.sync_data_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, EndQueryEXT) {
  cmds::EndQueryEXT& cmd = *GetBufferAs<cmds::EndQueryEXT>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::EndQueryEXT::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.target);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.submit_count);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, LoseContextCHROMIUM) {
  cmds::LoseContextCHROMIUM& cmd = *GetBufferAs<cmds::LoseContextCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::LoseContextCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.current);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.other);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, BeginRasterCHROMIUMImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::BeginRasterCHROMIUMImmediate& cmd =
      *GetBufferAs<cmds::BeginRasterCHROMIUMImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLfloat>(11), static_cast<GLfloat>(12),
              static_cast<GLfloat>(13), static_cast<GLfloat>(14),
              static_cast<GLboolean>(15), static_cast<GLuint>(16),
              static_cast<gpu::raster::MsaaMode>(17),
              static_cast<GLboolean>(18), static_cast<GLboolean>(19), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::BeginRasterCHROMIUMImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLfloat>(11), cmd.r);
  EXPECT_EQ(static_cast<GLfloat>(12), cmd.g);
  EXPECT_EQ(static_cast<GLfloat>(13), cmd.b);
  EXPECT_EQ(static_cast<GLfloat>(14), cmd.a);
  EXPECT_EQ(static_cast<GLboolean>(15), cmd.needs_clear);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.msaa_sample_count);
  EXPECT_EQ(static_cast<gpu::raster::MsaaMode>(17), cmd.msaa_mode);
  EXPECT_EQ(static_cast<GLboolean>(18), cmd.can_use_lcd_text);
  EXPECT_EQ(static_cast<GLboolean>(19), cmd.visible);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, RasterCHROMIUM) {
  cmds::RasterCHROMIUM& cmd = *GetBufferAs<cmds::RasterCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLsizeiptr>(13), static_cast<GLuint>(14),
              static_cast<GLuint>(15), static_cast<GLsizeiptr>(16));
  EXPECT_EQ(static_cast<uint32_t>(cmds::RasterCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.raster_shm_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.raster_shm_offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(13), cmd.raster_shm_size);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.font_shm_id);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.font_shm_offset);
  EXPECT_EQ(static_cast<GLsizeiptr>(16), cmd.font_shm_size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, EndRasterCHROMIUM) {
  cmds::EndRasterCHROMIUM& cmd = *GetBufferAs<cmds::EndRasterCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::EndRasterCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, CreateTransferCacheEntryINTERNAL) {
  cmds::CreateTransferCacheEntryINTERNAL& cmd =
      *GetBufferAs<cmds::CreateTransferCacheEntryINTERNAL>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLuint>(12), static_cast<GLuint>(13),
                           static_cast<GLuint>(14), static_cast<GLuint>(15),
                           static_cast<GLuint>(16), static_cast<GLuint>(17));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::CreateTransferCacheEntryINTERNAL::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.entry_type);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.entry_id);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.handle_shm_id);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.handle_shm_offset);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.data_shm_id);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.data_shm_offset);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.data_size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, DeleteTransferCacheEntryINTERNAL) {
  cmds::DeleteTransferCacheEntryINTERNAL& cmd =
      *GetBufferAs<cmds::DeleteTransferCacheEntryINTERNAL>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::DeleteTransferCacheEntryINTERNAL::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.entry_type);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.entry_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, UnlockTransferCacheEntryINTERNAL) {
  cmds::UnlockTransferCacheEntryINTERNAL& cmd =
      *GetBufferAs<cmds::UnlockTransferCacheEntryINTERNAL>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::UnlockTransferCacheEntryINTERNAL::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.entry_type);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.entry_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, DeletePaintCachePathsINTERNALImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeletePaintCachePathsINTERNALImmediate& cmd =
      *GetBufferAs<cmds::DeletePaintCachePathsINTERNALImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(std::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::DeletePaintCachePathsINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(std::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(std::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(RasterFormatTest, DeletePaintCachePathsINTERNAL) {
  cmds::DeletePaintCachePathsINTERNAL& cmd =
      *GetBufferAs<cmds::DeletePaintCachePathsINTERNAL>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLsizei>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeletePaintCachePathsINTERNAL::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(11), cmd.n);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.ids_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.ids_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, ClearPaintCacheINTERNAL) {
  cmds::ClearPaintCacheINTERNAL& cmd =
      *GetBufferAs<cmds::ClearPaintCacheINTERNAL>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::ClearPaintCacheINTERNAL::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, CopySharedImageINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 16),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 17),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 18),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 19),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 20),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 21),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 22),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 23),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 24),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 25),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 26),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 27),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 28),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 29),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 30),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 31),
  };
  cmds::CopySharedImageINTERNALImmediate& cmd =
      *GetBufferAs<cmds::CopySharedImageINTERNALImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
                           static_cast<GLint>(13), static_cast<GLint>(14),
                           static_cast<GLsizei>(15), static_cast<GLsizei>(16),
                           static_cast<GLboolean>(17), data);
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::CopySharedImageINTERNALImmediate::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(12), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(13), cmd.x);
  EXPECT_EQ(static_cast<GLint>(14), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
  EXPECT_EQ(static_cast<GLboolean>(17), cmd.unpack_flip_y);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, WritePixelsINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::WritePixelsINTERNALImmediate& cmd =
      *GetBufferAs<cmds::WritePixelsINTERNALImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLint>(11), static_cast<GLint>(12),
      static_cast<GLuint>(13), static_cast<GLuint>(14), static_cast<GLuint>(15),
      static_cast<GLuint>(16), static_cast<GLuint>(17), static_cast<GLint>(18),
      static_cast<GLuint>(19), static_cast<GLuint>(20), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::WritePixelsINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.x_offset);
  EXPECT_EQ(static_cast<GLint>(12), cmd.y_offset);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.src_width);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.src_height);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.row_bytes);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.src_sk_color_type);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.src_sk_alpha_type);
  EXPECT_EQ(static_cast<GLint>(18), cmd.shm_id);
  EXPECT_EQ(static_cast<GLuint>(19), cmd.shm_offset);
  EXPECT_EQ(static_cast<GLuint>(20), cmd.pixels_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, ReadbackARGBImagePixelsINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::ReadbackARGBImagePixelsINTERNALImmediate& cmd =
      *GetBufferAs<cmds::ReadbackARGBImagePixelsINTERNALImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLint>(11), static_cast<GLint>(12),
      static_cast<GLint>(13), static_cast<GLuint>(14), static_cast<GLuint>(15),
      static_cast<GLuint>(16), static_cast<GLuint>(17), static_cast<GLuint>(18),
      static_cast<GLint>(19), static_cast<GLuint>(20), static_cast<GLuint>(21),
      static_cast<GLuint>(22), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::ReadbackARGBImagePixelsINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.src_x);
  EXPECT_EQ(static_cast<GLint>(12), cmd.src_y);
  EXPECT_EQ(static_cast<GLint>(13), cmd.plane_index);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.dst_width);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.dst_height);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.row_bytes);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.dst_sk_color_type);
  EXPECT_EQ(static_cast<GLuint>(18), cmd.dst_sk_alpha_type);
  EXPECT_EQ(static_cast<GLint>(19), cmd.shm_id);
  EXPECT_EQ(static_cast<GLuint>(20), cmd.shm_offset);
  EXPECT_EQ(static_cast<GLuint>(21), cmd.color_space_offset);
  EXPECT_EQ(static_cast<GLuint>(22), cmd.pixels_offset);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, ReadbackYUVImagePixelsINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
  };
  cmds::ReadbackYUVImagePixelsINTERNALImmediate& cmd =
      *GetBufferAs<cmds::ReadbackYUVImagePixelsINTERNALImmediate>();
  void* next_cmd = cmd.Set(
      &cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
      static_cast<GLint>(13), static_cast<GLuint>(14), static_cast<GLuint>(15),
      static_cast<GLuint>(16), static_cast<GLuint>(17), static_cast<GLuint>(18),
      static_cast<GLuint>(19), static_cast<GLuint>(20), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::ReadbackYUVImagePixelsINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.dst_width);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.dst_height);
  EXPECT_EQ(static_cast<GLint>(13), cmd.shm_id);
  EXPECT_EQ(static_cast<GLuint>(14), cmd.shm_offset);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.y_offset);
  EXPECT_EQ(static_cast<GLuint>(16), cmd.y_stride);
  EXPECT_EQ(static_cast<GLuint>(17), cmd.u_offset);
  EXPECT_EQ(static_cast<GLuint>(18), cmd.u_stride);
  EXPECT_EQ(static_cast<GLuint>(19), cmd.v_offset);
  EXPECT_EQ(static_cast<GLuint>(20), cmd.v_stride);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, ConvertYUVAMailboxesToRGBINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 16),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 17),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 18),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 19),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 20),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 21),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 22),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 23),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 24),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 25),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 26),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 27),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 28),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 29),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 30),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 31),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 32),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 33),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 34),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 35),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 36),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 37),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 38),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 39),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 40),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 41),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 42),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 43),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 44),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 45),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 46),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 47),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 48),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 49),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 50),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 51),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 52),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 53),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 54),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 55),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 56),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 57),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 58),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 59),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 60),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 61),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 62),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 63),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 64),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 65),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 66),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 67),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 68),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 69),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 70),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 71),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 72),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 73),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 74),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 75),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 76),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 77),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 78),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 79),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 80),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 81),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 82),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 83),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 84),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 85),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 86),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 87),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 88),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 89),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 90),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 91),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 92),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 93),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 94),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 95),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 96),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 97),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 98),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 99),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 100),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 101),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 102),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 103),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 104),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 105),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 106),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 107),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 108),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 109),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 110),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 111),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 112),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 113),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 114),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 115),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 116),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 117),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 118),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 119),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 120),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 121),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 122),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 123),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 124),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 125),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 126),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 127),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 128),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 129),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 130),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 131),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 132),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 133),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 134),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 135),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 136),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 137),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 138),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 139),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 140),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 141),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 142),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 143),
  };
  cmds::ConvertYUVAMailboxesToRGBINTERNALImmediate& cmd =
      *GetBufferAs<cmds::ConvertYUVAMailboxesToRGBINTERNALImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::ConvertYUVAMailboxesToRGBINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.planes_yuv_color_space);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.plane_config);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.subsampling);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, ConvertRGBAToYUVAMailboxesINTERNALImmediate) {
  const int kSomeBaseValueToTestWith = 51;
  static GLbyte data[] = {
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 0),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 1),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 2),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 3),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 4),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 5),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 6),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 7),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 8),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 9),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 10),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 11),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 12),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 13),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 14),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 15),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 16),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 17),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 18),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 19),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 20),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 21),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 22),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 23),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 24),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 25),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 26),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 27),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 28),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 29),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 30),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 31),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 32),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 33),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 34),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 35),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 36),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 37),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 38),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 39),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 40),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 41),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 42),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 43),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 44),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 45),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 46),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 47),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 48),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 49),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 50),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 51),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 52),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 53),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 54),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 55),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 56),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 57),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 58),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 59),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 60),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 61),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 62),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 63),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 64),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 65),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 66),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 67),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 68),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 69),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 70),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 71),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 72),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 73),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 74),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 75),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 76),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 77),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 78),
      static_cast<GLbyte>(kSomeBaseValueToTestWith + 79),
  };
  cmds::ConvertRGBAToYUVAMailboxesINTERNALImmediate& cmd =
      *GetBufferAs<cmds::ConvertRGBAToYUVAMailboxesINTERNALImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<GLenum>(12),
              static_cast<GLenum>(13), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::ConvertRGBAToYUVAMailboxesINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.planes_yuv_color_space);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.plane_config);
  EXPECT_EQ(static_cast<GLenum>(13), cmd.subsampling);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, TraceBeginCHROMIUM) {
  cmds::TraceBeginCHROMIUM& cmd = *GetBufferAs<cmds::TraceBeginCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TraceBeginCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.category_bucket_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.name_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, TraceEndCHROMIUM) {
  cmds::TraceEndCHROMIUM& cmd = *GetBufferAs<cmds::TraceEndCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd);
  EXPECT_EQ(static_cast<uint32_t>(cmds::TraceEndCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, SetActiveURLCHROMIUM) {
  cmds::SetActiveURLCHROMIUM& cmd = *GetBufferAs<cmds::SetActiveURLCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::SetActiveURLCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.url_bucket_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_TEST_AUTOGEN_H_
