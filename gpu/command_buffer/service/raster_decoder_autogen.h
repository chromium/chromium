// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by raster_cmd_decoder.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_AUTOGEN_H_

error::Error RasterDecoderImpl::HandleFinish(uint32_t immediate_data_size,
                                             const volatile void* cmd_data) {
  DoFinish();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleFlush(uint32_t immediate_data_size,
                                            const volatile void* cmd_data) {
  DoFlush();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleGetError(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile raster::cmds::GetError& c =
      *static_cast<const volatile raster::cmds::GetError*>(cmd_data);
  typedef cmds::GetError::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = GetErrorState()->GetGLError();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleGenQueriesEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::GenQueriesEXTImmediate& c =
      *static_cast<const volatile raster::cmds::GenQueriesEXTImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t queries_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&queries_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* queries = gles2::GetImmediateDataAs<volatile GLuint*>(
      c, queries_size, immediate_data_size);
  if (queries == nullptr) {
    return error::kOutOfBounds;
  }
  auto queries_copy = std::make_unique<GLuint[]>(n);
  GLuint* queries_safe = queries_copy.get();
  std::copy(queries, queries + n, queries_safe);
  if (!gles2::CheckUniqueAndNonNullIds(n, queries_safe) ||
      !GenQueriesEXTHelper(n, queries_safe)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleDeleteQueriesEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::DeleteQueriesEXTImmediate& c =
      *static_cast<const volatile raster::cmds::DeleteQueriesEXTImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t queries_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&queries_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* queries =
      gles2::GetImmediateDataAs<volatile const GLuint*>(c, queries_size,
                                                        immediate_data_size);
  if (queries == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteQueriesEXTHelper(n, queries);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleLoseContextCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::LoseContextCHROMIUM& c =
      *static_cast<const volatile raster::cmds::LoseContextCHROMIUM*>(cmd_data);
  GLenum current = static_cast<GLenum>(c.current);
  GLenum other = static_cast<GLenum>(c.other);
  if (!validators_->reset_status.IsValid(current)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glLoseContextCHROMIUM", current,
                                    "current");
    return error::kNoError;
  }
  if (!validators_->reset_status.IsValid(other)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glLoseContextCHROMIUM", other, "other");
    return error::kNoError;
  }
  DoLoseContextCHROMIUM(current, other);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleBeginRasterCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::BeginRasterCHROMIUMImmediate& c =
      *static_cast<const volatile raster::cmds::BeginRasterCHROMIUMImmediate*>(
          cmd_data);
  GLuint sk_color = static_cast<GLuint>(c.sk_color);
  GLuint msaa_sample_count = static_cast<GLuint>(c.msaa_sample_count);
  GLboolean can_use_lcd_text = static_cast<GLboolean>(c.can_use_lcd_text);
  uint32_t mailbox_size;
  if (!gles2::GLES2Util::ComputeDataSize<GLbyte, 16>(1, &mailbox_size)) {
    return error::kOutOfBounds;
  }
  if (mailbox_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox =
      gles2::GetImmediateDataAs<volatile const GLbyte*>(c, mailbox_size,
                                                        immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  DoBeginRasterCHROMIUM(sk_color, msaa_sample_count, can_use_lcd_text, mailbox);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleRasterCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::RasterCHROMIUM& c =
      *static_cast<const volatile raster::cmds::RasterCHROMIUM*>(cmd_data);
  if (!features().chromium_raster_transport) {
    return error::kUnknownCommand;
  }

  GLuint raster_shm_id = static_cast<GLuint>(c.raster_shm_id);
  GLuint raster_shm_offset = static_cast<GLuint>(c.raster_shm_offset);
  GLuint raster_shm_size = static_cast<GLuint>(c.raster_shm_size);
  GLuint font_shm_id = static_cast<GLuint>(c.font_shm_id);
  GLuint font_shm_offset = static_cast<GLuint>(c.font_shm_offset);
  GLuint font_shm_size = static_cast<GLuint>(c.font_shm_size);
  DoRasterCHROMIUM(raster_shm_id, raster_shm_offset, raster_shm_size,
                   font_shm_id, font_shm_offset, font_shm_size);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleEndRasterCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  DoEndRasterCHROMIUM();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleCreateTransferCacheEntryINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::CreateTransferCacheEntryINTERNAL& c =
      *static_cast<
          const volatile raster::cmds::CreateTransferCacheEntryINTERNAL*>(
          cmd_data);
  GLuint entry_type = static_cast<GLuint>(c.entry_type);
  GLuint entry_id = static_cast<GLuint>(c.entry_id);
  GLuint handle_shm_id = static_cast<GLuint>(c.handle_shm_id);
  GLuint handle_shm_offset = static_cast<GLuint>(c.handle_shm_offset);
  GLuint data_shm_id = static_cast<GLuint>(c.data_shm_id);
  GLuint data_shm_offset = static_cast<GLuint>(c.data_shm_offset);
  GLuint data_size = static_cast<GLuint>(c.data_size);
  DoCreateTransferCacheEntryINTERNAL(entry_type, entry_id, handle_shm_id,
                                     handle_shm_offset, data_shm_id,
                                     data_shm_offset, data_size);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleDeleteTransferCacheEntryINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::DeleteTransferCacheEntryINTERNAL& c =
      *static_cast<
          const volatile raster::cmds::DeleteTransferCacheEntryINTERNAL*>(
          cmd_data);
  GLuint entry_type = static_cast<GLuint>(c.entry_type);
  GLuint entry_id = static_cast<GLuint>(c.entry_id);
  DoDeleteTransferCacheEntryINTERNAL(entry_type, entry_id);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleUnlockTransferCacheEntryINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::UnlockTransferCacheEntryINTERNAL& c =
      *static_cast<
          const volatile raster::cmds::UnlockTransferCacheEntryINTERNAL*>(
          cmd_data);
  GLuint entry_type = static_cast<GLuint>(c.entry_type);
  GLuint entry_id = static_cast<GLuint>(c.entry_id);
  DoUnlockTransferCacheEntryINTERNAL(entry_type, entry_id);
  return error::kNoError;
}

error::Error
RasterDecoderImpl::HandleDeletePaintCacheTextBlobsINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::DeletePaintCacheTextBlobsINTERNALImmediate& c =
      *static_cast<const volatile raster::cmds::
                       DeletePaintCacheTextBlobsINTERNALImmediate*>(cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t ids_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&ids_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* ids =
      gles2::GetImmediateDataAs<volatile const GLuint*>(c, ids_size,
                                                        immediate_data_size);
  if (ids == nullptr) {
    return error::kOutOfBounds;
  }
  DeletePaintCacheTextBlobsINTERNALHelper(n, ids);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleDeletePaintCachePathsINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::DeletePaintCachePathsINTERNALImmediate& c =
      *static_cast<
          const volatile raster::cmds::DeletePaintCachePathsINTERNALImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t ids_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&ids_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* ids =
      gles2::GetImmediateDataAs<volatile const GLuint*>(c, ids_size,
                                                        immediate_data_size);
  if (ids == nullptr) {
    return error::kOutOfBounds;
  }
  DeletePaintCachePathsINTERNALHelper(n, ids);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleClearPaintCacheINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  DoClearPaintCacheINTERNAL();
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleCopySubTextureINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::CopySubTextureINTERNALImmediate& c =
      *static_cast<
          const volatile raster::cmds::CopySubTextureINTERNALImmediate*>(
          cmd_data);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  uint32_t mailboxes_size;
  if (!gles2::GLES2Util::ComputeDataSize<GLbyte, 32>(1, &mailboxes_size)) {
    return error::kOutOfBounds;
  }
  if (mailboxes_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailboxes =
      gles2::GetImmediateDataAs<volatile const GLbyte*>(c, mailboxes_size,
                                                        immediate_data_size);
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTextureINTERNAL",
                       "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTextureINTERNAL",
                       "height < 0");
    return error::kNoError;
  }
  if (mailboxes == nullptr) {
    return error::kOutOfBounds;
  }
  DoCopySubTextureINTERNAL(xoffset, yoffset, x, y, width, height, mailboxes);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleTraceEndCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  DoTraceEndCHROMIUM();
  return error::kNoError;
}

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_AUTOGEN_H_
