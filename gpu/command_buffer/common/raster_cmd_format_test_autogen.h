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

TEST_F(RasterFormatTest, DeleteTexturesImmediate) {
  static GLuint ids[] = {
      12, 23, 34,
  };
  cmds::DeleteTexturesImmediate& cmd =
      *GetBufferAs<cmds::DeleteTexturesImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteTexturesImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

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

TEST_F(RasterFormatTest, GetIntegerv) {
  cmds::GetIntegerv& cmd = *GetBufferAs<cmds::GetIntegerv>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLenum>(11), static_cast<uint32_t>(12),
              static_cast<uint32_t>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::GetIntegerv::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLenum>(11), cmd.pname);
  EXPECT_EQ(static_cast<uint32_t>(12), cmd.params_shm_id);
  EXPECT_EQ(static_cast<uint32_t>(13), cmd.params_shm_offset);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, GenQueriesEXTImmediate) {
  static GLuint ids[] = {
      12, 23, 34,
  };
  cmds::GenQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::GenQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::GenQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
}

TEST_F(RasterFormatTest, DeleteQueriesEXTImmediate) {
  static GLuint ids[] = {
      12, 23, 34,
  };
  cmds::DeleteQueriesEXTImmediate& cmd =
      *GetBufferAs<cmds::DeleteQueriesEXTImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLsizei>(arraysize(ids)), ids);
  EXPECT_EQ(static_cast<uint32_t>(cmds::DeleteQueriesEXTImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(cmd.n * 4u),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLsizei>(arraysize(ids)), cmd.n);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd,
      sizeof(cmd) + RoundSizeToMultipleOfEntries(arraysize(ids) * 4u));
  EXPECT_EQ(0, memcmp(ids, ImmediateDataAddress(&cmd), sizeof(ids)));
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

TEST_F(RasterFormatTest, InsertFenceSyncCHROMIUM) {
  cmds::InsertFenceSyncCHROMIUM& cmd =
      *GetBufferAs<cmds::InsertFenceSyncCHROMIUM>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint64>(11));
  EXPECT_EQ(static_cast<uint32_t>(cmds::InsertFenceSyncCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint64>(11), cmd.release_count());
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, WaitSyncTokenCHROMIUM) {
  cmds::WaitSyncTokenCHROMIUM& cmd =
      *GetBufferAs<cmds::WaitSyncTokenCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLint>(11), static_cast<GLuint64>(12),
              static_cast<GLuint64>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::WaitSyncTokenCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLint>(11), cmd.namespace_id);
  EXPECT_EQ(static_cast<GLuint64>(12), cmd.command_buffer_id());
  EXPECT_EQ(static_cast<GLuint64>(13), cmd.release_count());
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, UnpremultiplyAndDitherCopyCHROMIUM) {
  cmds::UnpremultiplyAndDitherCopyCHROMIUM& cmd =
      *GetBufferAs<cmds::UnpremultiplyAndDitherCopyCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLint>(13), static_cast<GLint>(14),
              static_cast<GLsizei>(15), static_cast<GLsizei>(16));
  EXPECT_EQ(
      static_cast<uint32_t>(cmds::UnpremultiplyAndDitherCopyCHROMIUM::kCmdId),
      cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.source_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.dest_id);
  EXPECT_EQ(static_cast<GLint>(13), cmd.x);
  EXPECT_EQ(static_cast<GLint>(14), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(15), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(16), cmd.height);
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
              static_cast<GLboolean>(13), static_cast<GLint>(14),
              static_cast<GLuint>(15), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::BeginRasterCHROMIUMImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.sk_color);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.msaa_sample_count);
  EXPECT_EQ(static_cast<GLboolean>(13), cmd.can_use_lcd_text);
  EXPECT_EQ(static_cast<GLint>(14), cmd.color_type);
  EXPECT_EQ(static_cast<GLuint>(15), cmd.color_space_transfer_cache_id);
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

TEST_F(RasterFormatTest, CreateTexture) {
  cmds::CreateTexture& cmd = *GetBufferAs<cmds::CreateTexture>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<bool>(11), static_cast<gfx::BufferUsage>(12),
              static_cast<viz::ResourceFormat>(13), static_cast<uint32_t>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CreateTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<bool>(11), static_cast<bool>(cmd.use_buffer));
  EXPECT_EQ(static_cast<gfx::BufferUsage>(12),
            static_cast<gfx::BufferUsage>(cmd.buffer_usage));
  EXPECT_EQ(static_cast<viz::ResourceFormat>(13),
            static_cast<viz::ResourceFormat>(cmd.format));
  EXPECT_EQ(static_cast<uint32_t>(14), cmd.client_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, SetColorSpaceMetadata) {
  cmds::SetColorSpaceMetadata& cmd =
      *GetBufferAs<cmds::SetColorSpaceMetadata>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLuint>(13), static_cast<GLsizei>(14));
  EXPECT_EQ(static_cast<uint32_t>(cmds::SetColorSpaceMetadata::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.shm_id);
  EXPECT_EQ(static_cast<GLuint>(13), cmd.shm_offset);
  EXPECT_EQ(static_cast<GLsizei>(14), cmd.color_space_size);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, ProduceTextureDirectImmediate) {
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
  cmds::ProduceTextureDirectImmediate& cmd =
      *GetBufferAs<cmds::ProduceTextureDirectImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), data);
  EXPECT_EQ(static_cast<uint32_t>(cmds::ProduceTextureDirectImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture);
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, CreateAndConsumeTextureINTERNALImmediate) {
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
  cmds::CreateAndConsumeTextureINTERNALImmediate& cmd =
      *GetBufferAs<cmds::CreateAndConsumeTextureINTERNALImmediate>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<bool>(12),
                           static_cast<gfx::BufferUsage>(13),
                           static_cast<viz::ResourceFormat>(14), data);
  EXPECT_EQ(static_cast<uint32_t>(
                cmds::CreateAndConsumeTextureINTERNALImmediate::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)),
            cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<bool>(12), static_cast<bool>(cmd.use_buffer));
  EXPECT_EQ(static_cast<gfx::BufferUsage>(13),
            static_cast<gfx::BufferUsage>(cmd.buffer_usage));
  EXPECT_EQ(static_cast<viz::ResourceFormat>(14),
            static_cast<viz::ResourceFormat>(cmd.format));
  CheckBytesWrittenMatchesExpectedSize(
      next_cmd, sizeof(cmd) + RoundSizeToMultipleOfEntries(sizeof(data)));
}

TEST_F(RasterFormatTest, TexParameteri) {
  cmds::TexParameteri& cmd = *GetBufferAs<cmds::TexParameteri>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLenum>(12), static_cast<GLint>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexParameteri::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<GLenum>(12), cmd.pname);
  EXPECT_EQ(static_cast<GLint>(13), cmd.param);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, BindTexImage2DCHROMIUM) {
  cmds::BindTexImage2DCHROMIUM& cmd =
      *GetBufferAs<cmds::BindTexImage2DCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::BindTexImage2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<GLint>(12), cmd.image_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, ReleaseTexImage2DCHROMIUM) {
  cmds::ReleaseTexImage2DCHROMIUM& cmd =
      *GetBufferAs<cmds::ReleaseTexImage2DCHROMIUM>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLint>(12));
  EXPECT_EQ(static_cast<uint32_t>(cmds::ReleaseTexImage2DCHROMIUM::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<GLint>(12), cmd.image_id);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, TexStorage2D) {
  cmds::TexStorage2D& cmd = *GetBufferAs<cmds::TexStorage2D>();
  void* next_cmd = cmd.Set(&cmd, static_cast<GLuint>(11),
                           static_cast<GLsizei>(12), static_cast<GLsizei>(13));
  EXPECT_EQ(static_cast<uint32_t>(cmds::TexStorage2D::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.texture_id);
  EXPECT_EQ(static_cast<GLsizei>(12), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(13), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
}

TEST_F(RasterFormatTest, CopySubTexture) {
  cmds::CopySubTexture& cmd = *GetBufferAs<cmds::CopySubTexture>();
  void* next_cmd =
      cmd.Set(&cmd, static_cast<GLuint>(11), static_cast<GLuint>(12),
              static_cast<GLint>(13), static_cast<GLint>(14),
              static_cast<GLint>(15), static_cast<GLint>(16),
              static_cast<GLsizei>(17), static_cast<GLsizei>(18));
  EXPECT_EQ(static_cast<uint32_t>(cmds::CopySubTexture::kCmdId),
            cmd.header.command);
  EXPECT_EQ(sizeof(cmd), cmd.header.size * 4u);
  EXPECT_EQ(static_cast<GLuint>(11), cmd.source_id);
  EXPECT_EQ(static_cast<GLuint>(12), cmd.dest_id);
  EXPECT_EQ(static_cast<GLint>(13), cmd.xoffset);
  EXPECT_EQ(static_cast<GLint>(14), cmd.yoffset);
  EXPECT_EQ(static_cast<GLint>(15), cmd.x);
  EXPECT_EQ(static_cast<GLint>(16), cmd.y);
  EXPECT_EQ(static_cast<GLsizei>(17), cmd.width);
  EXPECT_EQ(static_cast<GLsizei>(18), cmd.height);
  CheckBytesWrittenMatchesExpectedSize(next_cmd, sizeof(cmd));
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
