// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by gles2_trace_implementation.cc
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_TRACE_IMPLEMENTATION_IMPL_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_TRACE_IMPLEMENTATION_IMPL_AUTOGEN_H_

void GLES2TraceImplementation::ActiveTexture(GLenum texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ActiveTexture");
  gl_->ActiveTexture(texture);
}

void GLES2TraceImplementation::AttachShader(GLuint program, GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::AttachShader");
  gl_->AttachShader(program, shader);
}

void GLES2TraceImplementation::BindAttribLocation(GLuint program,
                                                  GLuint index,
                                                  const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindAttribLocation");
  gl_->BindAttribLocation(program, index, name);
}

void GLES2TraceImplementation::BindBuffer(GLenum target, GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindBuffer");
  gl_->BindBuffer(target, buffer);
}

void GLES2TraceImplementation::BindBufferBase(GLenum target,
                                              GLuint index,
                                              GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindBufferBase");
  gl_->BindBufferBase(target, index, buffer);
}

void GLES2TraceImplementation::BindBufferRange(GLenum target,
                                               GLuint index,
                                               GLuint buffer,
                                               GLintptr offset,
                                               GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindBufferRange");
  gl_->BindBufferRange(target, index, buffer, offset, size);
}

void GLES2TraceImplementation::BindFramebuffer(GLenum target,
                                               GLuint framebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindFramebuffer");
  gl_->BindFramebuffer(target, framebuffer);
}

void GLES2TraceImplementation::BindRenderbuffer(GLenum target,
                                                GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindRenderbuffer");
  gl_->BindRenderbuffer(target, renderbuffer);
}

void GLES2TraceImplementation::BindSampler(GLuint unit, GLuint sampler) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindSampler");
  gl_->BindSampler(unit, sampler);
}

void GLES2TraceImplementation::BindTexture(GLenum target, GLuint texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindTexture");
  gl_->BindTexture(target, texture);
}

void GLES2TraceImplementation::BindTransformFeedback(GLenum target,
                                                     GLuint transformfeedback) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindTransformFeedback");
  gl_->BindTransformFeedback(target, transformfeedback);
}

void GLES2TraceImplementation::BlendColor(GLclampf red,
                                          GLclampf green,
                                          GLclampf blue,
                                          GLclampf alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendColor");
  gl_->BlendColor(red, green, blue, alpha);
}

void GLES2TraceImplementation::BlendEquation(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendEquation");
  gl_->BlendEquation(mode);
}

void GLES2TraceImplementation::BlendEquationSeparate(GLenum modeRGB,
                                                     GLenum modeAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendEquationSeparate");
  gl_->BlendEquationSeparate(modeRGB, modeAlpha);
}

void GLES2TraceImplementation::BlendFunc(GLenum sfactor, GLenum dfactor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendFunc");
  gl_->BlendFunc(sfactor, dfactor);
}

void GLES2TraceImplementation::BlendFuncSeparate(GLenum srcRGB,
                                                 GLenum dstRGB,
                                                 GLenum srcAlpha,
                                                 GLenum dstAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendFuncSeparate");
  gl_->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GLES2TraceImplementation::BufferData(GLenum target,
                                          GLsizeiptr size,
                                          const void* data,
                                          GLenum usage) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BufferData");
  gl_->BufferData(target, size, data, usage);
}

void GLES2TraceImplementation::BufferSubData(GLenum target,
                                             GLintptr offset,
                                             GLsizeiptr size,
                                             const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BufferSubData");
  gl_->BufferSubData(target, offset, size, data);
}

GLenum GLES2TraceImplementation::CheckFramebufferStatus(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CheckFramebufferStatus");
  return gl_->CheckFramebufferStatus(target);
}

void GLES2TraceImplementation::Clear(GLbitfield mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Clear");
  gl_->Clear(mask);
}

void GLES2TraceImplementation::ClearBufferfi(GLenum buffer,
                                             GLint drawbuffers,
                                             GLfloat depth,
                                             GLint stencil) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ClearBufferfi");
  gl_->ClearBufferfi(buffer, drawbuffers, depth, stencil);
}

void GLES2TraceImplementation::ClearBufferfv(GLenum buffer,
                                             GLint drawbuffers,
                                             const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ClearBufferfv");
  gl_->ClearBufferfv(buffer, drawbuffers, value);
}

void GLES2TraceImplementation::ClearBufferiv(GLenum buffer,
                                             GLint drawbuffers,
                                             const GLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ClearBufferiv");
  gl_->ClearBufferiv(buffer, drawbuffers, value);
}

void GLES2TraceImplementation::ClearBufferuiv(GLenum buffer,
                                              GLint drawbuffers,
                                              const GLuint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ClearBufferuiv");
  gl_->ClearBufferuiv(buffer, drawbuffers, value);
}

void GLES2TraceImplementation::ClearColor(GLclampf red,
                                          GLclampf green,
                                          GLclampf blue,
                                          GLclampf alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ClearColor");
  gl_->ClearColor(red, green, blue, alpha);
}

void GLES2TraceImplementation::ClearDepthf(GLclampf depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ClearDepthf");
  gl_->ClearDepthf(depth);
}

void GLES2TraceImplementation::ClearStencil(GLint s) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ClearStencil");
  gl_->ClearStencil(s);
}

GLenum GLES2TraceImplementation::ClientWaitSync(GLsync sync,
                                                GLbitfield flags,
                                                GLuint64 timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ClientWaitSync");
  return gl_->ClientWaitSync(sync, flags, timeout);
}

void GLES2TraceImplementation::ColorMask(GLboolean red,
                                         GLboolean green,
                                         GLboolean blue,
                                         GLboolean alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ColorMask");
  gl_->ColorMask(red, green, blue, alpha);
}

void GLES2TraceImplementation::CompileShader(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CompileShader");
  gl_->CompileShader(shader);
}

void GLES2TraceImplementation::CompressedTexImage2D(GLenum target,
                                                    GLint level,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLint border,
                                                    GLsizei imageSize,
                                                    const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CompressedTexImage2D");
  gl_->CompressedTexImage2D(target, level, internalformat, width, height,
                            border, imageSize, data);
}

void GLES2TraceImplementation::CompressedTexSubImage2D(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum format,
                                                       GLsizei imageSize,
                                                       const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CompressedTexSubImage2D");
  gl_->CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height,
                               format, imageSize, data);
}

void GLES2TraceImplementation::CompressedTexImage3D(GLenum target,
                                                    GLint level,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLsizei depth,
                                                    GLint border,
                                                    GLsizei imageSize,
                                                    const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CompressedTexImage3D");
  gl_->CompressedTexImage3D(target, level, internalformat, width, height, depth,
                            border, imageSize, data);
}

void GLES2TraceImplementation::CompressedTexSubImage3D(GLenum target,
                                                       GLint level,
                                                       GLint xoffset,
                                                       GLint yoffset,
                                                       GLint zoffset,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLsizei depth,
                                                       GLenum format,
                                                       GLsizei imageSize,
                                                       const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CompressedTexSubImage3D");
  gl_->CompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                               height, depth, format, imageSize, data);
}

void GLES2TraceImplementation::CopyBufferSubData(GLenum readtarget,
                                                 GLenum writetarget,
                                                 GLintptr readoffset,
                                                 GLintptr writeoffset,
                                                 GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CopyBufferSubData");
  gl_->CopyBufferSubData(readtarget, writetarget, readoffset, writeoffset,
                         size);
}

void GLES2TraceImplementation::CopyTexImage2D(GLenum target,
                                              GLint level,
                                              GLenum internalformat,
                                              GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height,
                                              GLint border) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CopyTexImage2D");
  gl_->CopyTexImage2D(target, level, internalformat, x, y, width, height,
                      border);
}

void GLES2TraceImplementation::CopyTexSubImage2D(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLint x,
                                                 GLint y,
                                                 GLsizei width,
                                                 GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CopyTexSubImage2D");
  gl_->CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void GLES2TraceImplementation::CopyTexSubImage3D(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLint zoffset,
                                                 GLint x,
                                                 GLint y,
                                                 GLsizei width,
                                                 GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CopyTexSubImage3D");
  gl_->CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width,
                         height);
}

GLuint GLES2TraceImplementation::CreateProgram() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CreateProgram");
  return gl_->CreateProgram();
}

GLuint GLES2TraceImplementation::CreateShader(GLenum type) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CreateShader");
  return gl_->CreateShader(type);
}

void GLES2TraceImplementation::CullFace(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CullFace");
  gl_->CullFace(mode);
}

void GLES2TraceImplementation::DeleteBuffers(GLsizei n, const GLuint* buffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteBuffers");
  gl_->DeleteBuffers(n, buffers);
}

void GLES2TraceImplementation::DeleteFramebuffers(GLsizei n,
                                                  const GLuint* framebuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteFramebuffers");
  gl_->DeleteFramebuffers(n, framebuffers);
}

void GLES2TraceImplementation::DeleteProgram(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteProgram");
  gl_->DeleteProgram(program);
}

void GLES2TraceImplementation::DeleteRenderbuffers(
    GLsizei n,
    const GLuint* renderbuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteRenderbuffers");
  gl_->DeleteRenderbuffers(n, renderbuffers);
}

void GLES2TraceImplementation::DeleteSamplers(GLsizei n,
                                              const GLuint* samplers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteSamplers");
  gl_->DeleteSamplers(n, samplers);
}

void GLES2TraceImplementation::DeleteSync(GLsync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteSync");
  gl_->DeleteSync(sync);
}

void GLES2TraceImplementation::DeleteShader(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteShader");
  gl_->DeleteShader(shader);
}

void GLES2TraceImplementation::DeleteTextures(GLsizei n,
                                              const GLuint* textures) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteTextures");
  gl_->DeleteTextures(n, textures);
}

void GLES2TraceImplementation::DeleteTransformFeedbacks(GLsizei n,
                                                        const GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteTransformFeedbacks");
  gl_->DeleteTransformFeedbacks(n, ids);
}

void GLES2TraceImplementation::DepthFunc(GLenum func) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DepthFunc");
  gl_->DepthFunc(func);
}

void GLES2TraceImplementation::DepthMask(GLboolean flag) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DepthMask");
  gl_->DepthMask(flag);
}

void GLES2TraceImplementation::DepthRangef(GLclampf zNear, GLclampf zFar) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DepthRangef");
  gl_->DepthRangef(zNear, zFar);
}

void GLES2TraceImplementation::DetachShader(GLuint program, GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DetachShader");
  gl_->DetachShader(program, shader);
}

void GLES2TraceImplementation::Disable(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Disable");
  gl_->Disable(cap);
}

void GLES2TraceImplementation::DisableVertexAttribArray(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DisableVertexAttribArray");
  gl_->DisableVertexAttribArray(index);
}

void GLES2TraceImplementation::DrawArrays(GLenum mode,
                                          GLint first,
                                          GLsizei count) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DrawArrays");
  gl_->DrawArrays(mode, first, count);
}

void GLES2TraceImplementation::DrawElements(GLenum mode,
                                            GLsizei count,
                                            GLenum type,
                                            const void* indices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DrawElements");
  gl_->DrawElements(mode, count, type, indices);
}

void GLES2TraceImplementation::DrawRangeElements(GLenum mode,
                                                 GLuint start,
                                                 GLuint end,
                                                 GLsizei count,
                                                 GLenum type,
                                                 const void* indices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DrawRangeElements");
  gl_->DrawRangeElements(mode, start, end, count, type, indices);
}

void GLES2TraceImplementation::Enable(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Enable");
  gl_->Enable(cap);
}

void GLES2TraceImplementation::EnableVertexAttribArray(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::EnableVertexAttribArray");
  gl_->EnableVertexAttribArray(index);
}

GLsync GLES2TraceImplementation::FenceSync(GLenum condition, GLbitfield flags) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::FenceSync");
  return gl_->FenceSync(condition, flags);
}

void GLES2TraceImplementation::Finish() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Finish");
  gl_->Finish();
}

void GLES2TraceImplementation::Flush() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Flush");
  gl_->Flush();
}

void GLES2TraceImplementation::FramebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::FramebufferRenderbuffer");
  gl_->FramebufferRenderbuffer(target, attachment, renderbuffertarget,
                               renderbuffer);
}

void GLES2TraceImplementation::FramebufferTexture2D(GLenum target,
                                                    GLenum attachment,
                                                    GLenum textarget,
                                                    GLuint texture,
                                                    GLint level) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::FramebufferTexture2D");
  gl_->FramebufferTexture2D(target, attachment, textarget, texture, level);
}

void GLES2TraceImplementation::FramebufferTextureLayer(GLenum target,
                                                       GLenum attachment,
                                                       GLuint texture,
                                                       GLint level,
                                                       GLint layer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::FramebufferTextureLayer");
  gl_->FramebufferTextureLayer(target, attachment, texture, level, layer);
}

void GLES2TraceImplementation::FrontFace(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::FrontFace");
  gl_->FrontFace(mode);
}

void GLES2TraceImplementation::GenBuffers(GLsizei n, GLuint* buffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GenBuffers");
  gl_->GenBuffers(n, buffers);
}

void GLES2TraceImplementation::GenerateMipmap(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GenerateMipmap");
  gl_->GenerateMipmap(target);
}

void GLES2TraceImplementation::GenFramebuffers(GLsizei n,
                                               GLuint* framebuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GenFramebuffers");
  gl_->GenFramebuffers(n, framebuffers);
}

void GLES2TraceImplementation::GenRenderbuffers(GLsizei n,
                                                GLuint* renderbuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GenRenderbuffers");
  gl_->GenRenderbuffers(n, renderbuffers);
}

void GLES2TraceImplementation::GenSamplers(GLsizei n, GLuint* samplers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GenSamplers");
  gl_->GenSamplers(n, samplers);
}

void GLES2TraceImplementation::GenTextures(GLsizei n, GLuint* textures) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GenTextures");
  gl_->GenTextures(n, textures);
}

void GLES2TraceImplementation::GenTransformFeedbacks(GLsizei n, GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GenTransformFeedbacks");
  gl_->GenTransformFeedbacks(n, ids);
}

void GLES2TraceImplementation::GetActiveAttrib(GLuint program,
                                               GLuint index,
                                               GLsizei bufsize,
                                               GLsizei* length,
                                               GLint* size,
                                               GLenum* type,
                                               char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetActiveAttrib");
  gl_->GetActiveAttrib(program, index, bufsize, length, size, type, name);
}

void GLES2TraceImplementation::GetActiveUniform(GLuint program,
                                                GLuint index,
                                                GLsizei bufsize,
                                                GLsizei* length,
                                                GLint* size,
                                                GLenum* type,
                                                char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetActiveUniform");
  gl_->GetActiveUniform(program, index, bufsize, length, size, type, name);
}

void GLES2TraceImplementation::GetActiveUniformBlockiv(GLuint program,
                                                       GLuint index,
                                                       GLenum pname,
                                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetActiveUniformBlockiv");
  gl_->GetActiveUniformBlockiv(program, index, pname, params);
}

void GLES2TraceImplementation::GetActiveUniformBlockName(GLuint program,
                                                         GLuint index,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetActiveUniformBlockName");
  gl_->GetActiveUniformBlockName(program, index, bufsize, length, name);
}

void GLES2TraceImplementation::GetActiveUniformsiv(GLuint program,
                                                   GLsizei count,
                                                   const GLuint* indices,
                                                   GLenum pname,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetActiveUniformsiv");
  gl_->GetActiveUniformsiv(program, count, indices, pname, params);
}

void GLES2TraceImplementation::GetAttachedShaders(GLuint program,
                                                  GLsizei maxcount,
                                                  GLsizei* count,
                                                  GLuint* shaders) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetAttachedShaders");
  gl_->GetAttachedShaders(program, maxcount, count, shaders);
}

GLint GLES2TraceImplementation::GetAttribLocation(GLuint program,
                                                  const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetAttribLocation");
  return gl_->GetAttribLocation(program, name);
}

void GLES2TraceImplementation::GetBooleanv(GLenum pname, GLboolean* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetBooleanv");
  gl_->GetBooleanv(pname, params);
}

void GLES2TraceImplementation::GetBooleani_v(GLenum pname,
                                             GLuint index,
                                             GLboolean* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetBooleani_v");
  gl_->GetBooleani_v(pname, index, data);
}

void GLES2TraceImplementation::GetBufferParameteri64v(GLenum target,
                                                      GLenum pname,
                                                      GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetBufferParameteri64v");
  gl_->GetBufferParameteri64v(target, pname, params);
}

void GLES2TraceImplementation::GetBufferParameteriv(GLenum target,
                                                    GLenum pname,
                                                    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetBufferParameteriv");
  gl_->GetBufferParameteriv(target, pname, params);
}

GLenum GLES2TraceImplementation::GetError() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetError");
  return gl_->GetError();
}

void GLES2TraceImplementation::GetFloatv(GLenum pname, GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetFloatv");
  gl_->GetFloatv(pname, params);
}

GLint GLES2TraceImplementation::GetFragDataLocation(GLuint program,
                                                    const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetFragDataLocation");
  return gl_->GetFragDataLocation(program, name);
}

void GLES2TraceImplementation::GetFramebufferAttachmentParameteriv(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::GetFramebufferAttachmentParameteriv");
  gl_->GetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

void GLES2TraceImplementation::GetInteger64v(GLenum pname, GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetInteger64v");
  gl_->GetInteger64v(pname, params);
}

void GLES2TraceImplementation::GetIntegeri_v(GLenum pname,
                                             GLuint index,
                                             GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetIntegeri_v");
  gl_->GetIntegeri_v(pname, index, data);
}

void GLES2TraceImplementation::GetInteger64i_v(GLenum pname,
                                               GLuint index,
                                               GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetInteger64i_v");
  gl_->GetInteger64i_v(pname, index, data);
}

void GLES2TraceImplementation::GetIntegerv(GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetIntegerv");
  gl_->GetIntegerv(pname, params);
}

void GLES2TraceImplementation::GetInternalformativ(GLenum target,
                                                   GLenum format,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetInternalformativ");
  gl_->GetInternalformativ(target, format, pname, bufSize, params);
}

void GLES2TraceImplementation::GetProgramiv(GLuint program,
                                            GLenum pname,
                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetProgramiv");
  gl_->GetProgramiv(program, pname, params);
}

void GLES2TraceImplementation::GetProgramInfoLog(GLuint program,
                                                 GLsizei bufsize,
                                                 GLsizei* length,
                                                 char* infolog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetProgramInfoLog");
  gl_->GetProgramInfoLog(program, bufsize, length, infolog);
}

void GLES2TraceImplementation::GetRenderbufferParameteriv(GLenum target,
                                                          GLenum pname,
                                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::GetRenderbufferParameteriv");
  gl_->GetRenderbufferParameteriv(target, pname, params);
}

void GLES2TraceImplementation::GetSamplerParameterfv(GLuint sampler,
                                                     GLenum pname,
                                                     GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetSamplerParameterfv");
  gl_->GetSamplerParameterfv(sampler, pname, params);
}

void GLES2TraceImplementation::GetSamplerParameteriv(GLuint sampler,
                                                     GLenum pname,
                                                     GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetSamplerParameteriv");
  gl_->GetSamplerParameteriv(sampler, pname, params);
}

void GLES2TraceImplementation::GetShaderiv(GLuint shader,
                                           GLenum pname,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetShaderiv");
  gl_->GetShaderiv(shader, pname, params);
}

void GLES2TraceImplementation::GetShaderInfoLog(GLuint shader,
                                                GLsizei bufsize,
                                                GLsizei* length,
                                                char* infolog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetShaderInfoLog");
  gl_->GetShaderInfoLog(shader, bufsize, length, infolog);
}

void GLES2TraceImplementation::GetShaderPrecisionFormat(GLenum shadertype,
                                                        GLenum precisiontype,
                                                        GLint* range,
                                                        GLint* precision) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetShaderPrecisionFormat");
  gl_->GetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}

void GLES2TraceImplementation::GetShaderSource(GLuint shader,
                                               GLsizei bufsize,
                                               GLsizei* length,
                                               char* source) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetShaderSource");
  gl_->GetShaderSource(shader, bufsize, length, source);
}

const GLubyte* GLES2TraceImplementation::GetString(GLenum name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetString");
  return gl_->GetString(name);
}

const GLubyte* GLES2TraceImplementation::GetStringi(GLenum name, GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetStringi");
  return gl_->GetStringi(name, index);
}

void GLES2TraceImplementation::GetSynciv(GLsync sync,
                                         GLenum pname,
                                         GLsizei bufsize,
                                         GLsizei* length,
                                         GLint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetSynciv");
  gl_->GetSynciv(sync, pname, bufsize, length, values);
}

void GLES2TraceImplementation::GetTexParameterfv(GLenum target,
                                                 GLenum pname,
                                                 GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetTexParameterfv");
  gl_->GetTexParameterfv(target, pname, params);
}

void GLES2TraceImplementation::GetTexParameteriv(GLenum target,
                                                 GLenum pname,
                                                 GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetTexParameteriv");
  gl_->GetTexParameteriv(target, pname, params);
}

void GLES2TraceImplementation::GetTransformFeedbackVarying(GLuint program,
                                                           GLuint index,
                                                           GLsizei bufsize,
                                                           GLsizei* length,
                                                           GLsizei* size,
                                                           GLenum* type,
                                                           char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::GetTransformFeedbackVarying");
  gl_->GetTransformFeedbackVarying(program, index, bufsize, length, size, type,
                                   name);
}

GLuint GLES2TraceImplementation::GetUniformBlockIndex(GLuint program,
                                                      const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetUniformBlockIndex");
  return gl_->GetUniformBlockIndex(program, name);
}

void GLES2TraceImplementation::GetUniformfv(GLuint program,
                                            GLint location,
                                            GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetUniformfv");
  gl_->GetUniformfv(program, location, params);
}

void GLES2TraceImplementation::GetUniformiv(GLuint program,
                                            GLint location,
                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetUniformiv");
  gl_->GetUniformiv(program, location, params);
}

void GLES2TraceImplementation::GetUniformuiv(GLuint program,
                                             GLint location,
                                             GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetUniformuiv");
  gl_->GetUniformuiv(program, location, params);
}

void GLES2TraceImplementation::GetUniformIndices(GLuint program,
                                                 GLsizei count,
                                                 const char* const* names,
                                                 GLuint* indices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetUniformIndices");
  gl_->GetUniformIndices(program, count, names, indices);
}

GLint GLES2TraceImplementation::GetUniformLocation(GLuint program,
                                                   const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetUniformLocation");
  return gl_->GetUniformLocation(program, name);
}

void GLES2TraceImplementation::GetVertexAttribfv(GLuint index,
                                                 GLenum pname,
                                                 GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetVertexAttribfv");
  gl_->GetVertexAttribfv(index, pname, params);
}

void GLES2TraceImplementation::GetVertexAttribiv(GLuint index,
                                                 GLenum pname,
                                                 GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetVertexAttribiv");
  gl_->GetVertexAttribiv(index, pname, params);
}

void GLES2TraceImplementation::GetVertexAttribIiv(GLuint index,
                                                  GLenum pname,
                                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetVertexAttribIiv");
  gl_->GetVertexAttribIiv(index, pname, params);
}

void GLES2TraceImplementation::GetVertexAttribIuiv(GLuint index,
                                                   GLenum pname,
                                                   GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetVertexAttribIuiv");
  gl_->GetVertexAttribIuiv(index, pname, params);
}

void GLES2TraceImplementation::GetVertexAttribPointerv(GLuint index,
                                                       GLenum pname,
                                                       void** pointer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetVertexAttribPointerv");
  gl_->GetVertexAttribPointerv(index, pname, pointer);
}

void GLES2TraceImplementation::Hint(GLenum target, GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Hint");
  gl_->Hint(target, mode);
}

void GLES2TraceImplementation::InvalidateFramebuffer(
    GLenum target,
    GLsizei count,
    const GLenum* attachments) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::InvalidateFramebuffer");
  gl_->InvalidateFramebuffer(target, count, attachments);
}

void GLES2TraceImplementation::InvalidateSubFramebuffer(
    GLenum target,
    GLsizei count,
    const GLenum* attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::InvalidateSubFramebuffer");
  gl_->InvalidateSubFramebuffer(target, count, attachments, x, y, width,
                                height);
}

GLboolean GLES2TraceImplementation::IsBuffer(GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsBuffer");
  return gl_->IsBuffer(buffer);
}

GLboolean GLES2TraceImplementation::IsEnabled(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsEnabled");
  return gl_->IsEnabled(cap);
}

GLboolean GLES2TraceImplementation::IsFramebuffer(GLuint framebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsFramebuffer");
  return gl_->IsFramebuffer(framebuffer);
}

GLboolean GLES2TraceImplementation::IsProgram(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsProgram");
  return gl_->IsProgram(program);
}

GLboolean GLES2TraceImplementation::IsRenderbuffer(GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsRenderbuffer");
  return gl_->IsRenderbuffer(renderbuffer);
}

GLboolean GLES2TraceImplementation::IsSampler(GLuint sampler) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsSampler");
  return gl_->IsSampler(sampler);
}

GLboolean GLES2TraceImplementation::IsShader(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsShader");
  return gl_->IsShader(shader);
}

GLboolean GLES2TraceImplementation::IsSync(GLsync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsSync");
  return gl_->IsSync(sync);
}

GLboolean GLES2TraceImplementation::IsTexture(GLuint texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsTexture");
  return gl_->IsTexture(texture);
}

GLboolean GLES2TraceImplementation::IsTransformFeedback(
    GLuint transformfeedback) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsTransformFeedback");
  return gl_->IsTransformFeedback(transformfeedback);
}

void GLES2TraceImplementation::LineWidth(GLfloat width) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::LineWidth");
  gl_->LineWidth(width);
}

void GLES2TraceImplementation::LinkProgram(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::LinkProgram");
  gl_->LinkProgram(program);
}

void GLES2TraceImplementation::PauseTransformFeedback() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::PauseTransformFeedback");
  gl_->PauseTransformFeedback();
}

void GLES2TraceImplementation::PixelStorei(GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::PixelStorei");
  gl_->PixelStorei(pname, param);
}

void GLES2TraceImplementation::PolygonOffset(GLfloat factor, GLfloat units) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::PolygonOffset");
  gl_->PolygonOffset(factor, units);
}

void GLES2TraceImplementation::ReadBuffer(GLenum src) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ReadBuffer");
  gl_->ReadBuffer(src);
}

void GLES2TraceImplementation::ReadPixels(GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ReadPixels");
  gl_->ReadPixels(x, y, width, height, format, type, pixels);
}

void GLES2TraceImplementation::ReleaseShaderCompiler() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ReleaseShaderCompiler");
  gl_->ReleaseShaderCompiler();
}

void GLES2TraceImplementation::RenderbufferStorage(GLenum target,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::RenderbufferStorage");
  gl_->RenderbufferStorage(target, internalformat, width, height);
}

void GLES2TraceImplementation::ResumeTransformFeedback() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ResumeTransformFeedback");
  gl_->ResumeTransformFeedback();
}

void GLES2TraceImplementation::SampleCoverage(GLclampf value,
                                              GLboolean invert) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::SampleCoverage");
  gl_->SampleCoverage(value, invert);
}

void GLES2TraceImplementation::SamplerParameterf(GLuint sampler,
                                                 GLenum pname,
                                                 GLfloat param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::SamplerParameterf");
  gl_->SamplerParameterf(sampler, pname, param);
}

void GLES2TraceImplementation::SamplerParameterfv(GLuint sampler,
                                                  GLenum pname,
                                                  const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::SamplerParameterfv");
  gl_->SamplerParameterfv(sampler, pname, params);
}

void GLES2TraceImplementation::SamplerParameteri(GLuint sampler,
                                                 GLenum pname,
                                                 GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::SamplerParameteri");
  gl_->SamplerParameteri(sampler, pname, param);
}

void GLES2TraceImplementation::SamplerParameteriv(GLuint sampler,
                                                  GLenum pname,
                                                  const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::SamplerParameteriv");
  gl_->SamplerParameteriv(sampler, pname, params);
}

void GLES2TraceImplementation::Scissor(GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Scissor");
  gl_->Scissor(x, y, width, height);
}

void GLES2TraceImplementation::ShaderBinary(GLsizei n,
                                            const GLuint* shaders,
                                            GLenum binaryformat,
                                            const void* binary,
                                            GLsizei length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ShaderBinary");
  gl_->ShaderBinary(n, shaders, binaryformat, binary, length);
}

void GLES2TraceImplementation::ShaderSource(GLuint shader,
                                            GLsizei count,
                                            const GLchar* const* str,
                                            const GLint* length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ShaderSource");
  gl_->ShaderSource(shader, count, str, length);
}

void GLES2TraceImplementation::ShallowFinishCHROMIUM() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ShallowFinishCHROMIUM");
  gl_->ShallowFinishCHROMIUM();
}

void GLES2TraceImplementation::OrderingBarrierCHROMIUM() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::OrderingBarrierCHROMIUM");
  gl_->OrderingBarrierCHROMIUM();
}

void GLES2TraceImplementation::MultiDrawArraysWEBGL(GLenum mode,
                                                    const GLint* firsts,
                                                    const GLsizei* counts,
                                                    GLsizei drawcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::MultiDrawArraysWEBGL");
  gl_->MultiDrawArraysWEBGL(mode, firsts, counts, drawcount);
}

void GLES2TraceImplementation::MultiDrawArraysInstancedWEBGL(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instance_counts,
    GLsizei drawcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::MultiDrawArraysInstancedWEBGL");
  gl_->MultiDrawArraysInstancedWEBGL(mode, firsts, counts, instance_counts,
                                     drawcount);
}

void GLES2TraceImplementation::MultiDrawArraysInstancedBaseInstanceWEBGL(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instance_counts,
    const GLuint* baseinstances,
    GLsizei drawcount) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::MultiDrawArraysInstancedBaseInstanceWEBGL");
  gl_->MultiDrawArraysInstancedBaseInstanceWEBGL(
      mode, firsts, counts, instance_counts, baseinstances, drawcount);
}

void GLES2TraceImplementation::MultiDrawElementsWEBGL(GLenum mode,
                                                      const GLsizei* counts,
                                                      GLenum type,
                                                      const GLsizei* offsets,
                                                      GLsizei drawcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::MultiDrawElementsWEBGL");
  gl_->MultiDrawElementsWEBGL(mode, counts, type, offsets, drawcount);
}

void GLES2TraceImplementation::MultiDrawElementsInstancedWEBGL(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLsizei* offsets,
    const GLsizei* instance_counts,
    GLsizei drawcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::MultiDrawElementsInstancedWEBGL");
  gl_->MultiDrawElementsInstancedWEBGL(mode, counts, type, offsets,
                                       instance_counts, drawcount);
}

void GLES2TraceImplementation::
    MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
        GLenum mode,
        const GLsizei* counts,
        GLenum type,
        const GLsizei* offsets,
        const GLsizei* instance_counts,
        const GLint* basevertices,
        const GLuint* baseinstances,
        GLsizei drawcount) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu",
      "GLES2Trace::MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL");
  gl_->MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
      mode, counts, type, offsets, instance_counts, basevertices, baseinstances,
      drawcount);
}

void GLES2TraceImplementation::StencilFunc(GLenum func,
                                           GLint ref,
                                           GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::StencilFunc");
  gl_->StencilFunc(func, ref, mask);
}

void GLES2TraceImplementation::StencilFuncSeparate(GLenum face,
                                                   GLenum func,
                                                   GLint ref,
                                                   GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::StencilFuncSeparate");
  gl_->StencilFuncSeparate(face, func, ref, mask);
}

void GLES2TraceImplementation::StencilMask(GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::StencilMask");
  gl_->StencilMask(mask);
}

void GLES2TraceImplementation::StencilMaskSeparate(GLenum face, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::StencilMaskSeparate");
  gl_->StencilMaskSeparate(face, mask);
}

void GLES2TraceImplementation::StencilOp(GLenum fail,
                                         GLenum zfail,
                                         GLenum zpass) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::StencilOp");
  gl_->StencilOp(fail, zfail, zpass);
}

void GLES2TraceImplementation::StencilOpSeparate(GLenum face,
                                                 GLenum fail,
                                                 GLenum zfail,
                                                 GLenum zpass) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::StencilOpSeparate");
  gl_->StencilOpSeparate(face, fail, zfail, zpass);
}

void GLES2TraceImplementation::TexImage2D(GLenum target,
                                          GLint level,
                                          GLint internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLenum format,
                                          GLenum type,
                                          const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexImage2D");
  gl_->TexImage2D(target, level, internalformat, width, height, border, format,
                  type, pixels);
}

void GLES2TraceImplementation::TexImage3D(GLenum target,
                                          GLint level,
                                          GLint internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLint border,
                                          GLenum format,
                                          GLenum type,
                                          const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexImage3D");
  gl_->TexImage3D(target, level, internalformat, width, height, depth, border,
                  format, type, pixels);
}

void GLES2TraceImplementation::TexParameterf(GLenum target,
                                             GLenum pname,
                                             GLfloat param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexParameterf");
  gl_->TexParameterf(target, pname, param);
}

void GLES2TraceImplementation::TexParameterfv(GLenum target,
                                              GLenum pname,
                                              const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexParameterfv");
  gl_->TexParameterfv(target, pname, params);
}

void GLES2TraceImplementation::TexParameteri(GLenum target,
                                             GLenum pname,
                                             GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexParameteri");
  gl_->TexParameteri(target, pname, param);
}

void GLES2TraceImplementation::TexParameteriv(GLenum target,
                                              GLenum pname,
                                              const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexParameteriv");
  gl_->TexParameteriv(target, pname, params);
}

void GLES2TraceImplementation::TexStorage3D(GLenum target,
                                            GLsizei levels,
                                            GLenum internalFormat,
                                            GLsizei width,
                                            GLsizei height,
                                            GLsizei depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexStorage3D");
  gl_->TexStorage3D(target, levels, internalFormat, width, height, depth);
}

void GLES2TraceImplementation::TexSubImage2D(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLenum type,
                                             const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexSubImage2D");
  gl_->TexSubImage2D(target, level, xoffset, yoffset, width, height, format,
                     type, pixels);
}

void GLES2TraceImplementation::TexSubImage3D(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLint zoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLenum format,
                                             GLenum type,
                                             const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexSubImage3D");
  gl_->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height,
                     depth, format, type, pixels);
}

void GLES2TraceImplementation::TransformFeedbackVaryings(
    GLuint program,
    GLsizei count,
    const char* const* varyings,
    GLenum buffermode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TransformFeedbackVaryings");
  gl_->TransformFeedbackVaryings(program, count, varyings, buffermode);
}

void GLES2TraceImplementation::Uniform1f(GLint location, GLfloat x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform1f");
  gl_->Uniform1f(location, x);
}

void GLES2TraceImplementation::Uniform1fv(GLint location,
                                          GLsizei count,
                                          const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform1fv");
  gl_->Uniform1fv(location, count, v);
}

void GLES2TraceImplementation::Uniform1i(GLint location, GLint x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform1i");
  gl_->Uniform1i(location, x);
}

void GLES2TraceImplementation::Uniform1iv(GLint location,
                                          GLsizei count,
                                          const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform1iv");
  gl_->Uniform1iv(location, count, v);
}

void GLES2TraceImplementation::Uniform1ui(GLint location, GLuint x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform1ui");
  gl_->Uniform1ui(location, x);
}

void GLES2TraceImplementation::Uniform1uiv(GLint location,
                                           GLsizei count,
                                           const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform1uiv");
  gl_->Uniform1uiv(location, count, v);
}

void GLES2TraceImplementation::Uniform2f(GLint location, GLfloat x, GLfloat y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform2f");
  gl_->Uniform2f(location, x, y);
}

void GLES2TraceImplementation::Uniform2fv(GLint location,
                                          GLsizei count,
                                          const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform2fv");
  gl_->Uniform2fv(location, count, v);
}

void GLES2TraceImplementation::Uniform2i(GLint location, GLint x, GLint y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform2i");
  gl_->Uniform2i(location, x, y);
}

void GLES2TraceImplementation::Uniform2iv(GLint location,
                                          GLsizei count,
                                          const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform2iv");
  gl_->Uniform2iv(location, count, v);
}

void GLES2TraceImplementation::Uniform2ui(GLint location, GLuint x, GLuint y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform2ui");
  gl_->Uniform2ui(location, x, y);
}

void GLES2TraceImplementation::Uniform2uiv(GLint location,
                                           GLsizei count,
                                           const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform2uiv");
  gl_->Uniform2uiv(location, count, v);
}

void GLES2TraceImplementation::Uniform3f(GLint location,
                                         GLfloat x,
                                         GLfloat y,
                                         GLfloat z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform3f");
  gl_->Uniform3f(location, x, y, z);
}

void GLES2TraceImplementation::Uniform3fv(GLint location,
                                          GLsizei count,
                                          const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform3fv");
  gl_->Uniform3fv(location, count, v);
}

void GLES2TraceImplementation::Uniform3i(GLint location,
                                         GLint x,
                                         GLint y,
                                         GLint z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform3i");
  gl_->Uniform3i(location, x, y, z);
}

void GLES2TraceImplementation::Uniform3iv(GLint location,
                                          GLsizei count,
                                          const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform3iv");
  gl_->Uniform3iv(location, count, v);
}

void GLES2TraceImplementation::Uniform3ui(GLint location,
                                          GLuint x,
                                          GLuint y,
                                          GLuint z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform3ui");
  gl_->Uniform3ui(location, x, y, z);
}

void GLES2TraceImplementation::Uniform3uiv(GLint location,
                                           GLsizei count,
                                           const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform3uiv");
  gl_->Uniform3uiv(location, count, v);
}

void GLES2TraceImplementation::Uniform4f(GLint location,
                                         GLfloat x,
                                         GLfloat y,
                                         GLfloat z,
                                         GLfloat w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform4f");
  gl_->Uniform4f(location, x, y, z, w);
}

void GLES2TraceImplementation::Uniform4fv(GLint location,
                                          GLsizei count,
                                          const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform4fv");
  gl_->Uniform4fv(location, count, v);
}

void GLES2TraceImplementation::Uniform4i(GLint location,
                                         GLint x,
                                         GLint y,
                                         GLint z,
                                         GLint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform4i");
  gl_->Uniform4i(location, x, y, z, w);
}

void GLES2TraceImplementation::Uniform4iv(GLint location,
                                          GLsizei count,
                                          const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform4iv");
  gl_->Uniform4iv(location, count, v);
}

void GLES2TraceImplementation::Uniform4ui(GLint location,
                                          GLuint x,
                                          GLuint y,
                                          GLuint z,
                                          GLuint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform4ui");
  gl_->Uniform4ui(location, x, y, z, w);
}

void GLES2TraceImplementation::Uniform4uiv(GLint location,
                                           GLsizei count,
                                           const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Uniform4uiv");
  gl_->Uniform4uiv(location, count, v);
}

void GLES2TraceImplementation::UniformBlockBinding(GLuint program,
                                                   GLuint index,
                                                   GLuint binding) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformBlockBinding");
  gl_->UniformBlockBinding(program, index, binding);
}

void GLES2TraceImplementation::UniformMatrix2fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformMatrix2fv");
  gl_->UniformMatrix2fv(location, count, transpose, value);
}

void GLES2TraceImplementation::UniformMatrix2x3fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformMatrix2x3fv");
  gl_->UniformMatrix2x3fv(location, count, transpose, value);
}

void GLES2TraceImplementation::UniformMatrix2x4fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformMatrix2x4fv");
  gl_->UniformMatrix2x4fv(location, count, transpose, value);
}

void GLES2TraceImplementation::UniformMatrix3fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformMatrix3fv");
  gl_->UniformMatrix3fv(location, count, transpose, value);
}

void GLES2TraceImplementation::UniformMatrix3x2fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformMatrix3x2fv");
  gl_->UniformMatrix3x2fv(location, count, transpose, value);
}

void GLES2TraceImplementation::UniformMatrix3x4fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformMatrix3x4fv");
  gl_->UniformMatrix3x4fv(location, count, transpose, value);
}

void GLES2TraceImplementation::UniformMatrix4fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformMatrix4fv");
  gl_->UniformMatrix4fv(location, count, transpose, value);
}

void GLES2TraceImplementation::UniformMatrix4x2fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformMatrix4x2fv");
  gl_->UniformMatrix4x2fv(location, count, transpose, value);
}

void GLES2TraceImplementation::UniformMatrix4x3fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UniformMatrix4x3fv");
  gl_->UniformMatrix4x3fv(location, count, transpose, value);
}

void GLES2TraceImplementation::UseProgram(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UseProgram");
  gl_->UseProgram(program);
}

void GLES2TraceImplementation::ValidateProgram(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ValidateProgram");
  gl_->ValidateProgram(program);
}

void GLES2TraceImplementation::VertexAttrib1f(GLuint indx, GLfloat x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttrib1f");
  gl_->VertexAttrib1f(indx, x);
}

void GLES2TraceImplementation::VertexAttrib1fv(GLuint indx,
                                               const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttrib1fv");
  gl_->VertexAttrib1fv(indx, values);
}

void GLES2TraceImplementation::VertexAttrib2f(GLuint indx,
                                              GLfloat x,
                                              GLfloat y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttrib2f");
  gl_->VertexAttrib2f(indx, x, y);
}

void GLES2TraceImplementation::VertexAttrib2fv(GLuint indx,
                                               const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttrib2fv");
  gl_->VertexAttrib2fv(indx, values);
}

void GLES2TraceImplementation::VertexAttrib3f(GLuint indx,
                                              GLfloat x,
                                              GLfloat y,
                                              GLfloat z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttrib3f");
  gl_->VertexAttrib3f(indx, x, y, z);
}

void GLES2TraceImplementation::VertexAttrib3fv(GLuint indx,
                                               const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttrib3fv");
  gl_->VertexAttrib3fv(indx, values);
}

void GLES2TraceImplementation::VertexAttrib4f(GLuint indx,
                                              GLfloat x,
                                              GLfloat y,
                                              GLfloat z,
                                              GLfloat w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttrib4f");
  gl_->VertexAttrib4f(indx, x, y, z, w);
}

void GLES2TraceImplementation::VertexAttrib4fv(GLuint indx,
                                               const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttrib4fv");
  gl_->VertexAttrib4fv(indx, values);
}

void GLES2TraceImplementation::VertexAttribI4i(GLuint indx,
                                               GLint x,
                                               GLint y,
                                               GLint z,
                                               GLint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttribI4i");
  gl_->VertexAttribI4i(indx, x, y, z, w);
}

void GLES2TraceImplementation::VertexAttribI4iv(GLuint indx,
                                                const GLint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttribI4iv");
  gl_->VertexAttribI4iv(indx, values);
}

void GLES2TraceImplementation::VertexAttribI4ui(GLuint indx,
                                                GLuint x,
                                                GLuint y,
                                                GLuint z,
                                                GLuint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttribI4ui");
  gl_->VertexAttribI4ui(indx, x, y, z, w);
}

void GLES2TraceImplementation::VertexAttribI4uiv(GLuint indx,
                                                 const GLuint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttribI4uiv");
  gl_->VertexAttribI4uiv(indx, values);
}

void GLES2TraceImplementation::VertexAttribIPointer(GLuint indx,
                                                    GLint size,
                                                    GLenum type,
                                                    GLsizei stride,
                                                    const void* ptr) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttribIPointer");
  gl_->VertexAttribIPointer(indx, size, type, stride, ptr);
}

void GLES2TraceImplementation::VertexAttribPointer(GLuint indx,
                                                   GLint size,
                                                   GLenum type,
                                                   GLboolean normalized,
                                                   GLsizei stride,
                                                   const void* ptr) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttribPointer");
  gl_->VertexAttribPointer(indx, size, type, normalized, stride, ptr);
}

void GLES2TraceImplementation::Viewport(GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::Viewport");
  gl_->Viewport(x, y, width, height);
}

void GLES2TraceImplementation::WaitSync(GLsync sync,
                                        GLbitfield flags,
                                        GLuint64 timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::WaitSync");
  gl_->WaitSync(sync, flags, timeout);
}

void GLES2TraceImplementation::BlitFramebufferCHROMIUM(GLint srcX0,
                                                       GLint srcY0,
                                                       GLint srcX1,
                                                       GLint srcY1,
                                                       GLint dstX0,
                                                       GLint dstY0,
                                                       GLint dstX1,
                                                       GLint dstY1,
                                                       GLbitfield mask,
                                                       GLenum filter) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlitFramebufferCHROMIUM");
  gl_->BlitFramebufferCHROMIUM(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                               dstY1, mask, filter);
}

void GLES2TraceImplementation::RenderbufferStorageMultisampleCHROMIUM(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::RenderbufferStorageMultisampleCHROMIUM");
  gl_->RenderbufferStorageMultisampleCHROMIUM(target, samples, internalformat,
                                              width, height);
}

void GLES2TraceImplementation::RenderbufferStorageMultisampleAdvancedAMD(
    GLenum target,
    GLsizei samples,
    GLsizei storageSamples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::RenderbufferStorageMultisampleAdvancedAMD");
  gl_->RenderbufferStorageMultisampleAdvancedAMD(
      target, samples, storageSamples, internalformat, width, height);
}

void GLES2TraceImplementation::RenderbufferStorageMultisampleEXT(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::RenderbufferStorageMultisampleEXT");
  gl_->RenderbufferStorageMultisampleEXT(target, samples, internalformat, width,
                                         height);
}

void GLES2TraceImplementation::FramebufferTexture2DMultisampleEXT(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::FramebufferTexture2DMultisampleEXT");
  gl_->FramebufferTexture2DMultisampleEXT(target, attachment, textarget,
                                          texture, level, samples);
}

void GLES2TraceImplementation::TexStorage2DEXT(GLenum target,
                                               GLsizei levels,
                                               GLenum internalFormat,
                                               GLsizei width,
                                               GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TexStorage2DEXT");
  gl_->TexStorage2DEXT(target, levels, internalFormat, width, height);
}

void GLES2TraceImplementation::GenQueriesEXT(GLsizei n, GLuint* queries) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GenQueriesEXT");
  gl_->GenQueriesEXT(n, queries);
}

void GLES2TraceImplementation::DeleteQueriesEXT(GLsizei n,
                                                const GLuint* queries) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteQueriesEXT");
  gl_->DeleteQueriesEXT(n, queries);
}

void GLES2TraceImplementation::QueryCounterEXT(GLuint id, GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::QueryCounterEXT");
  gl_->QueryCounterEXT(id, target);
}

GLboolean GLES2TraceImplementation::IsQueryEXT(GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsQueryEXT");
  return gl_->IsQueryEXT(id);
}

void GLES2TraceImplementation::BeginQueryEXT(GLenum target, GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BeginQueryEXT");
  gl_->BeginQueryEXT(target, id);
}

void GLES2TraceImplementation::BeginTransformFeedback(GLenum primitivemode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BeginTransformFeedback");
  gl_->BeginTransformFeedback(primitivemode);
}

void GLES2TraceImplementation::EndQueryEXT(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::EndQueryEXT");
  gl_->EndQueryEXT(target);
}

void GLES2TraceImplementation::EndTransformFeedback() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::EndTransformFeedback");
  gl_->EndTransformFeedback();
}

void GLES2TraceImplementation::GetQueryivEXT(GLenum target,
                                             GLenum pname,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetQueryivEXT");
  gl_->GetQueryivEXT(target, pname, params);
}

void GLES2TraceImplementation::GetQueryObjectivEXT(GLuint id,
                                                   GLenum pname,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetQueryObjectivEXT");
  gl_->GetQueryObjectivEXT(id, pname, params);
}

void GLES2TraceImplementation::GetQueryObjectuivEXT(GLuint id,
                                                    GLenum pname,
                                                    GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetQueryObjectuivEXT");
  gl_->GetQueryObjectuivEXT(id, pname, params);
}

void GLES2TraceImplementation::GetQueryObjecti64vEXT(GLuint id,
                                                     GLenum pname,
                                                     GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetQueryObjecti64vEXT");
  gl_->GetQueryObjecti64vEXT(id, pname, params);
}

void GLES2TraceImplementation::GetQueryObjectui64vEXT(GLuint id,
                                                      GLenum pname,
                                                      GLuint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetQueryObjectui64vEXT");
  gl_->GetQueryObjectui64vEXT(id, pname, params);
}

void GLES2TraceImplementation::SetDisjointValueSyncCHROMIUM() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::SetDisjointValueSyncCHROMIUM");
  gl_->SetDisjointValueSyncCHROMIUM();
}

void GLES2TraceImplementation::InsertEventMarkerEXT(GLsizei length,
                                                    const GLchar* marker) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::InsertEventMarkerEXT");
  gl_->InsertEventMarkerEXT(length, marker);
}

void GLES2TraceImplementation::PushGroupMarkerEXT(GLsizei length,
                                                  const GLchar* marker) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::PushGroupMarkerEXT");
  gl_->PushGroupMarkerEXT(length, marker);
}

void GLES2TraceImplementation::PopGroupMarkerEXT() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::PopGroupMarkerEXT");
  gl_->PopGroupMarkerEXT();
}

void GLES2TraceImplementation::GenVertexArraysOES(GLsizei n, GLuint* arrays) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GenVertexArraysOES");
  gl_->GenVertexArraysOES(n, arrays);
}

void GLES2TraceImplementation::DeleteVertexArraysOES(GLsizei n,
                                                     const GLuint* arrays) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DeleteVertexArraysOES");
  gl_->DeleteVertexArraysOES(n, arrays);
}

GLboolean GLES2TraceImplementation::IsVertexArrayOES(GLuint array) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsVertexArrayOES");
  return gl_->IsVertexArrayOES(array);
}

void GLES2TraceImplementation::BindVertexArrayOES(GLuint array) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindVertexArrayOES");
  gl_->BindVertexArrayOES(array);
}

void GLES2TraceImplementation::FramebufferParameteri(GLenum target,
                                                     GLenum pname,
                                                     GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::FramebufferParameteri");
  gl_->FramebufferParameteri(target, pname, param);
}

void GLES2TraceImplementation::BindImageTexture(GLuint unit,
                                                GLuint texture,
                                                GLint level,
                                                GLboolean layered,
                                                GLint layer,
                                                GLenum access,
                                                GLenum format) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindImageTexture");
  gl_->BindImageTexture(unit, texture, level, layered, layer, access, format);
}

void GLES2TraceImplementation::DispatchCompute(GLuint num_groups_x,
                                               GLuint num_groups_y,
                                               GLuint num_groups_z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DispatchCompute");
  gl_->DispatchCompute(num_groups_x, num_groups_y, num_groups_z);
}

void GLES2TraceImplementation::DispatchComputeIndirect(GLintptr offset) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DispatchComputeIndirect");
  gl_->DispatchComputeIndirect(offset);
}

void GLES2TraceImplementation::DrawArraysIndirect(GLenum mode,
                                                  const void* offset) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DrawArraysIndirect");
  gl_->DrawArraysIndirect(mode, offset);
}

void GLES2TraceImplementation::DrawElementsIndirect(GLenum mode,
                                                    GLenum type,
                                                    const void* offset) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DrawElementsIndirect");
  gl_->DrawElementsIndirect(mode, type, offset);
}

void GLES2TraceImplementation::GetProgramInterfaceiv(GLuint program,
                                                     GLenum program_interface,
                                                     GLenum pname,
                                                     GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetProgramInterfaceiv");
  gl_->GetProgramInterfaceiv(program, program_interface, pname, params);
}

GLuint GLES2TraceImplementation::GetProgramResourceIndex(
    GLuint program,
    GLenum program_interface,
    const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetProgramResourceIndex");
  return gl_->GetProgramResourceIndex(program, program_interface, name);
}

void GLES2TraceImplementation::GetProgramResourceName(GLuint program,
                                                      GLenum program_interface,
                                                      GLuint index,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetProgramResourceName");
  gl_->GetProgramResourceName(program, program_interface, index, bufsize,
                              length, name);
}

void GLES2TraceImplementation::GetProgramResourceiv(GLuint program,
                                                    GLenum program_interface,
                                                    GLuint index,
                                                    GLsizei prop_count,
                                                    const GLenum* props,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetProgramResourceiv");
  gl_->GetProgramResourceiv(program, program_interface, index, prop_count,
                            props, bufsize, length, params);
}

GLint GLES2TraceImplementation::GetProgramResourceLocation(
    GLuint program,
    GLenum program_interface,
    const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::GetProgramResourceLocation");
  return gl_->GetProgramResourceLocation(program, program_interface, name);
}

void GLES2TraceImplementation::MemoryBarrierEXT(GLbitfield barriers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::MemoryBarrierEXT");
  gl_->MemoryBarrierEXT(barriers);
}

void GLES2TraceImplementation::MemoryBarrierByRegion(GLbitfield barriers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::MemoryBarrierByRegion");
  gl_->MemoryBarrierByRegion(barriers);
}

void GLES2TraceImplementation::SwapBuffers(GLuint64 swap_id, GLbitfield flags) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::SwapBuffers");
  gl_->SwapBuffers(swap_id, flags);
}

GLuint GLES2TraceImplementation::GetMaxValueInBufferCHROMIUM(GLuint buffer_id,
                                                             GLsizei count,
                                                             GLenum type,
                                                             GLuint offset) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::GetMaxValueInBufferCHROMIUM");
  return gl_->GetMaxValueInBufferCHROMIUM(buffer_id, count, type, offset);
}

GLboolean GLES2TraceImplementation::EnableFeatureCHROMIUM(const char* feature) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::EnableFeatureCHROMIUM");
  return gl_->EnableFeatureCHROMIUM(feature);
}

void* GLES2TraceImplementation::MapBufferCHROMIUM(GLuint target,
                                                  GLenum access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::MapBufferCHROMIUM");
  return gl_->MapBufferCHROMIUM(target, access);
}

GLboolean GLES2TraceImplementation::UnmapBufferCHROMIUM(GLuint target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UnmapBufferCHROMIUM");
  return gl_->UnmapBufferCHROMIUM(target);
}

void* GLES2TraceImplementation::MapBufferSubDataCHROMIUM(GLuint target,
                                                         GLintptr offset,
                                                         GLsizeiptr size,
                                                         GLenum access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::MapBufferSubDataCHROMIUM");
  return gl_->MapBufferSubDataCHROMIUM(target, offset, size, access);
}

void GLES2TraceImplementation::UnmapBufferSubDataCHROMIUM(const void* mem) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::UnmapBufferSubDataCHROMIUM");
  gl_->UnmapBufferSubDataCHROMIUM(mem);
}

void* GLES2TraceImplementation::MapBufferRange(GLenum target,
                                               GLintptr offset,
                                               GLsizeiptr size,
                                               GLbitfield access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::MapBufferRange");
  return gl_->MapBufferRange(target, offset, size, access);
}

GLboolean GLES2TraceImplementation::UnmapBuffer(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::UnmapBuffer");
  return gl_->UnmapBuffer(target);
}

void GLES2TraceImplementation::FlushMappedBufferRange(GLenum target,
                                                      GLintptr offset,
                                                      GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::FlushMappedBufferRange");
  gl_->FlushMappedBufferRange(target, offset, size);
}

void* GLES2TraceImplementation::MapTexSubImage2DCHROMIUM(GLenum target,
                                                         GLint level,
                                                         GLint xoffset,
                                                         GLint yoffset,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLenum format,
                                                         GLenum type,
                                                         GLenum access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::MapTexSubImage2DCHROMIUM");
  return gl_->MapTexSubImage2DCHROMIUM(target, level, xoffset, yoffset, width,
                                       height, format, type, access);
}

void GLES2TraceImplementation::UnmapTexSubImage2DCHROMIUM(const void* mem) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::UnmapTexSubImage2DCHROMIUM");
  gl_->UnmapTexSubImage2DCHROMIUM(mem);
}

void GLES2TraceImplementation::ResizeCHROMIUM(GLuint width,
                                              GLuint height,
                                              GLfloat scale_factor,
                                              GLcolorSpace color_space,
                                              GLboolean alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ResizeCHROMIUM");
  gl_->ResizeCHROMIUM(width, height, scale_factor, color_space, alpha);
}

const GLchar* GLES2TraceImplementation::GetRequestableExtensionsCHROMIUM() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::GetRequestableExtensionsCHROMIUM");
  return gl_->GetRequestableExtensionsCHROMIUM();
}

void GLES2TraceImplementation::RequestExtensionCHROMIUM(const char* extension) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::RequestExtensionCHROMIUM");
  gl_->RequestExtensionCHROMIUM(extension);
}

void GLES2TraceImplementation::GetProgramInfoCHROMIUM(GLuint program,
                                                      GLsizei bufsize,
                                                      GLsizei* size,
                                                      void* info) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetProgramInfoCHROMIUM");
  gl_->GetProgramInfoCHROMIUM(program, bufsize, size, info);
}

void GLES2TraceImplementation::GetUniformBlocksCHROMIUM(GLuint program,
                                                        GLsizei bufsize,
                                                        GLsizei* size,
                                                        void* info) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetUniformBlocksCHROMIUM");
  gl_->GetUniformBlocksCHROMIUM(program, bufsize, size, info);
}

void GLES2TraceImplementation::GetTransformFeedbackVaryingsCHROMIUM(
    GLuint program,
    GLsizei bufsize,
    GLsizei* size,
    void* info) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::GetTransformFeedbackVaryingsCHROMIUM");
  gl_->GetTransformFeedbackVaryingsCHROMIUM(program, bufsize, size, info);
}

void GLES2TraceImplementation::GetUniformsES3CHROMIUM(GLuint program,
                                                      GLsizei bufsize,
                                                      GLsizei* size,
                                                      void* info) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetUniformsES3CHROMIUM");
  gl_->GetUniformsES3CHROMIUM(program, bufsize, size, info);
}

void GLES2TraceImplementation::DescheduleUntilFinishedCHROMIUM() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::DescheduleUntilFinishedCHROMIUM");
  gl_->DescheduleUntilFinishedCHROMIUM();
}

void GLES2TraceImplementation::GetTranslatedShaderSourceANGLE(GLuint shader,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              char* source) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::GetTranslatedShaderSourceANGLE");
  gl_->GetTranslatedShaderSourceANGLE(shader, bufsize, length, source);
}

void GLES2TraceImplementation::CopyTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint internalformat,
    GLenum dest_type,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CopyTextureCHROMIUM");
  gl_->CopyTextureCHROMIUM(source_id, source_level, dest_target, dest_id,
                           dest_level, internalformat, dest_type, unpack_flip_y,
                           unpack_premultiply_alpha, unpack_unmultiply_alpha);
}

void GLES2TraceImplementation::CopySubTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CopySubTextureCHROMIUM");
  gl_->CopySubTextureCHROMIUM(source_id, source_level, dest_target, dest_id,
                              dest_level, xoffset, yoffset, x, y, width, height,
                              unpack_flip_y, unpack_premultiply_alpha,
                              unpack_unmultiply_alpha);
}

void GLES2TraceImplementation::DrawArraysInstancedANGLE(GLenum mode,
                                                        GLint first,
                                                        GLsizei count,
                                                        GLsizei primcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DrawArraysInstancedANGLE");
  gl_->DrawArraysInstancedANGLE(mode, first, count, primcount);
}

void GLES2TraceImplementation::DrawArraysInstancedBaseInstanceANGLE(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount,
    GLuint baseinstance) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::DrawArraysInstancedBaseInstanceANGLE");
  gl_->DrawArraysInstancedBaseInstanceANGLE(mode, first, count, primcount,
                                            baseinstance);
}

void GLES2TraceImplementation::DrawElementsInstancedANGLE(GLenum mode,
                                                          GLsizei count,
                                                          GLenum type,
                                                          const void* indices,
                                                          GLsizei primcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::DrawElementsInstancedANGLE");
  gl_->DrawElementsInstancedANGLE(mode, count, type, indices, primcount);
}

void GLES2TraceImplementation::DrawElementsInstancedBaseVertexBaseInstanceANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint basevertex,
    GLuint baseinstance) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::DrawElementsInstancedBaseVertexBaseInstanceANGLE");
  gl_->DrawElementsInstancedBaseVertexBaseInstanceANGLE(
      mode, count, type, indices, primcount, basevertex, baseinstance);
}

void GLES2TraceImplementation::VertexAttribDivisorANGLE(GLuint index,
                                                        GLuint divisor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::VertexAttribDivisorANGLE");
  gl_->VertexAttribDivisorANGLE(index, divisor);
}

void GLES2TraceImplementation::BindUniformLocationCHROMIUM(GLuint program,
                                                           GLint location,
                                                           const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::BindUniformLocationCHROMIUM");
  gl_->BindUniformLocationCHROMIUM(program, location, name);
}

void GLES2TraceImplementation::TraceBeginCHROMIUM(const char* category_name,
                                                  const char* trace_name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TraceBeginCHROMIUM");
  gl_->TraceBeginCHROMIUM(category_name, trace_name);
}

void GLES2TraceImplementation::TraceEndCHROMIUM() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::TraceEndCHROMIUM");
  gl_->TraceEndCHROMIUM();
}

void GLES2TraceImplementation::DiscardFramebufferEXT(
    GLenum target,
    GLsizei count,
    const GLenum* attachments) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DiscardFramebufferEXT");
  gl_->DiscardFramebufferEXT(target, count, attachments);
}

void GLES2TraceImplementation::LoseContextCHROMIUM(GLenum current,
                                                   GLenum other) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::LoseContextCHROMIUM");
  gl_->LoseContextCHROMIUM(current, other);
}

void GLES2TraceImplementation::DrawBuffersEXT(GLsizei count,
                                              const GLenum* bufs) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DrawBuffersEXT");
  gl_->DrawBuffersEXT(count, bufs);
}

void GLES2TraceImplementation::FlushDriverCachesCHROMIUM() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::FlushDriverCachesCHROMIUM");
  gl_->FlushDriverCachesCHROMIUM();
}

GLuint GLES2TraceImplementation::GetLastFlushIdCHROMIUM() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetLastFlushIdCHROMIUM");
  return gl_->GetLastFlushIdCHROMIUM();
}

void GLES2TraceImplementation::SetActiveURLCHROMIUM(const char* url) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::SetActiveURLCHROMIUM");
  gl_->SetActiveURLCHROMIUM(url);
}

void GLES2TraceImplementation::ContextVisibilityHintCHROMIUM(
    GLboolean visibility) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::ContextVisibilityHintCHROMIUM");
  gl_->ContextVisibilityHintCHROMIUM(visibility);
}

GLenum GLES2TraceImplementation::GetGraphicsResetStatusKHR() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetGraphicsResetStatusKHR");
  return gl_->GetGraphicsResetStatusKHR();
}

void GLES2TraceImplementation::BlendBarrierKHR() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendBarrierKHR");
  gl_->BlendBarrierKHR();
}

void GLES2TraceImplementation::BindFragDataLocationIndexedEXT(
    GLuint program,
    GLuint colorNumber,
    GLuint index,
    const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::BindFragDataLocationIndexedEXT");
  gl_->BindFragDataLocationIndexedEXT(program, colorNumber, index, name);
}

void GLES2TraceImplementation::BindFragDataLocationEXT(GLuint program,
                                                       GLuint colorNumber,
                                                       const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BindFragDataLocationEXT");
  gl_->BindFragDataLocationEXT(program, colorNumber, name);
}

GLint GLES2TraceImplementation::GetFragDataIndexEXT(GLuint program,
                                                    const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::GetFragDataIndexEXT");
  return gl_->GetFragDataIndexEXT(program, name);
}

void GLES2TraceImplementation::InitializeDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::InitializeDiscardableTextureCHROMIUM");
  gl_->InitializeDiscardableTextureCHROMIUM(texture_id);
}

void GLES2TraceImplementation::UnlockDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::UnlockDiscardableTextureCHROMIUM");
  gl_->UnlockDiscardableTextureCHROMIUM(texture_id);
}

bool GLES2TraceImplementation::LockDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::LockDiscardableTextureCHROMIUM");
  return gl_->LockDiscardableTextureCHROMIUM(texture_id);
}

void GLES2TraceImplementation::WindowRectanglesEXT(GLenum mode,
                                                   GLsizei count,
                                                   const GLint* box) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::WindowRectanglesEXT");
  gl_->WindowRectanglesEXT(mode, count, box);
}

GLuint GLES2TraceImplementation::CreateGpuFenceCHROMIUM() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CreateGpuFenceCHROMIUM");
  return gl_->CreateGpuFenceCHROMIUM();
}

GLuint GLES2TraceImplementation::CreateClientGpuFenceCHROMIUM(
    ClientGpuFence source) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::CreateClientGpuFenceCHROMIUM");
  return gl_->CreateClientGpuFenceCHROMIUM(source);
}

void GLES2TraceImplementation::WaitGpuFenceCHROMIUM(GLuint gpu_fence_id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::WaitGpuFenceCHROMIUM");
  gl_->WaitGpuFenceCHROMIUM(gpu_fence_id);
}

void GLES2TraceImplementation::DestroyGpuFenceCHROMIUM(GLuint gpu_fence_id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DestroyGpuFenceCHROMIUM");
  gl_->DestroyGpuFenceCHROMIUM(gpu_fence_id);
}

void GLES2TraceImplementation::InvalidateReadbackBufferShadowDataCHROMIUM(
    GLuint buffer_id) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::InvalidateReadbackBufferShadowDataCHROMIUM");
  gl_->InvalidateReadbackBufferShadowDataCHROMIUM(buffer_id);
}

void GLES2TraceImplementation::FramebufferTextureMultiviewOVR(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::FramebufferTextureMultiviewOVR");
  gl_->FramebufferTextureMultiviewOVR(target, attachment, texture, level,
                                      baseViewIndex, numViews);
}

void GLES2TraceImplementation::MaxShaderCompilerThreadsKHR(GLuint count) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::MaxShaderCompilerThreadsKHR");
  gl_->MaxShaderCompilerThreadsKHR(count);
}

GLuint GLES2TraceImplementation::CreateAndTexStorage2DSharedImageCHROMIUM(
    const GLbyte* mailbox) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::CreateAndTexStorage2DSharedImageCHROMIUM");
  return gl_->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox);
}

void GLES2TraceImplementation::BeginSharedImageAccessDirectCHROMIUM(
    GLuint texture,
    GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::BeginSharedImageAccessDirectCHROMIUM");
  gl_->BeginSharedImageAccessDirectCHROMIUM(texture, mode);
}

void GLES2TraceImplementation::EndSharedImageAccessDirectCHROMIUM(
    GLuint texture) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::EndSharedImageAccessDirectCHROMIUM");
  gl_->EndSharedImageAccessDirectCHROMIUM(texture);
}

void GLES2TraceImplementation::CopySharedImageINTERNAL(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    const GLbyte* mailboxes) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::CopySharedImageINTERNAL");
  gl_->CopySharedImageINTERNAL(xoffset, yoffset, x, y, width, height,
                               unpack_flip_y, mailboxes);
}

void GLES2TraceImplementation::CopySharedImageToTextureINTERNAL(
    GLuint texture,
    GLenum target,
    GLuint internal_format,
    GLenum type,
    GLint src_x,
    GLint src_y,
    GLsizei width,
    GLsizei height,
    GLboolean flip_y,
    const GLbyte* src_mailbox) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::CopySharedImageToTextureINTERNAL");
  gl_->CopySharedImageToTextureINTERNAL(texture, target, internal_format, type,
                                        src_x, src_y, width, height, flip_y,
                                        src_mailbox);
}

GLboolean GLES2TraceImplementation::ReadbackARGBImagePixelsINTERNAL(
    const GLbyte* mailbox,
    const void* dst_color_space,
    GLuint dst_color_space_size,
    GLuint dst_size,
    GLuint dst_width,
    GLuint dst_height,
    GLuint dst_color_type,
    GLuint dst_alpha_type,
    GLuint dst_row_bytes,
    GLint src_x,
    GLint src_y,
    GLint plane_index,
    void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::ReadbackARGBImagePixelsINTERNAL");
  return gl_->ReadbackARGBImagePixelsINTERNAL(
      mailbox, dst_color_space, dst_color_space_size, dst_size, dst_width,
      dst_height, dst_color_type, dst_alpha_type, dst_row_bytes, src_x, src_y,
      plane_index, pixels);
}

void GLES2TraceImplementation::WritePixelsYUVINTERNAL(
    const GLbyte* mailbox,
    GLuint src_size_plane1,
    GLuint src_size_plane2,
    GLuint src_size_plane3,
    GLuint src_size_plane4,
    GLuint src_width,
    GLuint src_height,
    GLuint src_plane_config,
    GLuint src_subsampling,
    GLuint src_datatype,
    GLuint src_row_bytes_plane1,
    GLuint src_row_bytes_plane2,
    GLuint src_row_bytes_plane3,
    GLuint src_row_bytes_plane4,
    const void* src_pixels_plane1,
    const void* src_pixels_plane2,
    const void* src_pixels_plane3,
    const void* src_pixels_plane4) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::WritePixelsYUVINTERNAL");
  gl_->WritePixelsYUVINTERNAL(
      mailbox, src_size_plane1, src_size_plane2, src_size_plane3,
      src_size_plane4, src_width, src_height, src_plane_config, src_subsampling,
      src_datatype, src_row_bytes_plane1, src_row_bytes_plane2,
      src_row_bytes_plane3, src_row_bytes_plane4, src_pixels_plane1,
      src_pixels_plane2, src_pixels_plane3, src_pixels_plane4);
}

void GLES2TraceImplementation::EnableiOES(GLenum target, GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::EnableiOES");
  gl_->EnableiOES(target, index);
}

void GLES2TraceImplementation::DisableiOES(GLenum target, GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::DisableiOES");
  gl_->DisableiOES(target, index);
}

void GLES2TraceImplementation::BlendEquationiOES(GLuint buf, GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendEquationiOES");
  gl_->BlendEquationiOES(buf, mode);
}

void GLES2TraceImplementation::BlendEquationSeparateiOES(GLuint buf,
                                                         GLenum modeRGB,
                                                         GLenum modeAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendEquationSeparateiOES");
  gl_->BlendEquationSeparateiOES(buf, modeRGB, modeAlpha);
}

void GLES2TraceImplementation::BlendFunciOES(GLuint buf,
                                             GLenum src,
                                             GLenum dst) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendFunciOES");
  gl_->BlendFunciOES(buf, src, dst);
}

void GLES2TraceImplementation::BlendFuncSeparateiOES(GLuint buf,
                                                     GLenum srcRGB,
                                                     GLenum dstRGB,
                                                     GLenum srcAlpha,
                                                     GLenum dstAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::BlendFuncSeparateiOES");
  gl_->BlendFuncSeparateiOES(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GLES2TraceImplementation::ColorMaskiOES(GLuint buf,
                                             GLboolean r,
                                             GLboolean g,
                                             GLboolean b,
                                             GLboolean a) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ColorMaskiOES");
  gl_->ColorMaskiOES(buf, r, g, b, a);
}

GLboolean GLES2TraceImplementation::IsEnablediOES(GLenum target, GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::IsEnablediOES");
  return gl_->IsEnablediOES(target, index);
}

void GLES2TraceImplementation::ProvokingVertexANGLE(GLenum provokeMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ProvokingVertexANGLE");
  gl_->ProvokingVertexANGLE(provokeMode);
}

void GLES2TraceImplementation::FramebufferMemorylessPixelLocalStorageANGLE(
    GLint plane,
    GLenum internalformat) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::FramebufferMemorylessPixelLocalStorageANGLE");
  gl_->FramebufferMemorylessPixelLocalStorageANGLE(plane, internalformat);
}

void GLES2TraceImplementation::FramebufferTexturePixelLocalStorageANGLE(
    GLint plane,
    GLuint backingtexture,
    GLint level,
    GLint layer) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::FramebufferTexturePixelLocalStorageANGLE");
  gl_->FramebufferTexturePixelLocalStorageANGLE(plane, backingtexture, level,
                                                layer);
}

void GLES2TraceImplementation::FramebufferPixelLocalClearValuefvANGLE(
    GLint plane,
    const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::FramebufferPixelLocalClearValuefvANGLE");
  gl_->FramebufferPixelLocalClearValuefvANGLE(plane, value);
}

void GLES2TraceImplementation::FramebufferPixelLocalClearValueivANGLE(
    GLint plane,
    const GLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::FramebufferPixelLocalClearValueivANGLE");
  gl_->FramebufferPixelLocalClearValueivANGLE(plane, value);
}

void GLES2TraceImplementation::FramebufferPixelLocalClearValueuivANGLE(
    GLint plane,
    const GLuint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::FramebufferPixelLocalClearValueuivANGLE");
  gl_->FramebufferPixelLocalClearValueuivANGLE(plane, value);
}

void GLES2TraceImplementation::BeginPixelLocalStorageANGLE(
    GLsizei count,
    const GLenum* loadops) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::BeginPixelLocalStorageANGLE");
  gl_->BeginPixelLocalStorageANGLE(count, loadops);
}

void GLES2TraceImplementation::EndPixelLocalStorageANGLE(
    GLsizei count,
    const GLenum* storeops) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::EndPixelLocalStorageANGLE");
  gl_->EndPixelLocalStorageANGLE(count, storeops);
}

void GLES2TraceImplementation::PixelLocalStorageBarrierANGLE() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "GLES2Trace::PixelLocalStorageBarrierANGLE");
  gl_->PixelLocalStorageBarrierANGLE();
}

void GLES2TraceImplementation::FramebufferPixelLocalStorageInterruptANGLE() {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::FramebufferPixelLocalStorageInterruptANGLE");
  gl_->FramebufferPixelLocalStorageInterruptANGLE();
}

void GLES2TraceImplementation::FramebufferPixelLocalStorageRestoreANGLE() {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::FramebufferPixelLocalStorageRestoreANGLE");
  gl_->FramebufferPixelLocalStorageRestoreANGLE();
}

void GLES2TraceImplementation::GetFramebufferPixelLocalStorageParameterfvANGLE(
    GLint plane,
    GLenum pname,
    GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::GetFramebufferPixelLocalStorageParameterfvANGLE");
  gl_->GetFramebufferPixelLocalStorageParameterfvANGLE(plane, pname, params);
}

void GLES2TraceImplementation::GetFramebufferPixelLocalStorageParameterivANGLE(
    GLint plane,
    GLenum pname,
    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "GLES2Trace::GetFramebufferPixelLocalStorageParameterivANGLE");
  gl_->GetFramebufferPixelLocalStorageParameterivANGLE(plane, pname, params);
}

void GLES2TraceImplementation::ClipControlEXT(GLenum origin, GLenum depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::ClipControlEXT");
  gl_->ClipControlEXT(origin, depth);
}

void GLES2TraceImplementation::PolygonModeANGLE(GLenum face, GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::PolygonModeANGLE");
  gl_->PolygonModeANGLE(face, mode);
}

void GLES2TraceImplementation::PolygonOffsetClampEXT(GLfloat factor,
                                                     GLfloat units,
                                                     GLfloat clamp) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "GLES2Trace::PolygonOffsetClampEXT");
  gl_->PolygonOffsetClampEXT(factor, units, clamp);
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_TRACE_IMPLEMENTATION_IMPL_AUTOGEN_H_
