// Copyright 2018 The Chromium Authors. All rights reserved.
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

void BeginRasterCHROMIUMImmediate(GLuint sk_color,
                                  GLuint msaa_sample_count,
                                  GLboolean can_use_lcd_text,
                                  const GLbyte* mailbox) {
  const uint32_t size =
      raster::cmds::BeginRasterCHROMIUMImmediate::ComputeSize();
  raster::cmds::BeginRasterCHROMIUMImmediate* c =
      GetImmediateCmdSpaceTotalSize<raster::cmds::BeginRasterCHROMIUMImmediate>(
          size);
  if (c) {
    c->Init(sk_color, msaa_sample_count, can_use_lcd_text, mailbox);
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

void DeletePaintCacheTextBlobsINTERNALImmediate(GLsizei n, const GLuint* ids) {
  const uint32_t size =
      raster::cmds::DeletePaintCacheTextBlobsINTERNALImmediate::ComputeSize(n);
  raster::cmds::DeletePaintCacheTextBlobsINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          raster::cmds::DeletePaintCacheTextBlobsINTERNALImmediate>(size);
  if (c) {
    c->Init(n, ids);
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

void ClearPaintCacheINTERNAL() {
  raster::cmds::ClearPaintCacheINTERNAL* c =
      GetCmdSpace<raster::cmds::ClearPaintCacheINTERNAL>();
  if (c) {
    c->Init();
  }
}

void CopySubTextureINTERNALImmediate(GLint xoffset,
                                     GLint yoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height,
                                     const GLbyte* mailboxes) {
  const uint32_t size =
      raster::cmds::CopySubTextureINTERNALImmediate::ComputeSize();
  raster::cmds::CopySubTextureINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          raster::cmds::CopySubTextureINTERNALImmediate>(size);
  if (c) {
    c->Init(xoffset, yoffset, x, y, width, height, mailboxes);
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
