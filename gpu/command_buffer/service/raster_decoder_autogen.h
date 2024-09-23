// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
  GLfloat r = static_cast<GLfloat>(c.r);
  GLfloat g = static_cast<GLfloat>(c.g);
  GLfloat b = static_cast<GLfloat>(c.b);
  GLfloat a = static_cast<GLfloat>(c.a);
  GLboolean needs_clear = static_cast<GLboolean>(c.needs_clear);
  GLuint msaa_sample_count = static_cast<GLuint>(c.msaa_sample_count);
  gpu::raster::MsaaMode msaa_mode =
      static_cast<gpu::raster::MsaaMode>(c.msaa_mode);
  GLboolean can_use_lcd_text = static_cast<GLboolean>(c.can_use_lcd_text);
  GLboolean visible = static_cast<GLboolean>(c.visible);
  GLfloat hdr_headroom = static_cast<GLfloat>(c.hdr_headroom);
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
  DoBeginRasterCHROMIUM(r, g, b, a, needs_clear, msaa_sample_count, msaa_mode,
                        can_use_lcd_text, visible, hdr_headroom, mailbox);
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

error::Error RasterDecoderImpl::HandleDeletePaintCachePathsINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::DeletePaintCachePathsINTERNAL& c =
      *static_cast<const volatile raster::cmds::DeletePaintCachePathsINTERNAL*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t ids_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&ids_size)) {
    return error::kOutOfBounds;
  }
  const GLuint* ids = GetSharedMemoryAs<const GLuint*>(
      c.ids_shm_id, c.ids_shm_offset, ids_size);
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

error::Error RasterDecoderImpl::HandleCopySharedImageINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::CopySharedImageINTERNALImmediate& c =
      *static_cast<
          const volatile raster::cmds::CopySharedImageINTERNALImmediate*>(
          cmd_data);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLboolean unpack_flip_y = static_cast<GLboolean>(c.unpack_flip_y);
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
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySharedImageINTERNAL",
                       "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySharedImageINTERNAL",
                       "height < 0");
    return error::kNoError;
  }
  if (mailboxes == nullptr) {
    return error::kOutOfBounds;
  }
  DoCopySharedImageINTERNAL(xoffset, yoffset, x, y, width, height,
                            unpack_flip_y, mailboxes);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleWritePixelsINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::WritePixelsINTERNALImmediate& c =
      *static_cast<const volatile raster::cmds::WritePixelsINTERNALImmediate*>(
          cmd_data);
  GLint x_offset = static_cast<GLint>(c.x_offset);
  GLint y_offset = static_cast<GLint>(c.y_offset);
  GLuint src_width = static_cast<GLuint>(c.src_width);
  GLuint src_height = static_cast<GLuint>(c.src_height);
  GLuint src_row_bytes = static_cast<GLuint>(c.src_row_bytes);
  GLuint src_sk_color_type = static_cast<GLuint>(c.src_sk_color_type);
  GLuint src_sk_alpha_type = static_cast<GLuint>(c.src_sk_alpha_type);
  GLint shm_id = static_cast<GLint>(c.shm_id);
  GLuint shm_offset = static_cast<GLuint>(c.shm_offset);
  GLuint pixels_offset = static_cast<GLuint>(c.pixels_offset);
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
  DoWritePixelsINTERNAL(x_offset, y_offset, src_width, src_height,
                        src_row_bytes, src_sk_color_type, src_sk_alpha_type,
                        shm_id, shm_offset, pixels_offset, mailbox);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleWritePixelsYUVINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::WritePixelsYUVINTERNALImmediate& c =
      *static_cast<
          const volatile raster::cmds::WritePixelsYUVINTERNALImmediate*>(
          cmd_data);
  GLuint src_width = static_cast<GLuint>(c.src_width);
  GLuint src_height = static_cast<GLuint>(c.src_height);
  GLuint src_row_bytes_plane1 = static_cast<GLuint>(c.src_row_bytes_plane1);
  GLuint src_row_bytes_plane2 = static_cast<GLuint>(c.src_row_bytes_plane2);
  GLuint src_row_bytes_plane3 = static_cast<GLuint>(c.src_row_bytes_plane3);
  GLuint src_row_bytes_plane4 = static_cast<GLuint>(c.src_row_bytes_plane4);
  GLuint src_yuv_plane_config = static_cast<GLuint>(c.src_yuv_plane_config);
  GLuint src_yuv_subsampling = static_cast<GLuint>(c.src_yuv_subsampling);
  GLuint src_yuv_datatype = static_cast<GLuint>(c.src_yuv_datatype);
  GLint shm_id = static_cast<GLint>(c.shm_id);
  GLuint shm_offset = static_cast<GLuint>(c.shm_offset);
  GLuint plane2_offset = static_cast<GLuint>(c.plane2_offset);
  GLuint plane3_offset = static_cast<GLuint>(c.plane3_offset);
  GLuint plane4_offset = static_cast<GLuint>(c.plane4_offset);
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
  DoWritePixelsYUVINTERNAL(
      src_width, src_height, src_row_bytes_plane1, src_row_bytes_plane2,
      src_row_bytes_plane3, src_row_bytes_plane4, src_yuv_plane_config,
      src_yuv_subsampling, src_yuv_datatype, shm_id, shm_offset, plane2_offset,
      plane3_offset, plane4_offset, mailbox);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleReadbackARGBImagePixelsINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::ReadbackARGBImagePixelsINTERNALImmediate& c =
      *static_cast<const volatile raster::cmds::
                       ReadbackARGBImagePixelsINTERNALImmediate*>(cmd_data);
  GLint src_x = static_cast<GLint>(c.src_x);
  GLint src_y = static_cast<GLint>(c.src_y);
  GLint plane_index = static_cast<GLint>(c.plane_index);
  GLuint dst_width = static_cast<GLuint>(c.dst_width);
  GLuint dst_height = static_cast<GLuint>(c.dst_height);
  GLuint row_bytes = static_cast<GLuint>(c.row_bytes);
  GLuint dst_sk_color_type = static_cast<GLuint>(c.dst_sk_color_type);
  GLuint dst_sk_alpha_type = static_cast<GLuint>(c.dst_sk_alpha_type);
  GLint shm_id = static_cast<GLint>(c.shm_id);
  GLuint shm_offset = static_cast<GLuint>(c.shm_offset);
  GLuint color_space_offset = static_cast<GLuint>(c.color_space_offset);
  GLuint pixels_offset = static_cast<GLuint>(c.pixels_offset);
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
  DoReadbackARGBImagePixelsINTERNAL(src_x, src_y, plane_index, dst_width,
                                    dst_height, row_bytes, dst_sk_color_type,
                                    dst_sk_alpha_type, shm_id, shm_offset,
                                    color_space_offset, pixels_offset, mailbox);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleReadbackYUVImagePixelsINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::ReadbackYUVImagePixelsINTERNALImmediate& c =
      *static_cast<const volatile raster::cmds::
                       ReadbackYUVImagePixelsINTERNALImmediate*>(cmd_data);
  GLuint dst_width = static_cast<GLuint>(c.dst_width);
  GLuint dst_height = static_cast<GLuint>(c.dst_height);
  GLint shm_id = static_cast<GLint>(c.shm_id);
  GLuint shm_offset = static_cast<GLuint>(c.shm_offset);
  GLuint y_offset = static_cast<GLuint>(c.y_offset);
  GLuint y_stride = static_cast<GLuint>(c.y_stride);
  GLuint u_offset = static_cast<GLuint>(c.u_offset);
  GLuint u_stride = static_cast<GLuint>(c.u_stride);
  GLuint v_offset = static_cast<GLuint>(c.v_offset);
  GLuint v_stride = static_cast<GLuint>(c.v_stride);
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
  DoReadbackYUVImagePixelsINTERNAL(dst_width, dst_height, shm_id, shm_offset,
                                   y_offset, y_stride, u_offset, u_stride,
                                   v_offset, v_stride, mailbox);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleTraceEndCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  DoTraceEndCHROMIUM();
  return error::kNoError;
}

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_AUTOGEN_H_
