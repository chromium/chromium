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

void DeleteTexturesImmediate(GLsizei n, const GLuint* textures) {
  const uint32_t size = raster::cmds::DeleteTexturesImmediate::ComputeSize(n);
  raster::cmds::DeleteTexturesImmediate* c =
      GetImmediateCmdSpaceTotalSize<raster::cmds::DeleteTexturesImmediate>(
          size);
  if (c) {
    c->Init(n, textures);
  }
}

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

void GetIntegerv(GLenum pname,
                 uint32_t params_shm_id,
                 uint32_t params_shm_offset) {
  raster::cmds::GetIntegerv* c = GetCmdSpace<raster::cmds::GetIntegerv>();
  if (c) {
    c->Init(pname, params_shm_id, params_shm_offset);
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

void InsertFenceSyncCHROMIUM(GLuint64 release_count) {
  raster::cmds::InsertFenceSyncCHROMIUM* c =
      GetCmdSpace<raster::cmds::InsertFenceSyncCHROMIUM>();
  if (c) {
    c->Init(release_count);
  }
}

void WaitSyncTokenCHROMIUM(GLint namespace_id,
                           GLuint64 command_buffer_id,
                           GLuint64 release_count) {
  raster::cmds::WaitSyncTokenCHROMIUM* c =
      GetCmdSpace<raster::cmds::WaitSyncTokenCHROMIUM>();
  if (c) {
    c->Init(namespace_id, command_buffer_id, release_count);
  }
}

void UnpremultiplyAndDitherCopyCHROMIUM(GLuint source_id,
                                        GLuint dest_id,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height) {
  raster::cmds::UnpremultiplyAndDitherCopyCHROMIUM* c =
      GetCmdSpace<raster::cmds::UnpremultiplyAndDitherCopyCHROMIUM>();
  if (c) {
    c->Init(source_id, dest_id, x, y, width, height);
  }
}

void BeginRasterCHROMIUMImmediate(GLuint sk_color,
                                  GLuint msaa_sample_count,
                                  GLboolean can_use_lcd_text,
                                  GLint color_type,
                                  GLuint color_space_transfer_cache_id,
                                  const GLbyte* mailbox) {
  const uint32_t size =
      raster::cmds::BeginRasterCHROMIUMImmediate::ComputeSize();
  raster::cmds::BeginRasterCHROMIUMImmediate* c =
      GetImmediateCmdSpaceTotalSize<raster::cmds::BeginRasterCHROMIUMImmediate>(
          size);
  if (c) {
    c->Init(sk_color, msaa_sample_count, can_use_lcd_text, color_type,
            color_space_transfer_cache_id, mailbox);
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

void CreateTexture(bool use_buffer,
                   gfx::BufferUsage buffer_usage,
                   viz::ResourceFormat format,
                   uint32_t client_id) {
  raster::cmds::CreateTexture* c = GetCmdSpace<raster::cmds::CreateTexture>();
  if (c) {
    c->Init(use_buffer, buffer_usage, format, client_id);
  }
}

void SetColorSpaceMetadata(GLuint texture_id,
                           GLuint shm_id,
                           GLuint shm_offset,
                           GLsizei color_space_size) {
  raster::cmds::SetColorSpaceMetadata* c =
      GetCmdSpace<raster::cmds::SetColorSpaceMetadata>();
  if (c) {
    c->Init(texture_id, shm_id, shm_offset, color_space_size);
  }
}

void ProduceTextureDirectImmediate(GLuint texture, GLbyte* mailbox) {
  const uint32_t size =
      raster::cmds::ProduceTextureDirectImmediate::ComputeSize();
  raster::cmds::ProduceTextureDirectImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          raster::cmds::ProduceTextureDirectImmediate>(size);
  if (c) {
    c->Init(texture, mailbox);
  }
}

void CreateAndConsumeTextureINTERNALImmediate(GLuint texture_id,
                                              bool use_buffer,
                                              gfx::BufferUsage buffer_usage,
                                              viz::ResourceFormat format,
                                              const GLbyte* mailbox) {
  const uint32_t size =
      raster::cmds::CreateAndConsumeTextureINTERNALImmediate::ComputeSize();
  raster::cmds::CreateAndConsumeTextureINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          raster::cmds::CreateAndConsumeTextureINTERNALImmediate>(size);
  if (c) {
    c->Init(texture_id, use_buffer, buffer_usage, format, mailbox);
  }
}

void TexParameteri(GLuint texture_id, GLenum pname, GLint param) {
  raster::cmds::TexParameteri* c = GetCmdSpace<raster::cmds::TexParameteri>();
  if (c) {
    c->Init(texture_id, pname, param);
  }
}

void BindTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) {
  raster::cmds::BindTexImage2DCHROMIUM* c =
      GetCmdSpace<raster::cmds::BindTexImage2DCHROMIUM>();
  if (c) {
    c->Init(texture_id, image_id);
  }
}

void ReleaseTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) {
  raster::cmds::ReleaseTexImage2DCHROMIUM* c =
      GetCmdSpace<raster::cmds::ReleaseTexImage2DCHROMIUM>();
  if (c) {
    c->Init(texture_id, image_id);
  }
}

void TexStorage2D(GLuint texture_id, GLsizei width, GLsizei height) {
  raster::cmds::TexStorage2D* c = GetCmdSpace<raster::cmds::TexStorage2D>();
  if (c) {
    c->Init(texture_id, width, height);
  }
}

void CopySubTexture(GLuint source_id,
                    GLuint dest_id,
                    GLint xoffset,
                    GLint yoffset,
                    GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height) {
  raster::cmds::CopySubTexture* c = GetCmdSpace<raster::cmds::CopySubTexture>();
  if (c) {
    c->Init(source_id, dest_id, xoffset, yoffset, x, y, width, height);
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
