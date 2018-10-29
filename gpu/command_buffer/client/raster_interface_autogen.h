// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by raster_interface.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_AUTOGEN_H_

virtual void DeleteTextures(GLsizei n, const GLuint* textures) = 0;
virtual void Finish() = 0;
virtual void Flush() = 0;
virtual GLenum GetError() = 0;
virtual void GetIntegerv(GLenum pname, GLint* params) = 0;
virtual void ShallowFlushCHROMIUM() = 0;
virtual void OrderingBarrierCHROMIUM() = 0;
virtual void GenQueriesEXT(GLsizei n, GLuint* queries) = 0;
virtual void DeleteQueriesEXT(GLsizei n, const GLuint* queries) = 0;
virtual void BeginQueryEXT(GLenum target, GLuint id) = 0;
virtual void EndQueryEXT(GLenum target) = 0;
virtual void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) = 0;
virtual GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum internalformat) = 0;
virtual void DestroyImageCHROMIUM(GLuint image_id) = 0;
virtual void LoseContextCHROMIUM(GLenum current, GLenum other) = 0;
virtual void GenSyncTokenCHROMIUM(GLbyte* sync_token) = 0;
virtual void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) = 0;
virtual void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) = 0;
virtual void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) = 0;
virtual void UnpremultiplyAndDitherCopyCHROMIUM(GLuint source_id,
                                                GLuint dest_id,
                                                GLint x,
                                                GLint y,
                                                GLsizei width,
                                                GLsizei height) = 0;
virtual GLenum GetGraphicsResetStatusKHR() = 0;
virtual void EndRasterCHROMIUM() = 0;
virtual GLuint CreateTexture(bool use_buffer,
                             gfx::BufferUsage buffer_usage,
                             viz::ResourceFormat format) = 0;
virtual void SetColorSpaceMetadata(GLuint texture_id,
                                   GLColorSpace color_space) = 0;
virtual void ProduceTextureDirect(GLuint texture, GLbyte* mailbox) = 0;
virtual GLuint CreateAndConsumeTexture(bool use_buffer,
                                       gfx::BufferUsage buffer_usage,
                                       viz::ResourceFormat format,
                                       const GLbyte* mailbox) = 0;
virtual void TexParameteri(GLuint texture_id, GLenum pname, GLint param) = 0;
virtual void BindTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) = 0;
virtual void ReleaseTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) = 0;
virtual void TexStorage2D(GLuint texture_id, GLsizei width, GLsizei height) = 0;
virtual void CopySubTexture(GLuint source_id,
                            GLuint dest_id,
                            GLint xoffset,
                            GLint yoffset,
                            GLint x,
                            GLint y,
                            GLsizei width,
                            GLsizei height) = 0;
virtual void TraceBeginCHROMIUM(const char* category_name,
                                const char* trace_name) = 0;
virtual void TraceEndCHROMIUM() = 0;
virtual void SetActiveURLCHROMIUM(const char* url) = 0;
#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_AUTOGEN_H_
