// Copyright 2018 The Chromium Authors. All rights reserved.
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
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(base::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(base::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(base::size(ids) * 4u));
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
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(base::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(base::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(base::size(ids) * 4u));
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
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLboolean>(13), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::BeginRasterCHROMIUMImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sk_color);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.msaa_sample_count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.can_use_lcd_text);
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

TEST_F(RasterFormatTest, DeletePaintCacheTextBlobsINTERNALImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeletePaintCacheTextBlobsINTERNALImmediate& cmd =
      *GetBufferAs<cmds::DeletePaintCacheTextBlobsINTERNALImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(base::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::DeletePaintCacheTextBlobsINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(base::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(base::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(RasterFormatTest, DeletePaintCachePathsINTERNALImmediate) {
  static GLuint ids[] = {
      12,
      23,
      34,
  };
  cmds::DeletePaintCachePathsINTERNALImmediate& cmd =
      *GetBufferAs<cmds::DeletePaintCachePathsINTERNALImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(base::size(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::DeletePaintCachePathsINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(base::size(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(base::size(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
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

TEST_F(RasterFormatTest, CopySubTextureINTERNALImmediate) {
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
  cmds::CopySubTextureINTERNALImmediate& cmd =
      *GetBufferAs<cmds::CopySubTextureINTERNALImmediate>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLint>(12),
              static_cast<GLint>(13), static_cast<GLint>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16), data);
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::CopySubTextureINTERNALImmediate::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(12), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(13), cmd.x);
  EXPECT_EQ(static_cast<GLint>(14), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
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
