// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by raster_implementation.cc to define the
// GL api functions.
#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_IMPL_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_IMPL_AUTOGEN_H_

void RasterImplementation::DeleteTextures(GLsizei n, const GLuint* textures) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteTextures(" << n << ", "
                     << static_cast<const void*>(textures) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << textures[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(textures[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteTextures", "n < 0");
    return;
  }
  DeleteTexturesHelper(n, textures);
  CheckGLError();
}

void RasterImplementation::GetIntegerv(GLenum pname, GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetIntegerv("
                     << GLES2Util::GetStringGLState(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "RasterImplementation::GetIntegerv");
  if (GetIntegervHelper(pname, params)) {
    return;
  }
  typedef cmds::GetIntegerv::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetIntegerv(pname, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  CheckGLError();
}
void RasterImplementation::GenQueriesEXT(GLsizei n, GLuint* queries) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGenQueriesEXT(" << n << ", "
                     << static_cast<const void*>(queries) << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glGenQueriesEXT", "n < 0");
    return;
  }
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kQueries);
  for (GLsizei ii = 0; ii < n; ++ii)
    queries[ii] = id_allocator->AllocateID();
  GenQueriesEXTHelper(n, queries);
  helper_->GenQueriesEXTImmediate(n, queries);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << queries[i]);
    }
  });
  CheckGLError();
}

void RasterImplementation::DeleteQueriesEXT(GLsizei n, const GLuint* queries) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDeleteQueriesEXT(" << n << ", "
                     << static_cast<const void*>(queries) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << queries[i]);
    }
  });
  GPU_CLIENT_DCHECK_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      DCHECK(queries[i] != 0);
    }
  });
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteQueriesEXT", "n < 0");
    return;
  }
  DeleteQueriesEXTHelper(n, queries);
  CheckGLError();
}

void RasterImplementation::LoseContextCHROMIUM(GLenum current, GLenum other) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glLoseContextCHROMIUM("
                     << GLES2Util::GetStringResetStatus(current) << ", "
                     << GLES2Util::GetStringResetStatus(other) << ")");
  helper_->LoseContextCHROMIUM(current, other);
  CheckGLError();
}

void RasterImplementation::UnpremultiplyAndDitherCopyCHROMIUM(GLuint source_id,
                                                              GLuint dest_id,
                                                              GLint x,
                                                              GLint y,
                                                              GLsizei width,
                                                              GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glUnpremultiplyAndDitherCopyCHROMIUM(" << source_id
                     << ", " << dest_id << ", " << x << ", " << y << ", "
                     << width << ", " << height << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glUnpremultiplyAndDitherCopyCHROMIUM",
               "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glUnpremultiplyAndDitherCopyCHROMIUM",
               "height < 0");
    return;
  }
  helper_->UnpremultiplyAndDitherCopyCHROMIUM(source_id, dest_id, x, y, width,
                                              height);
  CheckGLError();
}

GLuint RasterImplementation::CreateTexture(bool use_buffer,
                                           gfx::BufferUsage buffer_usage,
                                           viz::ResourceFormat format) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCreateTexture(" << use_buffer
                     << ", " << static_cast<uint32_t>(buffer_usage) << ", "
                     << static_cast<uint32_t>(format) << ")");
  GLuint client_id;
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kTextures);
  client_id = id_allocator->AllocateID();
  helper_->CreateTexture(use_buffer, buffer_usage, format, client_id);
  GPU_CLIENT_LOG("returned " << client_id);
  CheckGLError();
  return client_id;
}

void RasterImplementation::TexParameteri(GLuint texture_id,
                                         GLenum pname,
                                         GLint param) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTexParameteri(" << texture_id
                     << ", " << GLES2Util::GetStringTextureParameter(pname)
                     << ", " << param << ")");
  helper_->TexParameteri(texture_id, pname, param);
  CheckGLError();
}

void RasterImplementation::BindTexImage2DCHROMIUM(GLuint texture_id,
                                                  GLint image_id) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindTexImage2DCHROMIUM("
                     << texture_id << ", " << image_id << ")");
  helper_->BindTexImage2DCHROMIUM(texture_id, image_id);
  CheckGLError();
}

void RasterImplementation::ReleaseTexImage2DCHROMIUM(GLuint texture_id,
                                                     GLint image_id) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glReleaseTexImage2DCHROMIUM("
                     << texture_id << ", " << image_id << ")");
  helper_->ReleaseTexImage2DCHROMIUM(texture_id, image_id);
  CheckGLError();
}

void RasterImplementation::TexStorage2D(GLuint texture_id,
                                        GLsizei width,
                                        GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTexStorage2D(" << texture_id
                     << ", " << width << ", " << height << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2D", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexStorage2D", "height < 0");
    return;
  }
  helper_->TexStorage2D(texture_id, width, height);
  CheckGLError();
}

void RasterImplementation::CopySubTexture(GLuint source_id,
                                          GLuint dest_id,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCopySubTexture(" << source_id
                     << ", " << dest_id << ", " << xoffset << ", " << yoffset
                     << ", " << x << ", " << y << ", " << width << ", "
                     << height << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySubTexture", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySubTexture", "height < 0");
    return;
  }
  helper_->CopySubTexture(source_id, dest_id, xoffset, yoffset, x, y, width,
                          height);
  CheckGLError();
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_IMPLEMENTATION_IMPL_AUTOGEN_H_
