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

error::Error RasterDecoderImpl::HandleDeleteTexturesImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::DeleteTexturesImmediate& c =
      *static_cast<const volatile raster::cmds::DeleteTexturesImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!gles2::SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* textures =
      gles2::GetImmediateDataAs<volatile const GLuint*>(c, data_size,
                                                        immediate_data_size);
  if (textures == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteTexturesHelper(n, textures);
  return error::kNoError;
}

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

error::Error RasterDecoderImpl::HandleGetIntegerv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::GetIntegerv& c =
      *static_cast<const volatile raster::cmds::GetIntegerv*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetIntegerv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetIntegerv", pname, "pname");
    return error::kNoError;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, Result::ComputeSize(num_values));
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->g_l_state.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetIntegerv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetIntegerv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetIntegerv(pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetIntegerv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleGenQueriesEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::GenQueriesEXTImmediate& c =
      *static_cast<const volatile raster::cmds::GenQueriesEXTImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t data_size;
  if (!gles2::SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* queries = gles2::GetImmediateDataAs<volatile GLuint*>(
      c, data_size, immediate_data_size);
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
  uint32_t data_size;
  if (!gles2::SafeMultiplyUint32(n, sizeof(GLuint), &data_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* queries =
      gles2::GetImmediateDataAs<volatile const GLuint*>(c, data_size,
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

error::Error RasterDecoderImpl::HandleUnpremultiplyAndDitherCopyCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::UnpremultiplyAndDitherCopyCHROMIUM& c =
      *static_cast<
          const volatile raster::cmds::UnpremultiplyAndDitherCopyCHROMIUM*>(
          cmd_data);
  GLuint source_id = static_cast<GLuint>(c.source_id);
  GLuint dest_id = static_cast<GLuint>(c.dest_id);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUnpremultiplyAndDitherCopyCHROMIUM",
                       "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUnpremultiplyAndDitherCopyCHROMIUM",
                       "height < 0");
    return error::kNoError;
  }
  DoUnpremultiplyAndDitherCopyCHROMIUM(source_id, dest_id, x, y, width, height);
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
  GLint color_type = static_cast<GLint>(c.color_type);
  GLuint color_space_transfer_cache_id =
      static_cast<GLuint>(c.color_space_transfer_cache_id);
  uint32_t data_size;
  if (!gles2::GLES2Util::ComputeDataSize<GLbyte, 16>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox =
      gles2::GetImmediateDataAs<volatile const GLbyte*>(c, data_size,
                                                        immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  DoBeginRasterCHROMIUM(sk_color, msaa_sample_count, can_use_lcd_text,
                        color_type, color_space_transfer_cache_id, mailbox);
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
  GLsizeiptr raster_shm_size = static_cast<GLsizeiptr>(c.raster_shm_size);
  GLuint font_shm_id = static_cast<GLuint>(c.font_shm_id);
  GLuint font_shm_offset = static_cast<GLuint>(c.font_shm_offset);
  GLsizeiptr font_shm_size = static_cast<GLsizeiptr>(c.font_shm_size);
  if (raster_shm_size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                       "raster_shm_size < 0");
    return error::kNoError;
  }
  if (font_shm_size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRasterCHROMIUM",
                       "font_shm_size < 0");
    return error::kNoError;
  }
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

error::Error RasterDecoderImpl::HandleCreateTexture(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::CreateTexture& c =
      *static_cast<const volatile raster::cmds::CreateTexture*>(cmd_data);
  bool use_buffer = static_cast<bool>(c.use_buffer);
  gfx::BufferUsage buffer_usage = static_cast<gfx::BufferUsage>(c.buffer_usage);
  viz::ResourceFormat format = static_cast<viz::ResourceFormat>(c.format);
  if (!validators_->gfx_buffer_usage.IsValid(buffer_usage)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCreateTexture", buffer_usage,
                                    "buffer_usage");
    return error::kNoError;
  }
  if (!validators_->viz_resource_format.IsValid(format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCreateTexture", format, "format");
    return error::kNoError;
  }
  uint32_t client_id = c.client_id;
  if (GetTexture(client_id)) {
    return error::kInvalidArguments;
  }
  GLuint service_id = DoCreateTexture(use_buffer, buffer_usage, format);
  if (service_id) {
    CreateTexture(client_id, service_id, use_buffer, buffer_usage, format);
  }
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleProduceTextureDirectImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::ProduceTextureDirectImmediate& c =
      *static_cast<const volatile raster::cmds::ProduceTextureDirectImmediate*>(
          cmd_data);
  GLuint texture = static_cast<GLuint>(c.texture);
  uint32_t data_size;
  if (!gles2::GLES2Util::ComputeDataSize<GLbyte, 16>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile GLbyte* mailbox = gles2::GetImmediateDataAs<volatile GLbyte*>(
      c, data_size, immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  DoProduceTextureDirect(texture, mailbox);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleCreateAndConsumeTextureINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::CreateAndConsumeTextureINTERNALImmediate& c =
      *static_cast<const volatile raster::cmds::
                       CreateAndConsumeTextureINTERNALImmediate*>(cmd_data);
  GLuint texture_id = static_cast<GLuint>(c.texture_id);
  bool use_buffer = static_cast<bool>(c.use_buffer);
  gfx::BufferUsage buffer_usage = static_cast<gfx::BufferUsage>(c.buffer_usage);
  viz::ResourceFormat format = static_cast<viz::ResourceFormat>(c.format);
  uint32_t data_size;
  if (!gles2::GLES2Util::ComputeDataSize<GLbyte, 16>(1, &data_size)) {
    return error::kOutOfBounds;
  }
  if (data_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox =
      gles2::GetImmediateDataAs<volatile const GLbyte*>(c, data_size,
                                                        immediate_data_size);
  if (!validators_->gfx_buffer_usage.IsValid(buffer_usage)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCreateAndConsumeTextureINTERNAL",
                                    buffer_usage, "buffer_usage");
    return error::kNoError;
  }
  if (!validators_->viz_resource_format.IsValid(format)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCreateAndConsumeTextureINTERNAL", format,
                                    "format");
    return error::kNoError;
  }
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  DoCreateAndConsumeTextureINTERNAL(texture_id, use_buffer, buffer_usage,
                                    format, mailbox);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleTexParameteri(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::TexParameteri& c =
      *static_cast<const volatile raster::cmds::TexParameteri*>(cmd_data);
  GLuint texture_id = static_cast<GLuint>(c.texture_id);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  if (!validators_->texture_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameteri", pname, "pname");
    return error::kNoError;
  }
  DoTexParameteri(texture_id, pname, param);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleBindTexImage2DCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::BindTexImage2DCHROMIUM& c =
      *static_cast<const volatile raster::cmds::BindTexImage2DCHROMIUM*>(
          cmd_data);
  GLuint texture_id = static_cast<GLuint>(c.texture_id);
  GLint image_id = static_cast<GLint>(c.image_id);
  DoBindTexImage2DCHROMIUM(texture_id, image_id);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleReleaseTexImage2DCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::ReleaseTexImage2DCHROMIUM& c =
      *static_cast<const volatile raster::cmds::ReleaseTexImage2DCHROMIUM*>(
          cmd_data);
  GLuint texture_id = static_cast<GLuint>(c.texture_id);
  GLint image_id = static_cast<GLint>(c.image_id);
  DoReleaseTexImage2DCHROMIUM(texture_id, image_id);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleTexStorage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::TexStorage2D& c =
      *static_cast<const volatile raster::cmds::TexStorage2D*>(cmd_data);
  GLuint texture_id = static_cast<GLuint>(c.texture_id);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2D", "height < 0");
    return error::kNoError;
  }
  DoTexStorage2D(texture_id, width, height);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleCopySubTexture(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile raster::cmds::CopySubTexture& c =
      *static_cast<const volatile raster::cmds::CopySubTexture*>(cmd_data);
  GLuint source_id = static_cast<GLuint>(c.source_id);
  GLuint dest_id = static_cast<GLuint>(c.dest_id);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTexture", "height < 0");
    return error::kNoError;
  }
  DoCopySubTexture(source_id, dest_id, xoffset, yoffset, x, y, width, height);
  return error::kNoError;
}

error::Error RasterDecoderImpl::HandleTraceEndCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  DoTraceEndCHROMIUM();
  return error::kNoError;
}

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_AUTOGEN_H_
