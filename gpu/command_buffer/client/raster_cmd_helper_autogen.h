// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_CMD_HELPER_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_CMD_HELPER_AUTOGEN_H_

void Finish() {
  raster::cmds::Finish* c = GetCmdSpace<raster::cmds::Finish>();
  if (c) {
    c->Init();
  }
}

void Flush() {
  raster::cmds::Flush* c = GetCmdSpace<raster::cmds::Flush>();
  if (c) {
    c->Init();
  }
}

void GetError(uint32_t result_shm_id, uint32_t result_shm_offset) {
  raster::cmds::GetError* c = GetCmdSpace<raster::cmds::GetError>();
  if (c) {
    c->Init(result_shm_id, result_shm_offset);
  }
}

void GenQueriesEXTImmediate(GLsizei n, GLuint* queries) {
  const uint32_t size = raster::cmds::GenQueriesEXTImmediate::ComputeSize(n);
  raster::cmds::GenQueriesEXTImmediate* c =
      GetImmediateCmdSpaceTotalSize<raster::cmds::GenQueriesEXTImmediate>(size);
  if (c) {
    c->Init(n, queries);
  }
}

void DeleteQueriesEXTImmediate(GLsizei n, const GLuint* queries) {
  const uint32_t size = raster::cmds::DeleteQueriesEXTImmediate::ComputeSize(n);
  raster::cmds::DeleteQueriesEXTImmediate* c =
      GetImmediateCmdSpaceTotalSize<raster::cmds::DeleteQueriesEXTImmediate>(
          size);
  if (c) {
    c->Init(n, queries);
  }
}

void QueryCounterEXT(GLuint id,
                     GLenum target,
                     uint32_t sync_data_shm_id,
                     uint32_t sync_data_shm_offset,
                     GLuint submit_count) {
  raster::cmds::QueryCounterEXT* c =
      GetCmdSpace<raster::cmds::QueryCounterEXT>();
  if (c) {
    c->Init(id, target, sync_data_shm_id, sync_data_shm_offset, submit_count);
  }
}

void BeginQueryEXT(GLenum target,
                   GLuint id,
                   uint32_t sync_data_shm_id,
                   uint32_t sync_data_shm_offset) {
  raster::cmds::BeginQueryEXT* c = GetCmdSpace<raster::cmds::BeginQueryEXT>();
  if (c) {
    c->Init(target, id, sync_data_shm_id, sync_data_shm_offset);
  }
}

void EndQueryEXT(GLenum target, GLuint submit_count) {
  raster::cmds::EndQueryEXT* c = GetCmdSpace<raster::cmds::EndQueryEXT>();
  if (c) {
    c->Init(target, submit_count);
  }
}

void LoseContextCHROMIUM(GLenum current, GLenum other) {
  raster::cmds::LoseContextCHROMIUM* c =
      GetCmdSpace<raster::cmds::LoseContextCHROMIUM>();
  if (c) {
    c->Init(current, other);
  }
}

void BeginRasterCHROMIUMImmediate(GLfloat r,
                                  GLfloat g,
                                  GLfloat b,
                                  GLfloat a,
                                  GLboolean needs_clear,
                                  GLuint msaa_sample_count,
                                  gpu::raster::MsaaMode msaa_mode,
                                  GLboolean can_use_lcd_text,
                                  GLboolean visible,
                                  GLfloat hdr_headroom,
                                  const GLbyte* mailbox) {
  const uint32_t size =
      raster::cmds::BeginRasterCHROMIUMImmediate::ComputeSize();
  raster::cmds::BeginRasterCHROMIUMImmediate* c =
      GetImmediateCmdSpaceTotalSize<raster::cmds::BeginRasterCHROMIUMImmediate>(
          size);
  if (c) {
    c->Init(r, g, b, a, needs_clear, msaa_sample_count, msaa_mode,
            can_use_lcd_text, visible, hdr_headroom, mailbox);
  }
}

void RasterCHROMIUM(GLuint raster_shm_id,
                    GLuint raster_shm_offset,
                    GLsizeiptr raster_shm_size,
                    GLuint font_shm_id,
                    GLuint font_shm_offset,
                    GLsizeiptr font_shm_size) {
  raster::cmds::RasterCHROMIUM* c = GetCmdSpace<raster::cmds::RasterCHROMIUM>();
  if (c) {
    c->Init(raster_shm_id, raster_shm_offset, raster_shm_size, font_shm_id,
            font_shm_offset, font_shm_size);
  }
}

void EndRasterCHROMIUM() {
  raster::cmds::EndRasterCHROMIUM* c =
      GetCmdSpace<raster::cmds::EndRasterCHROMIUM>();
  if (c) {
    c->Init();
  }
}

void CreateTransferCacheEntryINTERNAL(GLuint entry_type,
                                      GLuint entry_id,
                                      GLuint handle_shm_id,
                                      GLuint handle_shm_offset,
                                      GLuint data_shm_id,
                                      GLuint data_shm_offset,
                                      GLuint data_size) {
  raster::cmds::CreateTransferCacheEntryINTERNAL* c =
      GetCmdSpace<raster::cmds::CreateTransferCacheEntryINTERNAL>();
  if (c) {
    c->Init(entry_type, entry_id, handle_shm_id, handle_shm_offset, data_shm_id,
            data_shm_offset, data_size);
  }
}

void DeleteTransferCacheEntryINTERNAL(GLuint entry_type, GLuint entry_id) {
  raster::cmds::DeleteTransferCacheEntryINTERNAL* c =
      GetCmdSpace<raster::cmds::DeleteTransferCacheEntryINTERNAL>();
  if (c) {
    c->Init(entry_type, entry_id);
  }
}

void UnlockTransferCacheEntryINTERNAL(GLuint entry_type, GLuint entry_id) {
  raster::cmds::UnlockTransferCacheEntryINTERNAL* c =
      GetCmdSpace<raster::cmds::UnlockTransferCacheEntryINTERNAL>();
  if (c) {
    c->Init(entry_type, entry_id);
  }
}

void DeletePaintCachePathsINTERNALImmediate(GLsizei n, const GLuint* ids) {
  const uint32_t size =
      raster::cmds::DeletePaintCachePathsINTERNALImmediate::ComputeSize(n);
  raster::cmds::DeletePaintCachePathsINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          raster::cmds::DeletePaintCachePathsINTERNALImmediate>(size);
  if (c) {
    c->Init(n, ids);
  }
}

void DeletePaintCachePathsINTERNAL(GLsizei n,
                                   uint32_t ids_shm_id,
                                   uint32_t ids_shm_offset) {
  raster::cmds::DeletePaintCachePathsINTERNAL* c =
      GetCmdSpace<raster::cmds::DeletePaintCachePathsINTERNAL>();
  if (c) {
    c->Init(n, ids_shm_id, ids_shm_offset);
  }
}

void ClearPaintCacheINTERNAL() {
  raster::cmds::ClearPaintCacheINTERNAL* c =
      GetCmdSpace<raster::cmds::ClearPaintCacheINTERNAL>();
  if (c) {
    c->Init();
  }
}

void CopySharedImageINTERNALImmediate(GLint xoffset,
                                      GLint yoffset,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height,
                                      GLboolean unpack_flip_y,
                                      const GLbyte* mailboxes) {
  const uint32_t size =
      raster::cmds::CopySharedImageINTERNALImmediate::ComputeSize();
  raster::cmds::CopySharedImageINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          raster::cmds::CopySharedImageINTERNALImmediate>(size);
  if (c) {
    c->Init(xoffset, yoffset, x, y, width, height, unpack_flip_y, mailboxes);
  }
}

void WritePixelsINTERNALImmediate(GLint x_offset,
                                  GLint y_offset,
                                  GLuint src_width,
                                  GLuint src_height,
                                  GLuint src_row_bytes,
                                  GLuint src_sk_color_type,
                                  GLuint src_sk_alpha_type,
                                  GLint shm_id,
                                  GLuint shm_offset,
                                  GLuint pixels_offset,
                                  const GLbyte* mailbox) {
  const uint32_t size =
      raster::cmds::WritePixelsINTERNALImmediate::ComputeSize();
  raster::cmds::WritePixelsINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<raster::cmds::WritePixelsINTERNALImmediate>(
          size);
  if (c) {
    c->Init(x_offset, y_offset, src_width, src_height, src_row_bytes,
            src_sk_color_type, src_sk_alpha_type, shm_id, shm_offset,
            pixels_offset, mailbox);
  }
}

void WritePixelsYUVINTERNALImmediate(GLuint src_width,
                                     GLuint src_height,
                                     GLuint src_row_bytes_plane1,
                                     GLuint src_row_bytes_plane2,
                                     GLuint src_row_bytes_plane3,
                                     GLuint src_row_bytes_plane4,
                                     GLuint src_yuv_plane_config,
                                     GLuint src_yuv_subsampling,
                                     GLuint src_yuv_datatype,
                                     GLint shm_id,
                                     GLuint shm_offset,
                                     GLuint plane2_offset,
                                     GLuint plane3_offset,
                                     GLuint plane4_offset,
                                     const GLbyte* mailbox) {
  const uint32_t size =
      raster::cmds::WritePixelsYUVINTERNALImmediate::ComputeSize();
  raster::cmds::WritePixelsYUVINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          raster::cmds::WritePixelsYUVINTERNALImmediate>(size);
  if (c) {
    c->Init(src_width, src_height, src_row_bytes_plane1, src_row_bytes_plane2,
            src_row_bytes_plane3, src_row_bytes_plane4, src_yuv_plane_config,
            src_yuv_subsampling, src_yuv_datatype, shm_id, shm_offset,
            plane2_offset, plane3_offset, plane4_offset, mailbox);
  }
}

void ReadbackARGBImagePixelsINTERNALImmediate(GLint src_x,
                                              GLint src_y,
                                              GLint plane_index,
                                              GLuint dst_width,
                                              GLuint dst_height,
                                              GLuint row_bytes,
                                              GLuint dst_sk_color_type,
                                              GLuint dst_sk_alpha_type,
                                              GLint shm_id,
                                              GLuint shm_offset,
                                              GLuint color_space_offset,
                                              GLuint pixels_offset,
                                              const GLbyte* mailbox) {
  const uint32_t size =
      raster::cmds::ReadbackARGBImagePixelsINTERNALImmediate::ComputeSize();
  raster::cmds::ReadbackARGBImagePixelsINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          raster::cmds::ReadbackARGBImagePixelsINTERNALImmediate>(size);
  if (c) {
    c->Init(src_x, src_y, plane_index, dst_width, dst_height, row_bytes,
            dst_sk_color_type, dst_sk_alpha_type, shm_id, shm_offset,
            color_space_offset, pixels_offset, mailbox);
  }
}

void ReadbackYUVImagePixelsINTERNALImmediate(GLuint dst_width,
                                             GLuint dst_height,
                                             GLint shm_id,
                                             GLuint shm_offset,
                                             GLuint y_offset,
                                             GLuint y_stride,
                                             GLuint u_offset,
                                             GLuint u_stride,
                                             GLuint v_offset,
                                             GLuint v_stride,
                                             const GLbyte* mailbox) {
  const uint32_t size =
      raster::cmds::ReadbackYUVImagePixelsINTERNALImmediate::ComputeSize();
  raster::cmds::ReadbackYUVImagePixelsINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          raster::cmds::ReadbackYUVImagePixelsINTERNALImmediate>(size);
  if (c) {
    c->Init(dst_width, dst_height, shm_id, shm_offset, y_offset, y_stride,
            u_offset, u_stride, v_offset, v_stride, mailbox);
  }
}

void TraceBeginCHROMIUM(GLuint category_bucket_id, GLuint name_bucket_id) {
  raster::cmds::TraceBeginCHROMIUM* c =
      GetCmdSpace<raster::cmds::TraceBeginCHROMIUM>();
  if (c) {
    c->Init(category_bucket_id, name_bucket_id);
  }
}

void TraceEndCHROMIUM() {
  raster::cmds::TraceEndCHROMIUM* c =
      GetCmdSpace<raster::cmds::TraceEndCHROMIUM>();
  if (c) {
    c->Init();
  }
}

void SetActiveURLCHROMIUM(GLuint url_bucket_id) {
  raster::cmds::SetActiveURLCHROMIUM* c =
      GetCmdSpace<raster::cmds::SetActiveURLCHROMIUM>();
  if (c) {
    c->Init(url_bucket_id);
  }
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_CMD_HELPER_AUTOGEN_H_
