// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by raster_implementation.h to declare the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_AUTOGEN_H_

void DeleteTextures(GLsizei n, const GLuint* textures) override;

void Finish() override;

void Flush() override;

GLenum GetError() override;

void GetIntegerv(GLenum pname, GLint* params) override;

void ShallowFlushCHROMIUM() override;

void OrderingBarrierCHROMIUM() override;

void GenQueriesEXT(GLsizei n, GLuint* queries) override;

void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;

void BeginQueryEXT(GLenum target, GLuint id) override;

void EndQueryEXT(GLenum target) override;

void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;

GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                           GLsizei width,
                           GLsizei height,
                           GLenum internalformat) override;

void DestroyImageCHROMIUM(GLuint image_id) override;

void LoseContextCHROMIUM(GLenum current, GLenum other) override;

void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;

void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;

void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;

void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;

void UnpremultiplyAndDitherCopyCHROMIUM(GLuint source_id,
                                        GLuint dest_id,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height) override;

GLenum GetGraphicsResetStatusKHR() override;

void EndRasterCHROMIUM() override;

GLuint CreateTexture(bool use_buffer,
                     gfx::BufferUsage buffer_usage,
                     viz::ResourceFormat format) override;

void SetColorSpaceMetadata(GLuint texture_id,
                           GLColorSpace color_space) override;

void ProduceTextureDirect(GLuint texture, GLbyte* mailbox) override;

GLuint CreateAndConsumeTexture(bool use_buffer,
                               gfx::BufferUsage buffer_usage,
                               viz::ResourceFormat format,
                               const GLbyte* mailbox) override;

void TexParameteri(GLuint texture_id, GLenum pname, GLint param) override;

void BindTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) override;

void ReleaseTexImage2DCHROMIUM(GLuint texture_id, GLint image_id) override;

void TexStorage2D(GLuint texture_id, GLsizei width, GLsizei height) override;

void CopySubTexture(GLuint source_id,
                    GLuint dest_id,
                    GLint xoffset,
                    GLint yoffset,
                    GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height) override;

void TraceBeginCHROMIUM(const char* category_name,
                        const char* trace_name) override;

void TraceEndCHROMIUM() override;

void SetActiveURLCHROMIUM(const char* url) override;

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_AUTOGEN_H_
