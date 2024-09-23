// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// These functions emulate GLES2 over command buffers.
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_C_LIB_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_C_LIB_AUTOGEN_H_

void GL_APIENTRY GLES2ActiveTexture(GLenum texture) {
  gles2::GetGLContext()->ActiveTexture(texture);
}
void GL_APIENTRY GLES2AttachShader(GLuint program, GLuint shader) {
  gles2::GetGLContext()->AttachShader(program, shader);
}
void GL_APIENTRY GLES2BindAttribLocation(GLuint program,
                                         GLuint index,
                                         const char* name) {
  gles2::GetGLContext()->BindAttribLocation(program, index, name);
}
void GL_APIENTRY GLES2BindBuffer(GLenum target, GLuint buffer) {
  gles2::GetGLContext()->BindBuffer(target, buffer);
}
void GL_APIENTRY GLES2BindBufferBase(GLenum target,
                                     GLuint index,
                                     GLuint buffer) {
  gles2::GetGLContext()->BindBufferBase(target, index, buffer);
}
void GL_APIENTRY GLES2BindBufferRange(GLenum target,
                                      GLuint index,
                                      GLuint buffer,
                                      GLintptr offset,
                                      GLsizeiptr size) {
  gles2::GetGLContext()->BindBufferRange(target, index, buffer, offset, size);
}
void GL_APIENTRY GLES2BindFramebuffer(GLenum target, GLuint framebuffer) {
  gles2::GetGLContext()->BindFramebuffer(target, framebuffer);
}
void GL_APIENTRY GLES2BindRenderbuffer(GLenum target, GLuint renderbuffer) {
  gles2::GetGLContext()->BindRenderbuffer(target, renderbuffer);
}
void GL_APIENTRY GLES2BindSampler(GLuint unit, GLuint sampler) {
  gles2::GetGLContext()->BindSampler(unit, sampler);
}
void GL_APIENTRY GLES2BindTexture(GLenum target, GLuint texture) {
  gles2::GetGLContext()->BindTexture(target, texture);
}
void GL_APIENTRY GLES2BindTransformFeedback(GLenum target,
                                            GLuint transformfeedback) {
  gles2::GetGLContext()->BindTransformFeedback(target, transformfeedback);
}
void GL_APIENTRY GLES2BlendColor(GLclampf red,
                                 GLclampf green,
                                 GLclampf blue,
                                 GLclampf alpha) {
  gles2::GetGLContext()->BlendColor(red, green, blue, alpha);
}
void GL_APIENTRY GLES2BlendEquation(GLenum mode) {
  gles2::GetGLContext()->BlendEquation(mode);
}
void GL_APIENTRY GLES2BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  gles2::GetGLContext()->BlendEquationSeparate(modeRGB, modeAlpha);
}
void GL_APIENTRY GLES2BlendFunc(GLenum sfactor, GLenum dfactor) {
  gles2::GetGLContext()->BlendFunc(sfactor, dfactor);
}
void GL_APIENTRY GLES2BlendFuncSeparate(GLenum srcRGB,
                                        GLenum dstRGB,
                                        GLenum srcAlpha,
                                        GLenum dstAlpha) {
  gles2::GetGLContext()->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}
void GL_APIENTRY GLES2BufferData(GLenum target,
                                 GLsizeiptr size,
                                 const void* data,
                                 GLenum usage) {
  gles2::GetGLContext()->BufferData(target, size, data, usage);
}
void GL_APIENTRY GLES2BufferSubData(GLenum target,
                                    GLintptr offset,
                                    GLsizeiptr size,
                                    const void* data) {
  gles2::GetGLContext()->BufferSubData(target, offset, size, data);
}
GLenum GL_APIENTRY GLES2CheckFramebufferStatus(GLenum target) {
  return gles2::GetGLContext()->CheckFramebufferStatus(target);
}
void GL_APIENTRY GLES2Clear(GLbitfield mask) {
  gles2::GetGLContext()->Clear(mask);
}
void GL_APIENTRY GLES2ClearBufferfi(GLenum buffer,
                                    GLint drawbuffers,
                                    GLfloat depth,
                                    GLint stencil) {
  gles2::GetGLContext()->ClearBufferfi(buffer, drawbuffers, depth, stencil);
}
void GL_APIENTRY GLES2ClearBufferfv(GLenum buffer,
                                    GLint drawbuffers,
                                    const GLfloat* value) {
  gles2::GetGLContext()->ClearBufferfv(buffer, drawbuffers, value);
}
void GL_APIENTRY GLES2ClearBufferiv(GLenum buffer,
                                    GLint drawbuffers,
                                    const GLint* value) {
  gles2::GetGLContext()->ClearBufferiv(buffer, drawbuffers, value);
}
void GL_APIENTRY GLES2ClearBufferuiv(GLenum buffer,
                                     GLint drawbuffers,
                                     const GLuint* value) {
  gles2::GetGLContext()->ClearBufferuiv(buffer, drawbuffers, value);
}
void GL_APIENTRY GLES2ClearColor(GLclampf red,
                                 GLclampf green,
                                 GLclampf blue,
                                 GLclampf alpha) {
  gles2::GetGLContext()->ClearColor(red, green, blue, alpha);
}
void GL_APIENTRY GLES2ClearDepthf(GLclampf depth) {
  gles2::GetGLContext()->ClearDepthf(depth);
}
void GL_APIENTRY GLES2ClearStencil(GLint s) {
  gles2::GetGLContext()->ClearStencil(s);
}
GLenum GL_APIENTRY GLES2ClientWaitSync(GLsync sync,
                                       GLbitfield flags,
                                       GLuint64 timeout) {
  return gles2::GetGLContext()->ClientWaitSync(sync, flags, timeout);
}
void GL_APIENTRY GLES2ColorMask(GLboolean red,
                                GLboolean green,
                                GLboolean blue,
                                GLboolean alpha) {
  gles2::GetGLContext()->ColorMask(red, green, blue, alpha);
}
void GL_APIENTRY GLES2CompileShader(GLuint shader) {
  gles2::GetGLContext()->CompileShader(shader);
}
void GL_APIENTRY GLES2CompressedTexImage2D(GLenum target,
                                           GLint level,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border,
                                           GLsizei imageSize,
                                           const void* data) {
  gles2::GetGLContext()->CompressedTexImage2D(
      target, level, internalformat, width, height, border, imageSize, data);
}
void GL_APIENTRY GLES2CompressedTexSubImage2D(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLsizei imageSize,
                                              const void* data) {
  gles2::GetGLContext()->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void GL_APIENTRY GLES2CompressedTexImage3D(GLenum target,
                                           GLint level,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLint border,
                                           GLsizei imageSize,
                                           const void* data) {
  gles2::GetGLContext()->CompressedTexImage3D(target, level, internalformat,
                                              width, height, depth, border,
                                              imageSize, data);
}
void GL_APIENTRY GLES2CompressedTexSubImage3D(GLenum target,
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
  gles2::GetGLContext()->CompressedTexSubImage3D(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      imageSize, data);
}
void GL_APIENTRY GLES2CopyBufferSubData(GLenum readtarget,
                                        GLenum writetarget,
                                        GLintptr readoffset,
                                        GLintptr writeoffset,
                                        GLsizeiptr size) {
  gles2::GetGLContext()->CopyBufferSubData(readtarget, writetarget, readoffset,
                                           writeoffset, size);
}
void GL_APIENTRY GLES2CopyTexImage2D(GLenum target,
                                     GLint level,
                                     GLenum internalformat,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height,
                                     GLint border) {
  gles2::GetGLContext()->CopyTexImage2D(target, level, internalformat, x, y,
                                        width, height, border);
}
void GL_APIENTRY GLES2CopyTexSubImage2D(GLenum target,
                                        GLint level,
                                        GLint xoffset,
                                        GLint yoffset,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height) {
  gles2::GetGLContext()->CopyTexSubImage2D(target, level, xoffset, yoffset, x,
                                           y, width, height);
}
void GL_APIENTRY GLES2CopyTexSubImage3D(GLenum target,
                                        GLint level,
                                        GLint xoffset,
                                        GLint yoffset,
                                        GLint zoffset,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height) {
  gles2::GetGLContext()->CopyTexSubImage3D(target, level, xoffset, yoffset,
                                           zoffset, x, y, width, height);
}
GLuint GL_APIENTRY GLES2CreateProgram() {
  return gles2::GetGLContext()->CreateProgram();
}
GLuint GL_APIENTRY GLES2CreateShader(GLenum type) {
  return gles2::GetGLContext()->CreateShader(type);
}
void GL_APIENTRY GLES2CullFace(GLenum mode) {
  gles2::GetGLContext()->CullFace(mode);
}
void GL_APIENTRY GLES2DeleteBuffers(GLsizei n, const GLuint* buffers) {
  gles2::GetGLContext()->DeleteBuffers(n, buffers);
}
void GL_APIENTRY GLES2DeleteFramebuffers(GLsizei n,
                                         const GLuint* framebuffers) {
  gles2::GetGLContext()->DeleteFramebuffers(n, framebuffers);
}
void GL_APIENTRY GLES2DeleteProgram(GLuint program) {
  gles2::GetGLContext()->DeleteProgram(program);
}
void GL_APIENTRY GLES2DeleteRenderbuffers(GLsizei n,
                                          const GLuint* renderbuffers) {
  gles2::GetGLContext()->DeleteRenderbuffers(n, renderbuffers);
}
void GL_APIENTRY GLES2DeleteSamplers(GLsizei n, const GLuint* samplers) {
  gles2::GetGLContext()->DeleteSamplers(n, samplers);
}
void GL_APIENTRY GLES2DeleteSync(GLsync sync) {
  gles2::GetGLContext()->DeleteSync(sync);
}
void GL_APIENTRY GLES2DeleteShader(GLuint shader) {
  gles2::GetGLContext()->DeleteShader(shader);
}
void GL_APIENTRY GLES2DeleteTextures(GLsizei n, const GLuint* textures) {
  gles2::GetGLContext()->DeleteTextures(n, textures);
}
void GL_APIENTRY GLES2DeleteTransformFeedbacks(GLsizei n, const GLuint* ids) {
  gles2::GetGLContext()->DeleteTransformFeedbacks(n, ids);
}
void GL_APIENTRY GLES2DepthFunc(GLenum func) {
  gles2::GetGLContext()->DepthFunc(func);
}
void GL_APIENTRY GLES2DepthMask(GLboolean flag) {
  gles2::GetGLContext()->DepthMask(flag);
}
void GL_APIENTRY GLES2DepthRangef(GLclampf zNear, GLclampf zFar) {
  gles2::GetGLContext()->DepthRangef(zNear, zFar);
}
void GL_APIENTRY GLES2DetachShader(GLuint program, GLuint shader) {
  gles2::GetGLContext()->DetachShader(program, shader);
}
void GL_APIENTRY GLES2Disable(GLenum cap) {
  gles2::GetGLContext()->Disable(cap);
}
void GL_APIENTRY GLES2DisableVertexAttribArray(GLuint index) {
  gles2::GetGLContext()->DisableVertexAttribArray(index);
}
void GL_APIENTRY GLES2DrawArrays(GLenum mode, GLint first, GLsizei count) {
  gles2::GetGLContext()->DrawArrays(mode, first, count);
}
void GL_APIENTRY GLES2DrawElements(GLenum mode,
                                   GLsizei count,
                                   GLenum type,
                                   const void* indices) {
  gles2::GetGLContext()->DrawElements(mode, count, type, indices);
}
void GL_APIENTRY GLES2DrawRangeElements(GLenum mode,
                                        GLuint start,
                                        GLuint end,
                                        GLsizei count,
                                        GLenum type,
                                        const void* indices) {
  gles2::GetGLContext()->DrawRangeElements(mode, start, end, count, type,
                                           indices);
}
void GL_APIENTRY GLES2Enable(GLenum cap) {
  gles2::GetGLContext()->Enable(cap);
}
void GL_APIENTRY GLES2EnableVertexAttribArray(GLuint index) {
  gles2::GetGLContext()->EnableVertexAttribArray(index);
}
GLsync GL_APIENTRY GLES2FenceSync(GLenum condition, GLbitfield flags) {
  return gles2::GetGLContext()->FenceSync(condition, flags);
}
void GL_APIENTRY GLES2Finish() {
  gles2::GetGLContext()->Finish();
}
void GL_APIENTRY GLES2Flush() {
  gles2::GetGLContext()->Flush();
}
void GL_APIENTRY GLES2FramebufferRenderbuffer(GLenum target,
                                              GLenum attachment,
                                              GLenum renderbuffertarget,
                                              GLuint renderbuffer) {
  gles2::GetGLContext()->FramebufferRenderbuffer(
      target, attachment, renderbuffertarget, renderbuffer);
}
void GL_APIENTRY GLES2FramebufferTexture2D(GLenum target,
                                           GLenum attachment,
                                           GLenum textarget,
                                           GLuint texture,
                                           GLint level) {
  gles2::GetGLContext()->FramebufferTexture2D(target, attachment, textarget,
                                              texture, level);
}
void GL_APIENTRY GLES2FramebufferTextureLayer(GLenum target,
                                              GLenum attachment,
                                              GLuint texture,
                                              GLint level,
                                              GLint layer) {
  gles2::GetGLContext()->FramebufferTextureLayer(target, attachment, texture,
                                                 level, layer);
}
void GL_APIENTRY GLES2FrontFace(GLenum mode) {
  gles2::GetGLContext()->FrontFace(mode);
}
void GL_APIENTRY GLES2GenBuffers(GLsizei n, GLuint* buffers) {
  gles2::GetGLContext()->GenBuffers(n, buffers);
}
void GL_APIENTRY GLES2GenerateMipmap(GLenum target) {
  gles2::GetGLContext()->GenerateMipmap(target);
}
void GL_APIENTRY GLES2GenFramebuffers(GLsizei n, GLuint* framebuffers) {
  gles2::GetGLContext()->GenFramebuffers(n, framebuffers);
}
void GL_APIENTRY GLES2GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  gles2::GetGLContext()->GenRenderbuffers(n, renderbuffers);
}
void GL_APIENTRY GLES2GenSamplers(GLsizei n, GLuint* samplers) {
  gles2::GetGLContext()->GenSamplers(n, samplers);
}
void GL_APIENTRY GLES2GenTextures(GLsizei n, GLuint* textures) {
  gles2::GetGLContext()->GenTextures(n, textures);
}
void GL_APIENTRY GLES2GenTransformFeedbacks(GLsizei n, GLuint* ids) {
  gles2::GetGLContext()->GenTransformFeedbacks(n, ids);
}
void GL_APIENTRY GLES2GetActiveAttrib(GLuint program,
                                      GLuint index,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      GLint* size,
                                      GLenum* type,
                                      char* name) {
  gles2::GetGLContext()->GetActiveAttrib(program, index, bufsize, length, size,
                                         type, name);
}
void GL_APIENTRY GLES2GetActiveUniform(GLuint program,
                                       GLuint index,
                                       GLsizei bufsize,
                                       GLsizei* length,
                                       GLint* size,
                                       GLenum* type,
                                       char* name) {
  gles2::GetGLContext()->GetActiveUniform(program, index, bufsize, length, size,
                                          type, name);
}
void GL_APIENTRY GLES2GetActiveUniformBlockiv(GLuint program,
                                              GLuint index,
                                              GLenum pname,
                                              GLint* params) {
  gles2::GetGLContext()->GetActiveUniformBlockiv(program, index, pname, params);
}
void GL_APIENTRY GLES2GetActiveUniformBlockName(GLuint program,
                                                GLuint index,
                                                GLsizei bufsize,
                                                GLsizei* length,
                                                char* name) {
  gles2::GetGLContext()->GetActiveUniformBlockName(program, index, bufsize,
                                                   length, name);
}
void GL_APIENTRY GLES2GetActiveUniformsiv(GLuint program,
                                          GLsizei count,
                                          const GLuint* indices,
                                          GLenum pname,
                                          GLint* params) {
  gles2::GetGLContext()->GetActiveUniformsiv(program, count, indices, pname,
                                             params);
}
void GL_APIENTRY GLES2GetAttachedShaders(GLuint program,
                                         GLsizei maxcount,
                                         GLsizei* count,
                                         GLuint* shaders) {
  gles2::GetGLContext()->GetAttachedShaders(program, maxcount, count, shaders);
}
GLint GL_APIENTRY GLES2GetAttribLocation(GLuint program, const char* name) {
  return gles2::GetGLContext()->GetAttribLocation(program, name);
}
void GL_APIENTRY GLES2GetBooleanv(GLenum pname, GLboolean* params) {
  gles2::GetGLContext()->GetBooleanv(pname, params);
}
void GL_APIENTRY GLES2GetBooleani_v(GLenum pname,
                                    GLuint index,
                                    GLboolean* data) {
  gles2::GetGLContext()->GetBooleani_v(pname, index, data);
}
void GL_APIENTRY GLES2GetBufferParameteri64v(GLenum target,
                                             GLenum pname,
                                             GLint64* params) {
  gles2::GetGLContext()->GetBufferParameteri64v(target, pname, params);
}
void GL_APIENTRY GLES2GetBufferParameteriv(GLenum target,
                                           GLenum pname,
                                           GLint* params) {
  gles2::GetGLContext()->GetBufferParameteriv(target, pname, params);
}
GLenum GL_APIENTRY GLES2GetError() {
  return gles2::GetGLContext()->GetError();
}
void GL_APIENTRY GLES2GetFloatv(GLenum pname, GLfloat* params) {
  gles2::GetGLContext()->GetFloatv(pname, params);
}
GLint GL_APIENTRY GLES2GetFragDataLocation(GLuint program, const char* name) {
  return gles2::GetGLContext()->GetFragDataLocation(program, name);
}
void GL_APIENTRY GLES2GetFramebufferAttachmentParameteriv(GLenum target,
                                                          GLenum attachment,
                                                          GLenum pname,
                                                          GLint* params) {
  gles2::GetGLContext()->GetFramebufferAttachmentParameteriv(target, attachment,
                                                             pname, params);
}
void GL_APIENTRY GLES2GetInteger64v(GLenum pname, GLint64* params) {
  gles2::GetGLContext()->GetInteger64v(pname, params);
}
void GL_APIENTRY GLES2GetIntegeri_v(GLenum pname, GLuint index, GLint* data) {
  gles2::GetGLContext()->GetIntegeri_v(pname, index, data);
}
void GL_APIENTRY GLES2GetInteger64i_v(GLenum pname,
                                      GLuint index,
                                      GLint64* data) {
  gles2::GetGLContext()->GetInteger64i_v(pname, index, data);
}
void GL_APIENTRY GLES2GetIntegerv(GLenum pname, GLint* params) {
  gles2::GetGLContext()->GetIntegerv(pname, params);
}
void GL_APIENTRY GLES2GetInternalformativ(GLenum target,
                                          GLenum format,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLint* params) {
  gles2::GetGLContext()->GetInternalformativ(target, format, pname, bufSize,
                                             params);
}
void GL_APIENTRY GLES2GetProgramiv(GLuint program,
                                   GLenum pname,
                                   GLint* params) {
  gles2::GetGLContext()->GetProgramiv(program, pname, params);
}
void GL_APIENTRY GLES2GetProgramInfoLog(GLuint program,
                                        GLsizei bufsize,
                                        GLsizei* length,
                                        char* infolog) {
  gles2::GetGLContext()->GetProgramInfoLog(program, bufsize, length, infolog);
}
void GL_APIENTRY GLES2GetRenderbufferParameteriv(GLenum target,
                                                 GLenum pname,
                                                 GLint* params) {
  gles2::GetGLContext()->GetRenderbufferParameteriv(target, pname, params);
}
void GL_APIENTRY GLES2GetSamplerParameterfv(GLuint sampler,
                                            GLenum pname,
                                            GLfloat* params) {
  gles2::GetGLContext()->GetSamplerParameterfv(sampler, pname, params);
}
void GL_APIENTRY GLES2GetSamplerParameteriv(GLuint sampler,
                                            GLenum pname,
                                            GLint* params) {
  gles2::GetGLContext()->GetSamplerParameteriv(sampler, pname, params);
}
void GL_APIENTRY GLES2GetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  gles2::GetGLContext()->GetShaderiv(shader, pname, params);
}
void GL_APIENTRY GLES2GetShaderInfoLog(GLuint shader,
                                       GLsizei bufsize,
                                       GLsizei* length,
                                       char* infolog) {
  gles2::GetGLContext()->GetShaderInfoLog(shader, bufsize, length, infolog);
}
void GL_APIENTRY GLES2GetShaderPrecisionFormat(GLenum shadertype,
                                               GLenum precisiontype,
                                               GLint* range,
                                               GLint* precision) {
  gles2::GetGLContext()->GetShaderPrecisionFormat(shadertype, precisiontype,
                                                  range, precision);
}
void GL_APIENTRY GLES2GetShaderSource(GLuint shader,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* source) {
  gles2::GetGLContext()->GetShaderSource(shader, bufsize, length, source);
}
const GLubyte* GL_APIENTRY GLES2GetString(GLenum name) {
  return gles2::GetGLContext()->GetString(name);
}
const GLubyte* GL_APIENTRY GLES2GetStringi(GLenum name, GLuint index) {
  return gles2::GetGLContext()->GetStringi(name, index);
}
void GL_APIENTRY GLES2GetSynciv(GLsync sync,
                                GLenum pname,
                                GLsizei bufsize,
                                GLsizei* length,
                                GLint* values) {
  gles2::GetGLContext()->GetSynciv(sync, pname, bufsize, length, values);
}
void GL_APIENTRY GLES2GetTexParameterfv(GLenum target,
                                        GLenum pname,
                                        GLfloat* params) {
  gles2::GetGLContext()->GetTexParameterfv(target, pname, params);
}
void GL_APIENTRY GLES2GetTexParameteriv(GLenum target,
                                        GLenum pname,
                                        GLint* params) {
  gles2::GetGLContext()->GetTexParameteriv(target, pname, params);
}
void GL_APIENTRY GLES2GetTransformFeedbackVarying(GLuint program,
                                                  GLuint index,
                                                  GLsizei bufsize,
                                                  GLsizei* length,
                                                  GLsizei* size,
                                                  GLenum* type,
                                                  char* name) {
  gles2::GetGLContext()->GetTransformFeedbackVarying(program, index, bufsize,
                                                     length, size, type, name);
}
GLuint GL_APIENTRY GLES2GetUniformBlockIndex(GLuint program, const char* name) {
  return gles2::GetGLContext()->GetUniformBlockIndex(program, name);
}
void GL_APIENTRY GLES2GetUniformfv(GLuint program,
                                   GLint location,
                                   GLfloat* params) {
  gles2::GetGLContext()->GetUniformfv(program, location, params);
}
void GL_APIENTRY GLES2GetUniformiv(GLuint program,
                                   GLint location,
                                   GLint* params) {
  gles2::GetGLContext()->GetUniformiv(program, location, params);
}
void GL_APIENTRY GLES2GetUniformuiv(GLuint program,
                                    GLint location,
                                    GLuint* params) {
  gles2::GetGLContext()->GetUniformuiv(program, location, params);
}
void GL_APIENTRY GLES2GetUniformIndices(GLuint program,
                                        GLsizei count,
                                        const char* const* names,
                                        GLuint* indices) {
  gles2::GetGLContext()->GetUniformIndices(program, count, names, indices);
}
GLint GL_APIENTRY GLES2GetUniformLocation(GLuint program, const char* name) {
  return gles2::GetGLContext()->GetUniformLocation(program, name);
}
void GL_APIENTRY GLES2GetVertexAttribfv(GLuint index,
                                        GLenum pname,
                                        GLfloat* params) {
  gles2::GetGLContext()->GetVertexAttribfv(index, pname, params);
}
void GL_APIENTRY GLES2GetVertexAttribiv(GLuint index,
                                        GLenum pname,
                                        GLint* params) {
  gles2::GetGLContext()->GetVertexAttribiv(index, pname, params);
}
void GL_APIENTRY GLES2GetVertexAttribIiv(GLuint index,
                                         GLenum pname,
                                         GLint* params) {
  gles2::GetGLContext()->GetVertexAttribIiv(index, pname, params);
}
void GL_APIENTRY GLES2GetVertexAttribIuiv(GLuint index,
                                          GLenum pname,
                                          GLuint* params) {
  gles2::GetGLContext()->GetVertexAttribIuiv(index, pname, params);
}
void GL_APIENTRY GLES2GetVertexAttribPointerv(GLuint index,
                                              GLenum pname,
                                              void** pointer) {
  gles2::GetGLContext()->GetVertexAttribPointerv(index, pname, pointer);
}
void GL_APIENTRY GLES2Hint(GLenum target, GLenum mode) {
  gles2::GetGLContext()->Hint(target, mode);
}
void GL_APIENTRY GLES2InvalidateFramebuffer(GLenum target,
                                            GLsizei count,
                                            const GLenum* attachments) {
  gles2::GetGLContext()->InvalidateFramebuffer(target, count, attachments);
}
void GL_APIENTRY GLES2InvalidateSubFramebuffer(GLenum target,
                                               GLsizei count,
                                               const GLenum* attachments,
                                               GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height) {
  gles2::GetGLContext()->InvalidateSubFramebuffer(target, count, attachments, x,
                                                  y, width, height);
}
GLboolean GL_APIENTRY GLES2IsBuffer(GLuint buffer) {
  return gles2::GetGLContext()->IsBuffer(buffer);
}
GLboolean GL_APIENTRY GLES2IsEnabled(GLenum cap) {
  return gles2::GetGLContext()->IsEnabled(cap);
}
GLboolean GL_APIENTRY GLES2IsFramebuffer(GLuint framebuffer) {
  return gles2::GetGLContext()->IsFramebuffer(framebuffer);
}
GLboolean GL_APIENTRY GLES2IsProgram(GLuint program) {
  return gles2::GetGLContext()->IsProgram(program);
}
GLboolean GL_APIENTRY GLES2IsRenderbuffer(GLuint renderbuffer) {
  return gles2::GetGLContext()->IsRenderbuffer(renderbuffer);
}
GLboolean GL_APIENTRY GLES2IsSampler(GLuint sampler) {
  return gles2::GetGLContext()->IsSampler(sampler);
}
GLboolean GL_APIENTRY GLES2IsShader(GLuint shader) {
  return gles2::GetGLContext()->IsShader(shader);
}
GLboolean GL_APIENTRY GLES2IsSync(GLsync sync) {
  return gles2::GetGLContext()->IsSync(sync);
}
GLboolean GL_APIENTRY GLES2IsTexture(GLuint texture) {
  return gles2::GetGLContext()->IsTexture(texture);
}
GLboolean GL_APIENTRY GLES2IsTransformFeedback(GLuint transformfeedback) {
  return gles2::GetGLContext()->IsTransformFeedback(transformfeedback);
}
void GL_APIENTRY GLES2LineWidth(GLfloat width) {
  gles2::GetGLContext()->LineWidth(width);
}
void GL_APIENTRY GLES2LinkProgram(GLuint program) {
  gles2::GetGLContext()->LinkProgram(program);
}
void GL_APIENTRY GLES2PauseTransformFeedback() {
  gles2::GetGLContext()->PauseTransformFeedback();
}
void GL_APIENTRY GLES2PixelStorei(GLenum pname, GLint param) {
  gles2::GetGLContext()->PixelStorei(pname, param);
}
void GL_APIENTRY GLES2PolygonOffset(GLfloat factor, GLfloat units) {
  gles2::GetGLContext()->PolygonOffset(factor, units);
}
void GL_APIENTRY GLES2ReadBuffer(GLenum src) {
  gles2::GetGLContext()->ReadBuffer(src);
}
void GL_APIENTRY GLES2ReadPixels(GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLenum type,
                                 void* pixels) {
  gles2::GetGLContext()->ReadPixels(x, y, width, height, format, type, pixels);
}
void GL_APIENTRY GLES2ReleaseShaderCompiler() {
  gles2::GetGLContext()->ReleaseShaderCompiler();
}
void GL_APIENTRY GLES2RenderbufferStorage(GLenum target,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height) {
  gles2::GetGLContext()->RenderbufferStorage(target, internalformat, width,
                                             height);
}
void GL_APIENTRY GLES2ResumeTransformFeedback() {
  gles2::GetGLContext()->ResumeTransformFeedback();
}
void GL_APIENTRY GLES2SampleCoverage(GLclampf value, GLboolean invert) {
  gles2::GetGLContext()->SampleCoverage(value, invert);
}
void GL_APIENTRY GLES2SamplerParameterf(GLuint sampler,
                                        GLenum pname,
                                        GLfloat param) {
  gles2::GetGLContext()->SamplerParameterf(sampler, pname, param);
}
void GL_APIENTRY GLES2SamplerParameterfv(GLuint sampler,
                                         GLenum pname,
                                         const GLfloat* params) {
  gles2::GetGLContext()->SamplerParameterfv(sampler, pname, params);
}
void GL_APIENTRY GLES2SamplerParameteri(GLuint sampler,
                                        GLenum pname,
                                        GLint param) {
  gles2::GetGLContext()->SamplerParameteri(sampler, pname, param);
}
void GL_APIENTRY GLES2SamplerParameteriv(GLuint sampler,
                                         GLenum pname,
                                         const GLint* params) {
  gles2::GetGLContext()->SamplerParameteriv(sampler, pname, params);
}
void GL_APIENTRY GLES2Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  gles2::GetGLContext()->Scissor(x, y, width, height);
}
void GL_APIENTRY GLES2ShaderBinary(GLsizei n,
                                   const GLuint* shaders,
                                   GLenum binaryformat,
                                   const void* binary,
                                   GLsizei length) {
  gles2::GetGLContext()->ShaderBinary(n, shaders, binaryformat, binary, length);
}
void GL_APIENTRY GLES2ShaderSource(GLuint shader,
                                   GLsizei count,
                                   const GLchar* const* str,
                                   const GLint* length) {
  gles2::GetGLContext()->ShaderSource(shader, count, str, length);
}
void GL_APIENTRY GLES2ShallowFinishCHROMIUM() {
  gles2::GetGLContext()->ShallowFinishCHROMIUM();
}
void GL_APIENTRY GLES2OrderingBarrierCHROMIUM() {
  gles2::GetGLContext()->OrderingBarrierCHROMIUM();
}
void GL_APIENTRY GLES2MultiDrawArraysWEBGL(GLenum mode,
                                           const GLint* firsts,
                                           const GLsizei* counts,
                                           GLsizei drawcount) {
  gles2::GetGLContext()->MultiDrawArraysWEBGL(mode, firsts, counts, drawcount);
}
void GL_APIENTRY
GLES2MultiDrawArraysInstancedWEBGL(GLenum mode,
                                   const GLint* firsts,
                                   const GLsizei* counts,
                                   const GLsizei* instance_counts,
                                   GLsizei drawcount) {
  gles2::GetGLContext()->MultiDrawArraysInstancedWEBGL(
      mode, firsts, counts, instance_counts, drawcount);
}
void GL_APIENTRY
GLES2MultiDrawArraysInstancedBaseInstanceWEBGL(GLenum mode,
                                               const GLint* firsts,
                                               const GLsizei* counts,
                                               const GLsizei* instance_counts,
                                               const GLuint* baseinstances,
                                               GLsizei drawcount) {
  gles2::GetGLContext()->MultiDrawArraysInstancedBaseInstanceWEBGL(
      mode, firsts, counts, instance_counts, baseinstances, drawcount);
}
void GL_APIENTRY GLES2MultiDrawElementsWEBGL(GLenum mode,
                                             const GLsizei* counts,
                                             GLenum type,
                                             const GLsizei* offsets,
                                             GLsizei drawcount) {
  gles2::GetGLContext()->MultiDrawElementsWEBGL(mode, counts, type, offsets,
                                                drawcount);
}
void GL_APIENTRY
GLES2MultiDrawElementsInstancedWEBGL(GLenum mode,
                                     const GLsizei* counts,
                                     GLenum type,
                                     const GLsizei* offsets,
                                     const GLsizei* instance_counts,
                                     GLsizei drawcount) {
  gles2::GetGLContext()->MultiDrawElementsInstancedWEBGL(
      mode, counts, type, offsets, instance_counts, drawcount);
}
void GL_APIENTRY GLES2MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLsizei* offsets,
    const GLsizei* instance_counts,
    const GLint* basevertices,
    const GLuint* baseinstances,
    GLsizei drawcount) {
  gles2::GetGLContext()->MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
      mode, counts, type, offsets, instance_counts, basevertices, baseinstances,
      drawcount);
}
void GL_APIENTRY GLES2StencilFunc(GLenum func, GLint ref, GLuint mask) {
  gles2::GetGLContext()->StencilFunc(func, ref, mask);
}
void GL_APIENTRY GLES2StencilFuncSeparate(GLenum face,
                                          GLenum func,
                                          GLint ref,
                                          GLuint mask) {
  gles2::GetGLContext()->StencilFuncSeparate(face, func, ref, mask);
}
void GL_APIENTRY GLES2StencilMask(GLuint mask) {
  gles2::GetGLContext()->StencilMask(mask);
}
void GL_APIENTRY GLES2StencilMaskSeparate(GLenum face, GLuint mask) {
  gles2::GetGLContext()->StencilMaskSeparate(face, mask);
}
void GL_APIENTRY GLES2StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  gles2::GetGLContext()->StencilOp(fail, zfail, zpass);
}
void GL_APIENTRY GLES2StencilOpSeparate(GLenum face,
                                        GLenum fail,
                                        GLenum zfail,
                                        GLenum zpass) {
  gles2::GetGLContext()->StencilOpSeparate(face, fail, zfail, zpass);
}
void GL_APIENTRY GLES2TexImage2D(GLenum target,
                                 GLint level,
                                 GLint internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLint border,
                                 GLenum format,
                                 GLenum type,
                                 const void* pixels) {
  gles2::GetGLContext()->TexImage2D(target, level, internalformat, width,
                                    height, border, format, type, pixels);
}
void GL_APIENTRY GLES2TexImage3D(GLenum target,
                                 GLint level,
                                 GLint internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLint border,
                                 GLenum format,
                                 GLenum type,
                                 const void* pixels) {
  gles2::GetGLContext()->TexImage3D(target, level, internalformat, width,
                                    height, depth, border, format, type,
                                    pixels);
}
void GL_APIENTRY GLES2TexParameterf(GLenum target,
                                    GLenum pname,
                                    GLfloat param) {
  gles2::GetGLContext()->TexParameterf(target, pname, param);
}
void GL_APIENTRY GLES2TexParameterfv(GLenum target,
                                     GLenum pname,
                                     const GLfloat* params) {
  gles2::GetGLContext()->TexParameterfv(target, pname, params);
}
void GL_APIENTRY GLES2TexParameteri(GLenum target, GLenum pname, GLint param) {
  gles2::GetGLContext()->TexParameteri(target, pname, param);
}
void GL_APIENTRY GLES2TexParameteriv(GLenum target,
                                     GLenum pname,
                                     const GLint* params) {
  gles2::GetGLContext()->TexParameteriv(target, pname, params);
}
void GL_APIENTRY GLES2TexStorage3D(GLenum target,
                                   GLsizei levels,
                                   GLenum internalFormat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth) {
  gles2::GetGLContext()->TexStorage3D(target, levels, internalFormat, width,
                                      height, depth);
}
void GL_APIENTRY GLES2TexSubImage2D(GLenum target,
                                    GLint level,
                                    GLint xoffset,
                                    GLint yoffset,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    const void* pixels) {
  gles2::GetGLContext()->TexSubImage2D(target, level, xoffset, yoffset, width,
                                       height, format, type, pixels);
}
void GL_APIENTRY GLES2TexSubImage3D(GLenum target,
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
  gles2::GetGLContext()->TexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                       width, height, depth, format, type,
                                       pixels);
}
void GL_APIENTRY GLES2TransformFeedbackVaryings(GLuint program,
                                                GLsizei count,
                                                const char* const* varyings,
                                                GLenum buffermode) {
  gles2::GetGLContext()->TransformFeedbackVaryings(program, count, varyings,
                                                   buffermode);
}
void GL_APIENTRY GLES2Uniform1f(GLint location, GLfloat x) {
  gles2::GetGLContext()->Uniform1f(location, x);
}
void GL_APIENTRY GLES2Uniform1fv(GLint location,
                                 GLsizei count,
                                 const GLfloat* v) {
  gles2::GetGLContext()->Uniform1fv(location, count, v);
}
void GL_APIENTRY GLES2Uniform1i(GLint location, GLint x) {
  gles2::GetGLContext()->Uniform1i(location, x);
}
void GL_APIENTRY GLES2Uniform1iv(GLint location,
                                 GLsizei count,
                                 const GLint* v) {
  gles2::GetGLContext()->Uniform1iv(location, count, v);
}
void GL_APIENTRY GLES2Uniform1ui(GLint location, GLuint x) {
  gles2::GetGLContext()->Uniform1ui(location, x);
}
void GL_APIENTRY GLES2Uniform1uiv(GLint location,
                                  GLsizei count,
                                  const GLuint* v) {
  gles2::GetGLContext()->Uniform1uiv(location, count, v);
}
void GL_APIENTRY GLES2Uniform2f(GLint location, GLfloat x, GLfloat y) {
  gles2::GetGLContext()->Uniform2f(location, x, y);
}
void GL_APIENTRY GLES2Uniform2fv(GLint location,
                                 GLsizei count,
                                 const GLfloat* v) {
  gles2::GetGLContext()->Uniform2fv(location, count, v);
}
void GL_APIENTRY GLES2Uniform2i(GLint location, GLint x, GLint y) {
  gles2::GetGLContext()->Uniform2i(location, x, y);
}
void GL_APIENTRY GLES2Uniform2iv(GLint location,
                                 GLsizei count,
                                 const GLint* v) {
  gles2::GetGLContext()->Uniform2iv(location, count, v);
}
void GL_APIENTRY GLES2Uniform2ui(GLint location, GLuint x, GLuint y) {
  gles2::GetGLContext()->Uniform2ui(location, x, y);
}
void GL_APIENTRY GLES2Uniform2uiv(GLint location,
                                  GLsizei count,
                                  const GLuint* v) {
  gles2::GetGLContext()->Uniform2uiv(location, count, v);
}
void GL_APIENTRY GLES2Uniform3f(GLint location,
                                GLfloat x,
                                GLfloat y,
                                GLfloat z) {
  gles2::GetGLContext()->Uniform3f(location, x, y, z);
}
void GL_APIENTRY GLES2Uniform3fv(GLint location,
                                 GLsizei count,
                                 const GLfloat* v) {
  gles2::GetGLContext()->Uniform3fv(location, count, v);
}
void GL_APIENTRY GLES2Uniform3i(GLint location, GLint x, GLint y, GLint z) {
  gles2::GetGLContext()->Uniform3i(location, x, y, z);
}
void GL_APIENTRY GLES2Uniform3iv(GLint location,
                                 GLsizei count,
                                 const GLint* v) {
  gles2::GetGLContext()->Uniform3iv(location, count, v);
}
void GL_APIENTRY GLES2Uniform3ui(GLint location, GLuint x, GLuint y, GLuint z) {
  gles2::GetGLContext()->Uniform3ui(location, x, y, z);
}
void GL_APIENTRY GLES2Uniform3uiv(GLint location,
                                  GLsizei count,
                                  const GLuint* v) {
  gles2::GetGLContext()->Uniform3uiv(location, count, v);
}
void GL_APIENTRY
GLES2Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  gles2::GetGLContext()->Uniform4f(location, x, y, z, w);
}
void GL_APIENTRY GLES2Uniform4fv(GLint location,
                                 GLsizei count,
                                 const GLfloat* v) {
  gles2::GetGLContext()->Uniform4fv(location, count, v);
}
void GL_APIENTRY
GLES2Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  gles2::GetGLContext()->Uniform4i(location, x, y, z, w);
}
void GL_APIENTRY GLES2Uniform4iv(GLint location,
                                 GLsizei count,
                                 const GLint* v) {
  gles2::GetGLContext()->Uniform4iv(location, count, v);
}
void GL_APIENTRY
GLES2Uniform4ui(GLint location, GLuint x, GLuint y, GLuint z, GLuint w) {
  gles2::GetGLContext()->Uniform4ui(location, x, y, z, w);
}
void GL_APIENTRY GLES2Uniform4uiv(GLint location,
                                  GLsizei count,
                                  const GLuint* v) {
  gles2::GetGLContext()->Uniform4uiv(location, count, v);
}
void GL_APIENTRY GLES2UniformBlockBinding(GLuint program,
                                          GLuint index,
                                          GLuint binding) {
  gles2::GetGLContext()->UniformBlockBinding(program, index, binding);
}
void GL_APIENTRY GLES2UniformMatrix2fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  gles2::GetGLContext()->UniformMatrix2fv(location, count, transpose, value);
}
void GL_APIENTRY GLES2UniformMatrix2x3fv(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat* value) {
  gles2::GetGLContext()->UniformMatrix2x3fv(location, count, transpose, value);
}
void GL_APIENTRY GLES2UniformMatrix2x4fv(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat* value) {
  gles2::GetGLContext()->UniformMatrix2x4fv(location, count, transpose, value);
}
void GL_APIENTRY GLES2UniformMatrix3fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  gles2::GetGLContext()->UniformMatrix3fv(location, count, transpose, value);
}
void GL_APIENTRY GLES2UniformMatrix3x2fv(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat* value) {
  gles2::GetGLContext()->UniformMatrix3x2fv(location, count, transpose, value);
}
void GL_APIENTRY GLES2UniformMatrix3x4fv(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat* value) {
  gles2::GetGLContext()->UniformMatrix3x4fv(location, count, transpose, value);
}
void GL_APIENTRY GLES2UniformMatrix4fv(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  gles2::GetGLContext()->UniformMatrix4fv(location, count, transpose, value);
}
void GL_APIENTRY GLES2UniformMatrix4x2fv(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat* value) {
  gles2::GetGLContext()->UniformMatrix4x2fv(location, count, transpose, value);
}
void GL_APIENTRY GLES2UniformMatrix4x3fv(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const GLfloat* value) {
  gles2::GetGLContext()->UniformMatrix4x3fv(location, count, transpose, value);
}
void GL_APIENTRY GLES2UseProgram(GLuint program) {
  gles2::GetGLContext()->UseProgram(program);
}
void GL_APIENTRY GLES2ValidateProgram(GLuint program) {
  gles2::GetGLContext()->ValidateProgram(program);
}
void GL_APIENTRY GLES2VertexAttrib1f(GLuint indx, GLfloat x) {
  gles2::GetGLContext()->VertexAttrib1f(indx, x);
}
void GL_APIENTRY GLES2VertexAttrib1fv(GLuint indx, const GLfloat* values) {
  gles2::GetGLContext()->VertexAttrib1fv(indx, values);
}
void GL_APIENTRY GLES2VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  gles2::GetGLContext()->VertexAttrib2f(indx, x, y);
}
void GL_APIENTRY GLES2VertexAttrib2fv(GLuint indx, const GLfloat* values) {
  gles2::GetGLContext()->VertexAttrib2fv(indx, values);
}
void GL_APIENTRY GLES2VertexAttrib3f(GLuint indx,
                                     GLfloat x,
                                     GLfloat y,
                                     GLfloat z) {
  gles2::GetGLContext()->VertexAttrib3f(indx, x, y, z);
}
void GL_APIENTRY GLES2VertexAttrib3fv(GLuint indx, const GLfloat* values) {
  gles2::GetGLContext()->VertexAttrib3fv(indx, values);
}
void GL_APIENTRY
GLES2VertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  gles2::GetGLContext()->VertexAttrib4f(indx, x, y, z, w);
}
void GL_APIENTRY GLES2VertexAttrib4fv(GLuint indx, const GLfloat* values) {
  gles2::GetGLContext()->VertexAttrib4fv(indx, values);
}
void GL_APIENTRY
GLES2VertexAttribI4i(GLuint indx, GLint x, GLint y, GLint z, GLint w) {
  gles2::GetGLContext()->VertexAttribI4i(indx, x, y, z, w);
}
void GL_APIENTRY GLES2VertexAttribI4iv(GLuint indx, const GLint* values) {
  gles2::GetGLContext()->VertexAttribI4iv(indx, values);
}
void GL_APIENTRY
GLES2VertexAttribI4ui(GLuint indx, GLuint x, GLuint y, GLuint z, GLuint w) {
  gles2::GetGLContext()->VertexAttribI4ui(indx, x, y, z, w);
}
void GL_APIENTRY GLES2VertexAttribI4uiv(GLuint indx, const GLuint* values) {
  gles2::GetGLContext()->VertexAttribI4uiv(indx, values);
}
void GL_APIENTRY GLES2VertexAttribIPointer(GLuint indx,
                                           GLint size,
                                           GLenum type,
                                           GLsizei stride,
                                           const void* ptr) {
  gles2::GetGLContext()->VertexAttribIPointer(indx, size, type, stride, ptr);
}
void GL_APIENTRY GLES2VertexAttribPointer(GLuint indx,
                                          GLint size,
                                          GLenum type,
                                          GLboolean normalized,
                                          GLsizei stride,
                                          const void* ptr) {
  gles2::GetGLContext()->VertexAttribPointer(indx, size, type, normalized,
                                             stride, ptr);
}
void GL_APIENTRY GLES2Viewport(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height) {
  gles2::GetGLContext()->Viewport(x, y, width, height);
}
void GL_APIENTRY GLES2WaitSync(GLsync sync,
                               GLbitfield flags,
                               GLuint64 timeout) {
  gles2::GetGLContext()->WaitSync(sync, flags, timeout);
}
void GL_APIENTRY GLES2BlitFramebufferCHROMIUM(GLint srcX0,
                                              GLint srcY0,
                                              GLint srcX1,
                                              GLint srcY1,
                                              GLint dstX0,
                                              GLint dstY0,
                                              GLint dstX1,
                                              GLint dstY1,
                                              GLbitfield mask,
                                              GLenum filter) {
  gles2::GetGLContext()->BlitFramebufferCHROMIUM(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
void GL_APIENTRY
GLES2RenderbufferStorageMultisampleCHROMIUM(GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  gles2::GetGLContext()->RenderbufferStorageMultisampleCHROMIUM(
      target, samples, internalformat, width, height);
}
void GL_APIENTRY
GLES2RenderbufferStorageMultisampleAdvancedAMD(GLenum target,
                                               GLsizei samples,
                                               GLsizei storageSamples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height) {
  gles2::GetGLContext()->RenderbufferStorageMultisampleAdvancedAMD(
      target, samples, storageSamples, internalformat, width, height);
}
void GL_APIENTRY GLES2RenderbufferStorageMultisampleEXT(GLenum target,
                                                        GLsizei samples,
                                                        GLenum internalformat,
                                                        GLsizei width,
                                                        GLsizei height) {
  gles2::GetGLContext()->RenderbufferStorageMultisampleEXT(
      target, samples, internalformat, width, height);
}
void GL_APIENTRY GLES2FramebufferTexture2DMultisampleEXT(GLenum target,
                                                         GLenum attachment,
                                                         GLenum textarget,
                                                         GLuint texture,
                                                         GLint level,
                                                         GLsizei samples) {
  gles2::GetGLContext()->FramebufferTexture2DMultisampleEXT(
      target, attachment, textarget, texture, level, samples);
}
void GL_APIENTRY GLES2TexStorage2DEXT(GLenum target,
                                      GLsizei levels,
                                      GLenum internalFormat,
                                      GLsizei width,
                                      GLsizei height) {
  gles2::GetGLContext()->TexStorage2DEXT(target, levels, internalFormat, width,
                                         height);
}
void GL_APIENTRY GLES2GenQueriesEXT(GLsizei n, GLuint* queries) {
  gles2::GetGLContext()->GenQueriesEXT(n, queries);
}
void GL_APIENTRY GLES2DeleteQueriesEXT(GLsizei n, const GLuint* queries) {
  gles2::GetGLContext()->DeleteQueriesEXT(n, queries);
}
void GL_APIENTRY GLES2QueryCounterEXT(GLuint id, GLenum target) {
  gles2::GetGLContext()->QueryCounterEXT(id, target);
}
GLboolean GL_APIENTRY GLES2IsQueryEXT(GLuint id) {
  return gles2::GetGLContext()->IsQueryEXT(id);
}
void GL_APIENTRY GLES2BeginQueryEXT(GLenum target, GLuint id) {
  gles2::GetGLContext()->BeginQueryEXT(target, id);
}
void GL_APIENTRY GLES2BeginTransformFeedback(GLenum primitivemode) {
  gles2::GetGLContext()->BeginTransformFeedback(primitivemode);
}
void GL_APIENTRY GLES2EndQueryEXT(GLenum target) {
  gles2::GetGLContext()->EndQueryEXT(target);
}
void GL_APIENTRY GLES2EndTransformFeedback() {
  gles2::GetGLContext()->EndTransformFeedback();
}
void GL_APIENTRY GLES2GetQueryivEXT(GLenum target,
                                    GLenum pname,
                                    GLint* params) {
  gles2::GetGLContext()->GetQueryivEXT(target, pname, params);
}
void GL_APIENTRY GLES2GetQueryObjectivEXT(GLuint id,
                                          GLenum pname,
                                          GLint* params) {
  gles2::GetGLContext()->GetQueryObjectivEXT(id, pname, params);
}
void GL_APIENTRY GLES2GetQueryObjectuivEXT(GLuint id,
                                           GLenum pname,
                                           GLuint* params) {
  gles2::GetGLContext()->GetQueryObjectuivEXT(id, pname, params);
}
void GL_APIENTRY GLES2GetQueryObjecti64vEXT(GLuint id,
                                            GLenum pname,
                                            GLint64* params) {
  gles2::GetGLContext()->GetQueryObjecti64vEXT(id, pname, params);
}
void GL_APIENTRY GLES2GetQueryObjectui64vEXT(GLuint id,
                                             GLenum pname,
                                             GLuint64* params) {
  gles2::GetGLContext()->GetQueryObjectui64vEXT(id, pname, params);
}
void GL_APIENTRY GLES2SetDisjointValueSyncCHROMIUM() {
  gles2::GetGLContext()->SetDisjointValueSyncCHROMIUM();
}
void GL_APIENTRY GLES2InsertEventMarkerEXT(GLsizei length,
                                           const GLchar* marker) {
  gles2::GetGLContext()->InsertEventMarkerEXT(length, marker);
}
void GL_APIENTRY GLES2PushGroupMarkerEXT(GLsizei length, const GLchar* marker) {
  gles2::GetGLContext()->PushGroupMarkerEXT(length, marker);
}
void GL_APIENTRY GLES2PopGroupMarkerEXT() {
  gles2::GetGLContext()->PopGroupMarkerEXT();
}
void GL_APIENTRY GLES2GenVertexArraysOES(GLsizei n, GLuint* arrays) {
  gles2::GetGLContext()->GenVertexArraysOES(n, arrays);
}
void GL_APIENTRY GLES2DeleteVertexArraysOES(GLsizei n, const GLuint* arrays) {
  gles2::GetGLContext()->DeleteVertexArraysOES(n, arrays);
}
GLboolean GL_APIENTRY GLES2IsVertexArrayOES(GLuint array) {
  return gles2::GetGLContext()->IsVertexArrayOES(array);
}
void GL_APIENTRY GLES2BindVertexArrayOES(GLuint array) {
  gles2::GetGLContext()->BindVertexArrayOES(array);
}
void GL_APIENTRY GLES2FramebufferParameteri(GLenum target,
                                            GLenum pname,
                                            GLint param) {
  gles2::GetGLContext()->FramebufferParameteri(target, pname, param);
}
void GL_APIENTRY GLES2BindImageTexture(GLuint unit,
                                       GLuint texture,
                                       GLint level,
                                       GLboolean layered,
                                       GLint layer,
                                       GLenum access,
                                       GLenum format) {
  gles2::GetGLContext()->BindImageTexture(unit, texture, level, layered, layer,
                                          access, format);
}
void GL_APIENTRY GLES2DispatchCompute(GLuint num_groups_x,
                                      GLuint num_groups_y,
                                      GLuint num_groups_z) {
  gles2::GetGLContext()->DispatchCompute(num_groups_x, num_groups_y,
                                         num_groups_z);
}
void GL_APIENTRY GLES2DispatchComputeIndirect(GLintptr offset) {
  gles2::GetGLContext()->DispatchComputeIndirect(offset);
}
void GL_APIENTRY GLES2DrawArraysIndirect(GLenum mode, const void* offset) {
  gles2::GetGLContext()->DrawArraysIndirect(mode, offset);
}
void GL_APIENTRY GLES2DrawElementsIndirect(GLenum mode,
                                           GLenum type,
                                           const void* offset) {
  gles2::GetGLContext()->DrawElementsIndirect(mode, type, offset);
}
void GL_APIENTRY GLES2GetProgramInterfaceiv(GLuint program,
                                            GLenum program_interface,
                                            GLenum pname,
                                            GLint* params) {
  gles2::GetGLContext()->GetProgramInterfaceiv(program, program_interface,
                                               pname, params);
}
GLuint GL_APIENTRY GLES2GetProgramResourceIndex(GLuint program,
                                                GLenum program_interface,
                                                const char* name) {
  return gles2::GetGLContext()->GetProgramResourceIndex(
      program, program_interface, name);
}
void GL_APIENTRY GLES2GetProgramResourceName(GLuint program,
                                             GLenum program_interface,
                                             GLuint index,
                                             GLsizei bufsize,
                                             GLsizei* length,
                                             char* name) {
  gles2::GetGLContext()->GetProgramResourceName(program, program_interface,
                                                index, bufsize, length, name);
}
void GL_APIENTRY GLES2GetProgramResourceiv(GLuint program,
                                           GLenum program_interface,
                                           GLuint index,
                                           GLsizei prop_count,
                                           const GLenum* props,
                                           GLsizei bufsize,
                                           GLsizei* length,
                                           GLint* params) {
  gles2::GetGLContext()->GetProgramResourceiv(program, program_interface, index,
                                              prop_count, props, bufsize,
                                              length, params);
}
GLint GL_APIENTRY GLES2GetProgramResourceLocation(GLuint program,
                                                  GLenum program_interface,
                                                  const char* name) {
  return gles2::GetGLContext()->GetProgramResourceLocation(
      program, program_interface, name);
}
void GL_APIENTRY GLES2MemoryBarrierEXT(GLbitfield barriers) {
  gles2::GetGLContext()->MemoryBarrierEXT(barriers);
}
void GL_APIENTRY GLES2MemoryBarrierByRegion(GLbitfield barriers) {
  gles2::GetGLContext()->MemoryBarrierByRegion(barriers);
}
void GL_APIENTRY GLES2SwapBuffers(GLuint64 swap_id, GLbitfield flags) {
  gles2::GetGLContext()->SwapBuffers(swap_id, flags);
}
GLuint GL_APIENTRY GLES2GetMaxValueInBufferCHROMIUM(GLuint buffer_id,
                                                    GLsizei count,
                                                    GLenum type,
                                                    GLuint offset) {
  return gles2::GetGLContext()->GetMaxValueInBufferCHROMIUM(buffer_id, count,
                                                            type, offset);
}
GLboolean GL_APIENTRY GLES2EnableFeatureCHROMIUM(const char* feature) {
  return gles2::GetGLContext()->EnableFeatureCHROMIUM(feature);
}
void* GL_APIENTRY GLES2MapBufferCHROMIUM(GLuint target, GLenum access) {
  return gles2::GetGLContext()->MapBufferCHROMIUM(target, access);
}
GLboolean GL_APIENTRY GLES2UnmapBufferCHROMIUM(GLuint target) {
  return gles2::GetGLContext()->UnmapBufferCHROMIUM(target);
}
void* GL_APIENTRY GLES2MapBufferSubDataCHROMIUM(GLuint target,
                                                GLintptr offset,
                                                GLsizeiptr size,
                                                GLenum access) {
  return gles2::GetGLContext()->MapBufferSubDataCHROMIUM(target, offset, size,
                                                         access);
}
void GL_APIENTRY GLES2UnmapBufferSubDataCHROMIUM(const void* mem) {
  gles2::GetGLContext()->UnmapBufferSubDataCHROMIUM(mem);
}
void* GL_APIENTRY GLES2MapBufferRange(GLenum target,
                                      GLintptr offset,
                                      GLsizeiptr size,
                                      GLbitfield access) {
  return gles2::GetGLContext()->MapBufferRange(target, offset, size, access);
}
GLboolean GL_APIENTRY GLES2UnmapBuffer(GLenum target) {
  return gles2::GetGLContext()->UnmapBuffer(target);
}
void GL_APIENTRY GLES2FlushMappedBufferRange(GLenum target,
                                             GLintptr offset,
                                             GLsizeiptr size) {
  gles2::GetGLContext()->FlushMappedBufferRange(target, offset, size);
}
void* GL_APIENTRY GLES2MapTexSubImage2DCHROMIUM(GLenum target,
                                                GLint level,
                                                GLint xoffset,
                                                GLint yoffset,
                                                GLsizei width,
                                                GLsizei height,
                                                GLenum format,
                                                GLenum type,
                                                GLenum access) {
  return gles2::GetGLContext()->MapTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset, width, height, format, type, access);
}
void GL_APIENTRY GLES2UnmapTexSubImage2DCHROMIUM(const void* mem) {
  gles2::GetGLContext()->UnmapTexSubImage2DCHROMIUM(mem);
}
void GL_APIENTRY GLES2ResizeCHROMIUM(GLuint width,
                                     GLuint height,
                                     GLfloat scale_factor,
                                     GLcolorSpace color_space,
                                     GLboolean alpha) {
  gles2::GetGLContext()->ResizeCHROMIUM(width, height, scale_factor,
                                        color_space, alpha);
}
const GLchar* GL_APIENTRY GLES2GetRequestableExtensionsCHROMIUM() {
  return gles2::GetGLContext()->GetRequestableExtensionsCHROMIUM();
}
void GL_APIENTRY GLES2RequestExtensionCHROMIUM(const char* extension) {
  gles2::GetGLContext()->RequestExtensionCHROMIUM(extension);
}
void GL_APIENTRY GLES2GetProgramInfoCHROMIUM(GLuint program,
                                             GLsizei bufsize,
                                             GLsizei* size,
                                             void* info) {
  gles2::GetGLContext()->GetProgramInfoCHROMIUM(program, bufsize, size, info);
}
void GL_APIENTRY GLES2GetUniformBlocksCHROMIUM(GLuint program,
                                               GLsizei bufsize,
                                               GLsizei* size,
                                               void* info) {
  gles2::GetGLContext()->GetUniformBlocksCHROMIUM(program, bufsize, size, info);
}
void GL_APIENTRY GLES2GetTransformFeedbackVaryingsCHROMIUM(GLuint program,
                                                           GLsizei bufsize,
                                                           GLsizei* size,
                                                           void* info) {
  gles2::GetGLContext()->GetTransformFeedbackVaryingsCHROMIUM(program, bufsize,
                                                              size, info);
}
void GL_APIENTRY GLES2GetUniformsES3CHROMIUM(GLuint program,
                                             GLsizei bufsize,
                                             GLsizei* size,
                                             void* info) {
  gles2::GetGLContext()->GetUniformsES3CHROMIUM(program, bufsize, size, info);
}
void GL_APIENTRY GLES2DescheduleUntilFinishedCHROMIUM() {
  gles2::GetGLContext()->DescheduleUntilFinishedCHROMIUM();
}
void GL_APIENTRY GLES2GetTranslatedShaderSourceANGLE(GLuint shader,
                                                     GLsizei bufsize,
                                                     GLsizei* length,
                                                     char* source) {
  gles2::GetGLContext()->GetTranslatedShaderSourceANGLE(shader, bufsize, length,
                                                        source);
}
void GL_APIENTRY GLES2CopyTextureCHROMIUM(GLuint source_id,
                                          GLint source_level,
                                          GLenum dest_target,
                                          GLuint dest_id,
                                          GLint dest_level,
                                          GLint internalformat,
                                          GLenum dest_type,
                                          GLboolean unpack_flip_y,
                                          GLboolean unpack_premultiply_alpha,
                                          GLboolean unpack_unmultiply_alpha) {
  gles2::GetGLContext()->CopyTextureCHROMIUM(
      source_id, source_level, dest_target, dest_id, dest_level, internalformat,
      dest_type, unpack_flip_y, unpack_premultiply_alpha,
      unpack_unmultiply_alpha);
}
void GL_APIENTRY
GLES2CopySubTextureCHROMIUM(GLuint source_id,
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
  gles2::GetGLContext()->CopySubTextureCHROMIUM(
      source_id, source_level, dest_target, dest_id, dest_level, xoffset,
      yoffset, x, y, width, height, unpack_flip_y, unpack_premultiply_alpha,
      unpack_unmultiply_alpha);
}
void GL_APIENTRY GLES2DrawArraysInstancedANGLE(GLenum mode,
                                               GLint first,
                                               GLsizei count,
                                               GLsizei primcount) {
  gles2::GetGLContext()->DrawArraysInstancedANGLE(mode, first, count,
                                                  primcount);
}
void GL_APIENTRY
GLES2DrawArraysInstancedBaseInstanceANGLE(GLenum mode,
                                          GLint first,
                                          GLsizei count,
                                          GLsizei primcount,
                                          GLuint baseinstance) {
  gles2::GetGLContext()->DrawArraysInstancedBaseInstanceANGLE(
      mode, first, count, primcount, baseinstance);
}
void GL_APIENTRY GLES2DrawElementsInstancedANGLE(GLenum mode,
                                                 GLsizei count,
                                                 GLenum type,
                                                 const void* indices,
                                                 GLsizei primcount) {
  gles2::GetGLContext()->DrawElementsInstancedANGLE(mode, count, type, indices,
                                                    primcount);
}
void GL_APIENTRY
GLES2DrawElementsInstancedBaseVertexBaseInstanceANGLE(GLenum mode,
                                                      GLsizei count,
                                                      GLenum type,
                                                      const void* indices,
                                                      GLsizei primcount,
                                                      GLint basevertex,
                                                      GLuint baseinstance) {
  gles2::GetGLContext()->DrawElementsInstancedBaseVertexBaseInstanceANGLE(
      mode, count, type, indices, primcount, basevertex, baseinstance);
}
void GL_APIENTRY GLES2VertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
  gles2::GetGLContext()->VertexAttribDivisorANGLE(index, divisor);
}
void GL_APIENTRY GLES2BindUniformLocationCHROMIUM(GLuint program,
                                                  GLint location,
                                                  const char* name) {
  gles2::GetGLContext()->BindUniformLocationCHROMIUM(program, location, name);
}
void GL_APIENTRY GLES2TraceBeginCHROMIUM(const char* category_name,
                                         const char* trace_name) {
  gles2::GetGLContext()->TraceBeginCHROMIUM(category_name, trace_name);
}
void GL_APIENTRY GLES2TraceEndCHROMIUM() {
  gles2::GetGLContext()->TraceEndCHROMIUM();
}
void GL_APIENTRY GLES2DiscardFramebufferEXT(GLenum target,
                                            GLsizei count,
                                            const GLenum* attachments) {
  gles2::GetGLContext()->DiscardFramebufferEXT(target, count, attachments);
}
void GL_APIENTRY GLES2LoseContextCHROMIUM(GLenum current, GLenum other) {
  gles2::GetGLContext()->LoseContextCHROMIUM(current, other);
}
void GL_APIENTRY GLES2DrawBuffersEXT(GLsizei count, const GLenum* bufs) {
  gles2::GetGLContext()->DrawBuffersEXT(count, bufs);
}
void GL_APIENTRY GLES2FlushDriverCachesCHROMIUM() {
  gles2::GetGLContext()->FlushDriverCachesCHROMIUM();
}
GLuint GL_APIENTRY GLES2GetLastFlushIdCHROMIUM() {
  return gles2::GetGLContext()->GetLastFlushIdCHROMIUM();
}
void GL_APIENTRY GLES2SetActiveURLCHROMIUM(const char* url) {
  gles2::GetGLContext()->SetActiveURLCHROMIUM(url);
}
void GL_APIENTRY GLES2ContextVisibilityHintCHROMIUM(GLboolean visibility) {
  gles2::GetGLContext()->ContextVisibilityHintCHROMIUM(visibility);
}
GLenum GL_APIENTRY GLES2GetGraphicsResetStatusKHR() {
  return gles2::GetGLContext()->GetGraphicsResetStatusKHR();
}
void GL_APIENTRY GLES2BlendBarrierKHR() {
  gles2::GetGLContext()->BlendBarrierKHR();
}
void GL_APIENTRY GLES2BindFragDataLocationIndexedEXT(GLuint program,
                                                     GLuint colorNumber,
                                                     GLuint index,
                                                     const char* name) {
  gles2::GetGLContext()->BindFragDataLocationIndexedEXT(program, colorNumber,
                                                        index, name);
}
void GL_APIENTRY GLES2BindFragDataLocationEXT(GLuint program,
                                              GLuint colorNumber,
                                              const char* name) {
  gles2::GetGLContext()->BindFragDataLocationEXT(program, colorNumber, name);
}
GLint GL_APIENTRY GLES2GetFragDataIndexEXT(GLuint program, const char* name) {
  return gles2::GetGLContext()->GetFragDataIndexEXT(program, name);
}
void GL_APIENTRY GLES2InitializeDiscardableTextureCHROMIUM(GLuint texture_id) {
  gles2::GetGLContext()->InitializeDiscardableTextureCHROMIUM(texture_id);
}
void GL_APIENTRY GLES2UnlockDiscardableTextureCHROMIUM(GLuint texture_id) {
  gles2::GetGLContext()->UnlockDiscardableTextureCHROMIUM(texture_id);
}
bool GL_APIENTRY GLES2LockDiscardableTextureCHROMIUM(GLuint texture_id) {
  return gles2::GetGLContext()->LockDiscardableTextureCHROMIUM(texture_id);
}
void GL_APIENTRY GLES2WindowRectanglesEXT(GLenum mode,
                                          GLsizei count,
                                          const GLint* box) {
  gles2::GetGLContext()->WindowRectanglesEXT(mode, count, box);
}
GLuint GL_APIENTRY GLES2CreateGpuFenceCHROMIUM() {
  return gles2::GetGLContext()->CreateGpuFenceCHROMIUM();
}
GLuint GL_APIENTRY GLES2CreateClientGpuFenceCHROMIUM(ClientGpuFence source) {
  return gles2::GetGLContext()->CreateClientGpuFenceCHROMIUM(source);
}
void GL_APIENTRY GLES2WaitGpuFenceCHROMIUM(GLuint gpu_fence_id) {
  gles2::GetGLContext()->WaitGpuFenceCHROMIUM(gpu_fence_id);
}
void GL_APIENTRY GLES2DestroyGpuFenceCHROMIUM(GLuint gpu_fence_id) {
  gles2::GetGLContext()->DestroyGpuFenceCHROMIUM(gpu_fence_id);
}
void GL_APIENTRY
GLES2InvalidateReadbackBufferShadowDataCHROMIUM(GLuint buffer_id) {
  gles2::GetGLContext()->InvalidateReadbackBufferShadowDataCHROMIUM(buffer_id);
}
void GL_APIENTRY GLES2FramebufferTextureMultiviewOVR(GLenum target,
                                                     GLenum attachment,
                                                     GLuint texture,
                                                     GLint level,
                                                     GLint baseViewIndex,
                                                     GLsizei numViews) {
  gles2::GetGLContext()->FramebufferTextureMultiviewOVR(
      target, attachment, texture, level, baseViewIndex, numViews);
}
void GL_APIENTRY GLES2MaxShaderCompilerThreadsKHR(GLuint count) {
  gles2::GetGLContext()->MaxShaderCompilerThreadsKHR(count);
}
GLuint GL_APIENTRY
GLES2CreateAndTexStorage2DSharedImageCHROMIUM(const GLbyte* mailbox) {
  return gles2::GetGLContext()->CreateAndTexStorage2DSharedImageCHROMIUM(
      mailbox);
}
void GL_APIENTRY GLES2BeginSharedImageAccessDirectCHROMIUM(GLuint texture,
                                                           GLenum mode) {
  gles2::GetGLContext()->BeginSharedImageAccessDirectCHROMIUM(texture, mode);
}
void GL_APIENTRY GLES2EndSharedImageAccessDirectCHROMIUM(GLuint texture) {
  gles2::GetGLContext()->EndSharedImageAccessDirectCHROMIUM(texture);
}
void GL_APIENTRY GLES2CopySharedImageINTERNAL(GLint xoffset,
                                              GLint yoffset,
                                              GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height,
                                              GLboolean unpack_flip_y,
                                              const GLbyte* mailboxes) {
  gles2::GetGLContext()->CopySharedImageINTERNAL(
      xoffset, yoffset, x, y, width, height, unpack_flip_y, mailboxes);
}
void GL_APIENTRY
GLES2CopySharedImageToTextureINTERNAL(GLuint texture,
                                      GLenum target,
                                      GLuint internal_format,
                                      GLenum type,
                                      GLint src_x,
                                      GLint src_y,
                                      GLsizei width,
                                      GLsizei height,
                                      GLboolean flip_y,
                                      const GLbyte* src_mailbox) {
  gles2::GetGLContext()->CopySharedImageToTextureINTERNAL(
      texture, target, internal_format, type, src_x, src_y, width, height,
      flip_y, src_mailbox);
}
GLboolean GL_APIENTRY
GLES2ReadbackARGBImagePixelsINTERNAL(const GLbyte* mailbox,
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
  return gles2::GetGLContext()->ReadbackARGBImagePixelsINTERNAL(
      mailbox, dst_color_space, dst_color_space_size, dst_size, dst_width,
      dst_height, dst_color_type, dst_alpha_type, dst_row_bytes, src_x, src_y,
      plane_index, pixels);
}
void GL_APIENTRY GLES2WritePixelsYUVINTERNAL(const GLbyte* mailbox,
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
  gles2::GetGLContext()->WritePixelsYUVINTERNAL(
      mailbox, src_size_plane1, src_size_plane2, src_size_plane3,
      src_size_plane4, src_width, src_height, src_plane_config, src_subsampling,
      src_datatype, src_row_bytes_plane1, src_row_bytes_plane2,
      src_row_bytes_plane3, src_row_bytes_plane4, src_pixels_plane1,
      src_pixels_plane2, src_pixels_plane3, src_pixels_plane4);
}
void GL_APIENTRY GLES2EnableiOES(GLenum target, GLuint index) {
  gles2::GetGLContext()->EnableiOES(target, index);
}
void GL_APIENTRY GLES2DisableiOES(GLenum target, GLuint index) {
  gles2::GetGLContext()->DisableiOES(target, index);
}
void GL_APIENTRY GLES2BlendEquationiOES(GLuint buf, GLenum mode) {
  gles2::GetGLContext()->BlendEquationiOES(buf, mode);
}
void GL_APIENTRY GLES2BlendEquationSeparateiOES(GLuint buf,
                                                GLenum modeRGB,
                                                GLenum modeAlpha) {
  gles2::GetGLContext()->BlendEquationSeparateiOES(buf, modeRGB, modeAlpha);
}
void GL_APIENTRY GLES2BlendFunciOES(GLuint buf, GLenum src, GLenum dst) {
  gles2::GetGLContext()->BlendFunciOES(buf, src, dst);
}
void GL_APIENTRY GLES2BlendFuncSeparateiOES(GLuint buf,
                                            GLenum srcRGB,
                                            GLenum dstRGB,
                                            GLenum srcAlpha,
                                            GLenum dstAlpha) {
  gles2::GetGLContext()->BlendFuncSeparateiOES(buf, srcRGB, dstRGB, srcAlpha,
                                               dstAlpha);
}
void GL_APIENTRY GLES2ColorMaskiOES(GLuint buf,
                                    GLboolean r,
                                    GLboolean g,
                                    GLboolean b,
                                    GLboolean a) {
  gles2::GetGLContext()->ColorMaskiOES(buf, r, g, b, a);
}
GLboolean GL_APIENTRY GLES2IsEnablediOES(GLenum target, GLuint index) {
  return gles2::GetGLContext()->IsEnablediOES(target, index);
}
void GL_APIENTRY GLES2ProvokingVertexANGLE(GLenum provokeMode) {
  gles2::GetGLContext()->ProvokingVertexANGLE(provokeMode);
}
void GL_APIENTRY
GLES2FramebufferMemorylessPixelLocalStorageANGLE(GLint plane,
                                                 GLenum internalformat) {
  gles2::GetGLContext()->FramebufferMemorylessPixelLocalStorageANGLE(
      plane, internalformat);
}
void GL_APIENTRY
GLES2FramebufferTexturePixelLocalStorageANGLE(GLint plane,
                                              GLuint backingtexture,
                                              GLint level,
                                              GLint layer) {
  gles2::GetGLContext()->FramebufferTexturePixelLocalStorageANGLE(
      plane, backingtexture, level, layer);
}
void GL_APIENTRY
GLES2FramebufferPixelLocalClearValuefvANGLE(GLint plane, const GLfloat* value) {
  gles2::GetGLContext()->FramebufferPixelLocalClearValuefvANGLE(plane, value);
}
void GL_APIENTRY
GLES2FramebufferPixelLocalClearValueivANGLE(GLint plane, const GLint* value) {
  gles2::GetGLContext()->FramebufferPixelLocalClearValueivANGLE(plane, value);
}
void GL_APIENTRY
GLES2FramebufferPixelLocalClearValueuivANGLE(GLint plane, const GLuint* value) {
  gles2::GetGLContext()->FramebufferPixelLocalClearValueuivANGLE(plane, value);
}
void GL_APIENTRY GLES2BeginPixelLocalStorageANGLE(GLsizei count,
                                                  const GLenum* loadops) {
  gles2::GetGLContext()->BeginPixelLocalStorageANGLE(count, loadops);
}
void GL_APIENTRY GLES2EndPixelLocalStorageANGLE(GLsizei count,
                                                const GLenum* storeops) {
  gles2::GetGLContext()->EndPixelLocalStorageANGLE(count, storeops);
}
void GL_APIENTRY GLES2PixelLocalStorageBarrierANGLE() {
  gles2::GetGLContext()->PixelLocalStorageBarrierANGLE();
}
void GL_APIENTRY GLES2FramebufferPixelLocalStorageInterruptANGLE() {
  gles2::GetGLContext()->FramebufferPixelLocalStorageInterruptANGLE();
}
void GL_APIENTRY GLES2FramebufferPixelLocalStorageRestoreANGLE() {
  gles2::GetGLContext()->FramebufferPixelLocalStorageRestoreANGLE();
}
void GL_APIENTRY
GLES2GetFramebufferPixelLocalStorageParameterfvANGLE(GLint plane,
                                                     GLenum pname,
                                                     GLfloat* params) {
  gles2::GetGLContext()->GetFramebufferPixelLocalStorageParameterfvANGLE(
      plane, pname, params);
}
void GL_APIENTRY
GLES2GetFramebufferPixelLocalStorageParameterivANGLE(GLint plane,
                                                     GLenum pname,
                                                     GLint* params) {
  gles2::GetGLContext()->GetFramebufferPixelLocalStorageParameterivANGLE(
      plane, pname, params);
}
void GL_APIENTRY GLES2ClipControlEXT(GLenum origin, GLenum depth) {
  gles2::GetGLContext()->ClipControlEXT(origin, depth);
}
void GL_APIENTRY GLES2PolygonModeANGLE(GLenum face, GLenum mode) {
  gles2::GetGLContext()->PolygonModeANGLE(face, mode);
}
void GL_APIENTRY GLES2PolygonOffsetClampEXT(GLfloat factor,
                                            GLfloat units,
                                            GLfloat clamp) {
  gles2::GetGLContext()->PolygonOffsetClampEXT(factor, units, clamp);
}

namespace gles2 {

extern const NameToFunc g_gles2_function_table[] = {
    {
        "glActiveTexture",
        reinterpret_cast<GLES2FunctionPointer>(glActiveTexture),
    },
    {
        "glAttachShader",
        reinterpret_cast<GLES2FunctionPointer>(glAttachShader),
    },
    {
        "glBindAttribLocation",
        reinterpret_cast<GLES2FunctionPointer>(glBindAttribLocation),
    },
    {
        "glBindBuffer",
        reinterpret_cast<GLES2FunctionPointer>(glBindBuffer),
    },
    {
        "glBindBufferBase",
        reinterpret_cast<GLES2FunctionPointer>(glBindBufferBase),
    },
    {
        "glBindBufferRange",
        reinterpret_cast<GLES2FunctionPointer>(glBindBufferRange),
    },
    {
        "glBindFramebuffer",
        reinterpret_cast<GLES2FunctionPointer>(glBindFramebuffer),
    },
    {
        "glBindRenderbuffer",
        reinterpret_cast<GLES2FunctionPointer>(glBindRenderbuffer),
    },
    {
        "glBindSampler",
        reinterpret_cast<GLES2FunctionPointer>(glBindSampler),
    },
    {
        "glBindTexture",
        reinterpret_cast<GLES2FunctionPointer>(glBindTexture),
    },
    {
        "glBindTransformFeedback",
        reinterpret_cast<GLES2FunctionPointer>(glBindTransformFeedback),
    },
    {
        "glBlendColor",
        reinterpret_cast<GLES2FunctionPointer>(glBlendColor),
    },
    {
        "glBlendEquation",
        reinterpret_cast<GLES2FunctionPointer>(glBlendEquation),
    },
    {
        "glBlendEquationSeparate",
        reinterpret_cast<GLES2FunctionPointer>(glBlendEquationSeparate),
    },
    {
        "glBlendFunc",
        reinterpret_cast<GLES2FunctionPointer>(glBlendFunc),
    },
    {
        "glBlendFuncSeparate",
        reinterpret_cast<GLES2FunctionPointer>(glBlendFuncSeparate),
    },
    {
        "glBufferData",
        reinterpret_cast<GLES2FunctionPointer>(glBufferData),
    },
    {
        "glBufferSubData",
        reinterpret_cast<GLES2FunctionPointer>(glBufferSubData),
    },
    {
        "glCheckFramebufferStatus",
        reinterpret_cast<GLES2FunctionPointer>(glCheckFramebufferStatus),
    },
    {
        "glClear",
        reinterpret_cast<GLES2FunctionPointer>(glClear),
    },
    {
        "glClearBufferfi",
        reinterpret_cast<GLES2FunctionPointer>(glClearBufferfi),
    },
    {
        "glClearBufferfv",
        reinterpret_cast<GLES2FunctionPointer>(glClearBufferfv),
    },
    {
        "glClearBufferiv",
        reinterpret_cast<GLES2FunctionPointer>(glClearBufferiv),
    },
    {
        "glClearBufferuiv",
        reinterpret_cast<GLES2FunctionPointer>(glClearBufferuiv),
    },
    {
        "glClearColor",
        reinterpret_cast<GLES2FunctionPointer>(glClearColor),
    },
    {
        "glClearDepthf",
        reinterpret_cast<GLES2FunctionPointer>(glClearDepthf),
    },
    {
        "glClearStencil",
        reinterpret_cast<GLES2FunctionPointer>(glClearStencil),
    },
    {
        "glClientWaitSync",
        reinterpret_cast<GLES2FunctionPointer>(glClientWaitSync),
    },
    {
        "glColorMask",
        reinterpret_cast<GLES2FunctionPointer>(glColorMask),
    },
    {
        "glCompileShader",
        reinterpret_cast<GLES2FunctionPointer>(glCompileShader),
    },
    {
        "glCompressedTexImage2D",
        reinterpret_cast<GLES2FunctionPointer>(glCompressedTexImage2D),
    },
    {
        "glCompressedTexSubImage2D",
        reinterpret_cast<GLES2FunctionPointer>(glCompressedTexSubImage2D),
    },
    {
        "glCompressedTexImage3D",
        reinterpret_cast<GLES2FunctionPointer>(glCompressedTexImage3D),
    },
    {
        "glCompressedTexSubImage3D",
        reinterpret_cast<GLES2FunctionPointer>(glCompressedTexSubImage3D),
    },
    {
        "glCopyBufferSubData",
        reinterpret_cast<GLES2FunctionPointer>(glCopyBufferSubData),
    },
    {
        "glCopyTexImage2D",
        reinterpret_cast<GLES2FunctionPointer>(glCopyTexImage2D),
    },
    {
        "glCopyTexSubImage2D",
        reinterpret_cast<GLES2FunctionPointer>(glCopyTexSubImage2D),
    },
    {
        "glCopyTexSubImage3D",
        reinterpret_cast<GLES2FunctionPointer>(glCopyTexSubImage3D),
    },
    {
        "glCreateProgram",
        reinterpret_cast<GLES2FunctionPointer>(glCreateProgram),
    },
    {
        "glCreateShader",
        reinterpret_cast<GLES2FunctionPointer>(glCreateShader),
    },
    {
        "glCullFace",
        reinterpret_cast<GLES2FunctionPointer>(glCullFace),
    },
    {
        "glDeleteBuffers",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteBuffers),
    },
    {
        "glDeleteFramebuffers",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteFramebuffers),
    },
    {
        "glDeleteProgram",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteProgram),
    },
    {
        "glDeleteRenderbuffers",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteRenderbuffers),
    },
    {
        "glDeleteSamplers",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteSamplers),
    },
    {
        "glDeleteSync",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteSync),
    },
    {
        "glDeleteShader",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteShader),
    },
    {
        "glDeleteTextures",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteTextures),
    },
    {
        "glDeleteTransformFeedbacks",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteTransformFeedbacks),
    },
    {
        "glDepthFunc",
        reinterpret_cast<GLES2FunctionPointer>(glDepthFunc),
    },
    {
        "glDepthMask",
        reinterpret_cast<GLES2FunctionPointer>(glDepthMask),
    },
    {
        "glDepthRangef",
        reinterpret_cast<GLES2FunctionPointer>(glDepthRangef),
    },
    {
        "glDetachShader",
        reinterpret_cast<GLES2FunctionPointer>(glDetachShader),
    },
    {
        "glDisable",
        reinterpret_cast<GLES2FunctionPointer>(glDisable),
    },
    {
        "glDisableVertexAttribArray",
        reinterpret_cast<GLES2FunctionPointer>(glDisableVertexAttribArray),
    },
    {
        "glDrawArrays",
        reinterpret_cast<GLES2FunctionPointer>(glDrawArrays),
    },
    {
        "glDrawElements",
        reinterpret_cast<GLES2FunctionPointer>(glDrawElements),
    },
    {
        "glDrawRangeElements",
        reinterpret_cast<GLES2FunctionPointer>(glDrawRangeElements),
    },
    {
        "glEnable",
        reinterpret_cast<GLES2FunctionPointer>(glEnable),
    },
    {
        "glEnableVertexAttribArray",
        reinterpret_cast<GLES2FunctionPointer>(glEnableVertexAttribArray),
    },
    {
        "glFenceSync",
        reinterpret_cast<GLES2FunctionPointer>(glFenceSync),
    },
    {
        "glFinish",
        reinterpret_cast<GLES2FunctionPointer>(glFinish),
    },
    {
        "glFlush",
        reinterpret_cast<GLES2FunctionPointer>(glFlush),
    },
    {
        "glFramebufferRenderbuffer",
        reinterpret_cast<GLES2FunctionPointer>(glFramebufferRenderbuffer),
    },
    {
        "glFramebufferTexture2D",
        reinterpret_cast<GLES2FunctionPointer>(glFramebufferTexture2D),
    },
    {
        "glFramebufferTextureLayer",
        reinterpret_cast<GLES2FunctionPointer>(glFramebufferTextureLayer),
    },
    {
        "glFrontFace",
        reinterpret_cast<GLES2FunctionPointer>(glFrontFace),
    },
    {
        "glGenBuffers",
        reinterpret_cast<GLES2FunctionPointer>(glGenBuffers),
    },
    {
        "glGenerateMipmap",
        reinterpret_cast<GLES2FunctionPointer>(glGenerateMipmap),
    },
    {
        "glGenFramebuffers",
        reinterpret_cast<GLES2FunctionPointer>(glGenFramebuffers),
    },
    {
        "glGenRenderbuffers",
        reinterpret_cast<GLES2FunctionPointer>(glGenRenderbuffers),
    },
    {
        "glGenSamplers",
        reinterpret_cast<GLES2FunctionPointer>(glGenSamplers),
    },
    {
        "glGenTextures",
        reinterpret_cast<GLES2FunctionPointer>(glGenTextures),
    },
    {
        "glGenTransformFeedbacks",
        reinterpret_cast<GLES2FunctionPointer>(glGenTransformFeedbacks),
    },
    {
        "glGetActiveAttrib",
        reinterpret_cast<GLES2FunctionPointer>(glGetActiveAttrib),
    },
    {
        "glGetActiveUniform",
        reinterpret_cast<GLES2FunctionPointer>(glGetActiveUniform),
    },
    {
        "glGetActiveUniformBlockiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetActiveUniformBlockiv),
    },
    {
        "glGetActiveUniformBlockName",
        reinterpret_cast<GLES2FunctionPointer>(glGetActiveUniformBlockName),
    },
    {
        "glGetActiveUniformsiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetActiveUniformsiv),
    },
    {
        "glGetAttachedShaders",
        reinterpret_cast<GLES2FunctionPointer>(glGetAttachedShaders),
    },
    {
        "glGetAttribLocation",
        reinterpret_cast<GLES2FunctionPointer>(glGetAttribLocation),
    },
    {
        "glGetBooleanv",
        reinterpret_cast<GLES2FunctionPointer>(glGetBooleanv),
    },
    {
        "glGetBooleani_v",
        reinterpret_cast<GLES2FunctionPointer>(glGetBooleani_v),
    },
    {
        "glGetBufferParameteri64v",
        reinterpret_cast<GLES2FunctionPointer>(glGetBufferParameteri64v),
    },
    {
        "glGetBufferParameteriv",
        reinterpret_cast<GLES2FunctionPointer>(glGetBufferParameteriv),
    },
    {
        "glGetError",
        reinterpret_cast<GLES2FunctionPointer>(glGetError),
    },
    {
        "glGetFloatv",
        reinterpret_cast<GLES2FunctionPointer>(glGetFloatv),
    },
    {
        "glGetFragDataLocation",
        reinterpret_cast<GLES2FunctionPointer>(glGetFragDataLocation),
    },
    {
        "glGetFramebufferAttachmentParameteriv",
        reinterpret_cast<GLES2FunctionPointer>(
            glGetFramebufferAttachmentParameteriv),
    },
    {
        "glGetInteger64v",
        reinterpret_cast<GLES2FunctionPointer>(glGetInteger64v),
    },
    {
        "glGetIntegeri_v",
        reinterpret_cast<GLES2FunctionPointer>(glGetIntegeri_v),
    },
    {
        "glGetInteger64i_v",
        reinterpret_cast<GLES2FunctionPointer>(glGetInteger64i_v),
    },
    {
        "glGetIntegerv",
        reinterpret_cast<GLES2FunctionPointer>(glGetIntegerv),
    },
    {
        "glGetInternalformativ",
        reinterpret_cast<GLES2FunctionPointer>(glGetInternalformativ),
    },
    {
        "glGetProgramiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetProgramiv),
    },
    {
        "glGetProgramInfoLog",
        reinterpret_cast<GLES2FunctionPointer>(glGetProgramInfoLog),
    },
    {
        "glGetRenderbufferParameteriv",
        reinterpret_cast<GLES2FunctionPointer>(glGetRenderbufferParameteriv),
    },
    {
        "glGetSamplerParameterfv",
        reinterpret_cast<GLES2FunctionPointer>(glGetSamplerParameterfv),
    },
    {
        "glGetSamplerParameteriv",
        reinterpret_cast<GLES2FunctionPointer>(glGetSamplerParameteriv),
    },
    {
        "glGetShaderiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetShaderiv),
    },
    {
        "glGetShaderInfoLog",
        reinterpret_cast<GLES2FunctionPointer>(glGetShaderInfoLog),
    },
    {
        "glGetShaderPrecisionFormat",
        reinterpret_cast<GLES2FunctionPointer>(glGetShaderPrecisionFormat),
    },
    {
        "glGetShaderSource",
        reinterpret_cast<GLES2FunctionPointer>(glGetShaderSource),
    },
    {
        "glGetString",
        reinterpret_cast<GLES2FunctionPointer>(glGetString),
    },
    {
        "glGetStringi",
        reinterpret_cast<GLES2FunctionPointer>(glGetStringi),
    },
    {
        "glGetSynciv",
        reinterpret_cast<GLES2FunctionPointer>(glGetSynciv),
    },
    {
        "glGetTexParameterfv",
        reinterpret_cast<GLES2FunctionPointer>(glGetTexParameterfv),
    },
    {
        "glGetTexParameteriv",
        reinterpret_cast<GLES2FunctionPointer>(glGetTexParameteriv),
    },
    {
        "glGetTransformFeedbackVarying",
        reinterpret_cast<GLES2FunctionPointer>(glGetTransformFeedbackVarying),
    },
    {
        "glGetUniformBlockIndex",
        reinterpret_cast<GLES2FunctionPointer>(glGetUniformBlockIndex),
    },
    {
        "glGetUniformfv",
        reinterpret_cast<GLES2FunctionPointer>(glGetUniformfv),
    },
    {
        "glGetUniformiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetUniformiv),
    },
    {
        "glGetUniformuiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetUniformuiv),
    },
    {
        "glGetUniformIndices",
        reinterpret_cast<GLES2FunctionPointer>(glGetUniformIndices),
    },
    {
        "glGetUniformLocation",
        reinterpret_cast<GLES2FunctionPointer>(glGetUniformLocation),
    },
    {
        "glGetVertexAttribfv",
        reinterpret_cast<GLES2FunctionPointer>(glGetVertexAttribfv),
    },
    {
        "glGetVertexAttribiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetVertexAttribiv),
    },
    {
        "glGetVertexAttribIiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetVertexAttribIiv),
    },
    {
        "glGetVertexAttribIuiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetVertexAttribIuiv),
    },
    {
        "glGetVertexAttribPointerv",
        reinterpret_cast<GLES2FunctionPointer>(glGetVertexAttribPointerv),
    },
    {
        "glHint",
        reinterpret_cast<GLES2FunctionPointer>(glHint),
    },
    {
        "glInvalidateFramebuffer",
        reinterpret_cast<GLES2FunctionPointer>(glInvalidateFramebuffer),
    },
    {
        "glInvalidateSubFramebuffer",
        reinterpret_cast<GLES2FunctionPointer>(glInvalidateSubFramebuffer),
    },
    {
        "glIsBuffer",
        reinterpret_cast<GLES2FunctionPointer>(glIsBuffer),
    },
    {
        "glIsEnabled",
        reinterpret_cast<GLES2FunctionPointer>(glIsEnabled),
    },
    {
        "glIsFramebuffer",
        reinterpret_cast<GLES2FunctionPointer>(glIsFramebuffer),
    },
    {
        "glIsProgram",
        reinterpret_cast<GLES2FunctionPointer>(glIsProgram),
    },
    {
        "glIsRenderbuffer",
        reinterpret_cast<GLES2FunctionPointer>(glIsRenderbuffer),
    },
    {
        "glIsSampler",
        reinterpret_cast<GLES2FunctionPointer>(glIsSampler),
    },
    {
        "glIsShader",
        reinterpret_cast<GLES2FunctionPointer>(glIsShader),
    },
    {
        "glIsSync",
        reinterpret_cast<GLES2FunctionPointer>(glIsSync),
    },
    {
        "glIsTexture",
        reinterpret_cast<GLES2FunctionPointer>(glIsTexture),
    },
    {
        "glIsTransformFeedback",
        reinterpret_cast<GLES2FunctionPointer>(glIsTransformFeedback),
    },
    {
        "glLineWidth",
        reinterpret_cast<GLES2FunctionPointer>(glLineWidth),
    },
    {
        "glLinkProgram",
        reinterpret_cast<GLES2FunctionPointer>(glLinkProgram),
    },
    {
        "glPauseTransformFeedback",
        reinterpret_cast<GLES2FunctionPointer>(glPauseTransformFeedback),
    },
    {
        "glPixelStorei",
        reinterpret_cast<GLES2FunctionPointer>(glPixelStorei),
    },
    {
        "glPolygonOffset",
        reinterpret_cast<GLES2FunctionPointer>(glPolygonOffset),
    },
    {
        "glReadBuffer",
        reinterpret_cast<GLES2FunctionPointer>(glReadBuffer),
    },
    {
        "glReadPixels",
        reinterpret_cast<GLES2FunctionPointer>(glReadPixels),
    },
    {
        "glReleaseShaderCompiler",
        reinterpret_cast<GLES2FunctionPointer>(glReleaseShaderCompiler),
    },
    {
        "glRenderbufferStorage",
        reinterpret_cast<GLES2FunctionPointer>(glRenderbufferStorage),
    },
    {
        "glResumeTransformFeedback",
        reinterpret_cast<GLES2FunctionPointer>(glResumeTransformFeedback),
    },
    {
        "glSampleCoverage",
        reinterpret_cast<GLES2FunctionPointer>(glSampleCoverage),
    },
    {
        "glSamplerParameterf",
        reinterpret_cast<GLES2FunctionPointer>(glSamplerParameterf),
    },
    {
        "glSamplerParameterfv",
        reinterpret_cast<GLES2FunctionPointer>(glSamplerParameterfv),
    },
    {
        "glSamplerParameteri",
        reinterpret_cast<GLES2FunctionPointer>(glSamplerParameteri),
    },
    {
        "glSamplerParameteriv",
        reinterpret_cast<GLES2FunctionPointer>(glSamplerParameteriv),
    },
    {
        "glScissor",
        reinterpret_cast<GLES2FunctionPointer>(glScissor),
    },
    {
        "glShaderBinary",
        reinterpret_cast<GLES2FunctionPointer>(glShaderBinary),
    },
    {
        "glShaderSource",
        reinterpret_cast<GLES2FunctionPointer>(glShaderSource),
    },
    {
        "glShallowFinishCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glShallowFinishCHROMIUM),
    },
    {
        "glOrderingBarrierCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glOrderingBarrierCHROMIUM),
    },
    {
        "glMultiDrawArraysWEBGL",
        reinterpret_cast<GLES2FunctionPointer>(glMultiDrawArraysWEBGL),
    },
    {
        "glMultiDrawArraysInstancedWEBGL",
        reinterpret_cast<GLES2FunctionPointer>(glMultiDrawArraysInstancedWEBGL),
    },
    {
        "glMultiDrawArraysInstancedBaseInstanceWEBGL",
        reinterpret_cast<GLES2FunctionPointer>(
            glMultiDrawArraysInstancedBaseInstanceWEBGL),
    },
    {
        "glMultiDrawElementsWEBGL",
        reinterpret_cast<GLES2FunctionPointer>(glMultiDrawElementsWEBGL),
    },
    {
        "glMultiDrawElementsInstancedWEBGL",
        reinterpret_cast<GLES2FunctionPointer>(
            glMultiDrawElementsInstancedWEBGL),
    },
    {
        "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
        reinterpret_cast<GLES2FunctionPointer>(
            glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL),
    },
    {
        "glStencilFunc",
        reinterpret_cast<GLES2FunctionPointer>(glStencilFunc),
    },
    {
        "glStencilFuncSeparate",
        reinterpret_cast<GLES2FunctionPointer>(glStencilFuncSeparate),
    },
    {
        "glStencilMask",
        reinterpret_cast<GLES2FunctionPointer>(glStencilMask),
    },
    {
        "glStencilMaskSeparate",
        reinterpret_cast<GLES2FunctionPointer>(glStencilMaskSeparate),
    },
    {
        "glStencilOp",
        reinterpret_cast<GLES2FunctionPointer>(glStencilOp),
    },
    {
        "glStencilOpSeparate",
        reinterpret_cast<GLES2FunctionPointer>(glStencilOpSeparate),
    },
    {
        "glTexImage2D",
        reinterpret_cast<GLES2FunctionPointer>(glTexImage2D),
    },
    {
        "glTexImage3D",
        reinterpret_cast<GLES2FunctionPointer>(glTexImage3D),
    },
    {
        "glTexParameterf",
        reinterpret_cast<GLES2FunctionPointer>(glTexParameterf),
    },
    {
        "glTexParameterfv",
        reinterpret_cast<GLES2FunctionPointer>(glTexParameterfv),
    },
    {
        "glTexParameteri",
        reinterpret_cast<GLES2FunctionPointer>(glTexParameteri),
    },
    {
        "glTexParameteriv",
        reinterpret_cast<GLES2FunctionPointer>(glTexParameteriv),
    },
    {
        "glTexStorage3D",
        reinterpret_cast<GLES2FunctionPointer>(glTexStorage3D),
    },
    {
        "glTexSubImage2D",
        reinterpret_cast<GLES2FunctionPointer>(glTexSubImage2D),
    },
    {
        "glTexSubImage3D",
        reinterpret_cast<GLES2FunctionPointer>(glTexSubImage3D),
    },
    {
        "glTransformFeedbackVaryings",
        reinterpret_cast<GLES2FunctionPointer>(glTransformFeedbackVaryings),
    },
    {
        "glUniform1f",
        reinterpret_cast<GLES2FunctionPointer>(glUniform1f),
    },
    {
        "glUniform1fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform1fv),
    },
    {
        "glUniform1i",
        reinterpret_cast<GLES2FunctionPointer>(glUniform1i),
    },
    {
        "glUniform1iv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform1iv),
    },
    {
        "glUniform1ui",
        reinterpret_cast<GLES2FunctionPointer>(glUniform1ui),
    },
    {
        "glUniform1uiv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform1uiv),
    },
    {
        "glUniform2f",
        reinterpret_cast<GLES2FunctionPointer>(glUniform2f),
    },
    {
        "glUniform2fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform2fv),
    },
    {
        "glUniform2i",
        reinterpret_cast<GLES2FunctionPointer>(glUniform2i),
    },
    {
        "glUniform2iv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform2iv),
    },
    {
        "glUniform2ui",
        reinterpret_cast<GLES2FunctionPointer>(glUniform2ui),
    },
    {
        "glUniform2uiv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform2uiv),
    },
    {
        "glUniform3f",
        reinterpret_cast<GLES2FunctionPointer>(glUniform3f),
    },
    {
        "glUniform3fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform3fv),
    },
    {
        "glUniform3i",
        reinterpret_cast<GLES2FunctionPointer>(glUniform3i),
    },
    {
        "glUniform3iv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform3iv),
    },
    {
        "glUniform3ui",
        reinterpret_cast<GLES2FunctionPointer>(glUniform3ui),
    },
    {
        "glUniform3uiv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform3uiv),
    },
    {
        "glUniform4f",
        reinterpret_cast<GLES2FunctionPointer>(glUniform4f),
    },
    {
        "glUniform4fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform4fv),
    },
    {
        "glUniform4i",
        reinterpret_cast<GLES2FunctionPointer>(glUniform4i),
    },
    {
        "glUniform4iv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform4iv),
    },
    {
        "glUniform4ui",
        reinterpret_cast<GLES2FunctionPointer>(glUniform4ui),
    },
    {
        "glUniform4uiv",
        reinterpret_cast<GLES2FunctionPointer>(glUniform4uiv),
    },
    {
        "glUniformBlockBinding",
        reinterpret_cast<GLES2FunctionPointer>(glUniformBlockBinding),
    },
    {
        "glUniformMatrix2fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniformMatrix2fv),
    },
    {
        "glUniformMatrix2x3fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniformMatrix2x3fv),
    },
    {
        "glUniformMatrix2x4fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniformMatrix2x4fv),
    },
    {
        "glUniformMatrix3fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniformMatrix3fv),
    },
    {
        "glUniformMatrix3x2fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniformMatrix3x2fv),
    },
    {
        "glUniformMatrix3x4fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniformMatrix3x4fv),
    },
    {
        "glUniformMatrix4fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniformMatrix4fv),
    },
    {
        "glUniformMatrix4x2fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniformMatrix4x2fv),
    },
    {
        "glUniformMatrix4x3fv",
        reinterpret_cast<GLES2FunctionPointer>(glUniformMatrix4x3fv),
    },
    {
        "glUseProgram",
        reinterpret_cast<GLES2FunctionPointer>(glUseProgram),
    },
    {
        "glValidateProgram",
        reinterpret_cast<GLES2FunctionPointer>(glValidateProgram),
    },
    {
        "glVertexAttrib1f",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttrib1f),
    },
    {
        "glVertexAttrib1fv",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttrib1fv),
    },
    {
        "glVertexAttrib2f",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttrib2f),
    },
    {
        "glVertexAttrib2fv",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttrib2fv),
    },
    {
        "glVertexAttrib3f",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttrib3f),
    },
    {
        "glVertexAttrib3fv",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttrib3fv),
    },
    {
        "glVertexAttrib4f",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttrib4f),
    },
    {
        "glVertexAttrib4fv",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttrib4fv),
    },
    {
        "glVertexAttribI4i",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttribI4i),
    },
    {
        "glVertexAttribI4iv",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttribI4iv),
    },
    {
        "glVertexAttribI4ui",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttribI4ui),
    },
    {
        "glVertexAttribI4uiv",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttribI4uiv),
    },
    {
        "glVertexAttribIPointer",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttribIPointer),
    },
    {
        "glVertexAttribPointer",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttribPointer),
    },
    {
        "glViewport",
        reinterpret_cast<GLES2FunctionPointer>(glViewport),
    },
    {
        "glWaitSync",
        reinterpret_cast<GLES2FunctionPointer>(glWaitSync),
    },
    {
        "glBlitFramebufferCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glBlitFramebufferCHROMIUM),
    },
    {
        "glRenderbufferStorageMultisampleCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glRenderbufferStorageMultisampleCHROMIUM),
    },
    {
        "glRenderbufferStorageMultisampleAdvancedAMD",
        reinterpret_cast<GLES2FunctionPointer>(
            glRenderbufferStorageMultisampleAdvancedAMD),
    },
    {
        "glRenderbufferStorageMultisampleEXT",
        reinterpret_cast<GLES2FunctionPointer>(
            glRenderbufferStorageMultisampleEXT),
    },
    {
        "glFramebufferTexture2DMultisampleEXT",
        reinterpret_cast<GLES2FunctionPointer>(
            glFramebufferTexture2DMultisampleEXT),
    },
    {
        "glTexStorage2DEXT",
        reinterpret_cast<GLES2FunctionPointer>(glTexStorage2DEXT),
    },
    {
        "glGenQueriesEXT",
        reinterpret_cast<GLES2FunctionPointer>(glGenQueriesEXT),
    },
    {
        "glDeleteQueriesEXT",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteQueriesEXT),
    },
    {
        "glQueryCounterEXT",
        reinterpret_cast<GLES2FunctionPointer>(glQueryCounterEXT),
    },
    {
        "glIsQueryEXT",
        reinterpret_cast<GLES2FunctionPointer>(glIsQueryEXT),
    },
    {
        "glBeginQueryEXT",
        reinterpret_cast<GLES2FunctionPointer>(glBeginQueryEXT),
    },
    {
        "glBeginTransformFeedback",
        reinterpret_cast<GLES2FunctionPointer>(glBeginTransformFeedback),
    },
    {
        "glEndQueryEXT",
        reinterpret_cast<GLES2FunctionPointer>(glEndQueryEXT),
    },
    {
        "glEndTransformFeedback",
        reinterpret_cast<GLES2FunctionPointer>(glEndTransformFeedback),
    },
    {
        "glGetQueryivEXT",
        reinterpret_cast<GLES2FunctionPointer>(glGetQueryivEXT),
    },
    {
        "glGetQueryObjectivEXT",
        reinterpret_cast<GLES2FunctionPointer>(glGetQueryObjectivEXT),
    },
    {
        "glGetQueryObjectuivEXT",
        reinterpret_cast<GLES2FunctionPointer>(glGetQueryObjectuivEXT),
    },
    {
        "glGetQueryObjecti64vEXT",
        reinterpret_cast<GLES2FunctionPointer>(glGetQueryObjecti64vEXT),
    },
    {
        "glGetQueryObjectui64vEXT",
        reinterpret_cast<GLES2FunctionPointer>(glGetQueryObjectui64vEXT),
    },
    {
        "glSetDisjointValueSyncCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glSetDisjointValueSyncCHROMIUM),
    },
    {
        "glInsertEventMarkerEXT",
        reinterpret_cast<GLES2FunctionPointer>(glInsertEventMarkerEXT),
    },
    {
        "glPushGroupMarkerEXT",
        reinterpret_cast<GLES2FunctionPointer>(glPushGroupMarkerEXT),
    },
    {
        "glPopGroupMarkerEXT",
        reinterpret_cast<GLES2FunctionPointer>(glPopGroupMarkerEXT),
    },
    {
        "glGenVertexArraysOES",
        reinterpret_cast<GLES2FunctionPointer>(glGenVertexArraysOES),
    },
    {
        "glDeleteVertexArraysOES",
        reinterpret_cast<GLES2FunctionPointer>(glDeleteVertexArraysOES),
    },
    {
        "glIsVertexArrayOES",
        reinterpret_cast<GLES2FunctionPointer>(glIsVertexArrayOES),
    },
    {
        "glBindVertexArrayOES",
        reinterpret_cast<GLES2FunctionPointer>(glBindVertexArrayOES),
    },
    {
        "glFramebufferParameteri",
        reinterpret_cast<GLES2FunctionPointer>(glFramebufferParameteri),
    },
    {
        "glBindImageTexture",
        reinterpret_cast<GLES2FunctionPointer>(glBindImageTexture),
    },
    {
        "glDispatchCompute",
        reinterpret_cast<GLES2FunctionPointer>(glDispatchCompute),
    },
    {
        "glDispatchComputeIndirect",
        reinterpret_cast<GLES2FunctionPointer>(glDispatchComputeIndirect),
    },
    {
        "glDrawArraysIndirect",
        reinterpret_cast<GLES2FunctionPointer>(glDrawArraysIndirect),
    },
    {
        "glDrawElementsIndirect",
        reinterpret_cast<GLES2FunctionPointer>(glDrawElementsIndirect),
    },
    {
        "glGetProgramInterfaceiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetProgramInterfaceiv),
    },
    {
        "glGetProgramResourceIndex",
        reinterpret_cast<GLES2FunctionPointer>(glGetProgramResourceIndex),
    },
    {
        "glGetProgramResourceName",
        reinterpret_cast<GLES2FunctionPointer>(glGetProgramResourceName),
    },
    {
        "glGetProgramResourceiv",
        reinterpret_cast<GLES2FunctionPointer>(glGetProgramResourceiv),
    },
    {
        "glGetProgramResourceLocation",
        reinterpret_cast<GLES2FunctionPointer>(glGetProgramResourceLocation),
    },
    {
        "glMemoryBarrierEXT",
        reinterpret_cast<GLES2FunctionPointer>(glMemoryBarrierEXT),
    },
    {
        "glMemoryBarrierByRegion",
        reinterpret_cast<GLES2FunctionPointer>(glMemoryBarrierByRegion),
    },
    {
        "glSwapBuffers",
        reinterpret_cast<GLES2FunctionPointer>(glSwapBuffers),
    },
    {
        "glGetMaxValueInBufferCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glGetMaxValueInBufferCHROMIUM),
    },
    {
        "glEnableFeatureCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glEnableFeatureCHROMIUM),
    },
    {
        "glMapBufferCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glMapBufferCHROMIUM),
    },
    {
        "glUnmapBufferCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glUnmapBufferCHROMIUM),
    },
    {
        "glMapBufferSubDataCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glMapBufferSubDataCHROMIUM),
    },
    {
        "glUnmapBufferSubDataCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glUnmapBufferSubDataCHROMIUM),
    },
    {
        "glMapBufferRange",
        reinterpret_cast<GLES2FunctionPointer>(glMapBufferRange),
    },
    {
        "glUnmapBuffer",
        reinterpret_cast<GLES2FunctionPointer>(glUnmapBuffer),
    },
    {
        "glFlushMappedBufferRange",
        reinterpret_cast<GLES2FunctionPointer>(glFlushMappedBufferRange),
    },
    {
        "glMapTexSubImage2DCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glMapTexSubImage2DCHROMIUM),
    },
    {
        "glUnmapTexSubImage2DCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glUnmapTexSubImage2DCHROMIUM),
    },
    {
        "glResizeCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glResizeCHROMIUM),
    },
    {
        "glGetRequestableExtensionsCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glGetRequestableExtensionsCHROMIUM),
    },
    {
        "glRequestExtensionCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glRequestExtensionCHROMIUM),
    },
    {
        "glGetProgramInfoCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glGetProgramInfoCHROMIUM),
    },
    {
        "glGetUniformBlocksCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glGetUniformBlocksCHROMIUM),
    },
    {
        "glGetTransformFeedbackVaryingsCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glGetTransformFeedbackVaryingsCHROMIUM),
    },
    {
        "glGetUniformsES3CHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glGetUniformsES3CHROMIUM),
    },
    {
        "glDescheduleUntilFinishedCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glDescheduleUntilFinishedCHROMIUM),
    },
    {
        "glGetTranslatedShaderSourceANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glGetTranslatedShaderSourceANGLE),
    },
    {
        "glCopyTextureCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glCopyTextureCHROMIUM),
    },
    {
        "glCopySubTextureCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glCopySubTextureCHROMIUM),
    },
    {
        "glDrawArraysInstancedANGLE",
        reinterpret_cast<GLES2FunctionPointer>(glDrawArraysInstancedANGLE),
    },
    {
        "glDrawArraysInstancedBaseInstanceANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glDrawArraysInstancedBaseInstanceANGLE),
    },
    {
        "glDrawElementsInstancedANGLE",
        reinterpret_cast<GLES2FunctionPointer>(glDrawElementsInstancedANGLE),
    },
    {
        "glDrawElementsInstancedBaseVertexBaseInstanceANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glDrawElementsInstancedBaseVertexBaseInstanceANGLE),
    },
    {
        "glVertexAttribDivisorANGLE",
        reinterpret_cast<GLES2FunctionPointer>(glVertexAttribDivisorANGLE),
    },
    {
        "glBindUniformLocationCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glBindUniformLocationCHROMIUM),
    },
    {
        "glTraceBeginCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glTraceBeginCHROMIUM),
    },
    {
        "glTraceEndCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glTraceEndCHROMIUM),
    },
    {
        "glDiscardFramebufferEXT",
        reinterpret_cast<GLES2FunctionPointer>(glDiscardFramebufferEXT),
    },
    {
        "glLoseContextCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glLoseContextCHROMIUM),
    },
    {
        "glDrawBuffersEXT",
        reinterpret_cast<GLES2FunctionPointer>(glDrawBuffersEXT),
    },
    {
        "glFlushDriverCachesCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glFlushDriverCachesCHROMIUM),
    },
    {
        "glGetLastFlushIdCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glGetLastFlushIdCHROMIUM),
    },
    {
        "glSetActiveURLCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glSetActiveURLCHROMIUM),
    },
    {
        "glContextVisibilityHintCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glContextVisibilityHintCHROMIUM),
    },
    {
        "glGetGraphicsResetStatusKHR",
        reinterpret_cast<GLES2FunctionPointer>(glGetGraphicsResetStatusKHR),
    },
    {
        "glBlendBarrierKHR",
        reinterpret_cast<GLES2FunctionPointer>(glBlendBarrierKHR),
    },
    {
        "glBindFragDataLocationIndexedEXT",
        reinterpret_cast<GLES2FunctionPointer>(
            glBindFragDataLocationIndexedEXT),
    },
    {
        "glBindFragDataLocationEXT",
        reinterpret_cast<GLES2FunctionPointer>(glBindFragDataLocationEXT),
    },
    {
        "glGetFragDataIndexEXT",
        reinterpret_cast<GLES2FunctionPointer>(glGetFragDataIndexEXT),
    },
    {
        "glInitializeDiscardableTextureCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glInitializeDiscardableTextureCHROMIUM),
    },
    {
        "glUnlockDiscardableTextureCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glUnlockDiscardableTextureCHROMIUM),
    },
    {
        "glLockDiscardableTextureCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glLockDiscardableTextureCHROMIUM),
    },
    {
        "glWindowRectanglesEXT",
        reinterpret_cast<GLES2FunctionPointer>(glWindowRectanglesEXT),
    },
    {
        "glCreateGpuFenceCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glCreateGpuFenceCHROMIUM),
    },
    {
        "glCreateClientGpuFenceCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glCreateClientGpuFenceCHROMIUM),
    },
    {
        "glWaitGpuFenceCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glWaitGpuFenceCHROMIUM),
    },
    {
        "glDestroyGpuFenceCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(glDestroyGpuFenceCHROMIUM),
    },
    {
        "glInvalidateReadbackBufferShadowDataCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glInvalidateReadbackBufferShadowDataCHROMIUM),
    },
    {
        "glFramebufferTextureMultiviewOVR",
        reinterpret_cast<GLES2FunctionPointer>(
            glFramebufferTextureMultiviewOVR),
    },
    {
        "glMaxShaderCompilerThreadsKHR",
        reinterpret_cast<GLES2FunctionPointer>(glMaxShaderCompilerThreadsKHR),
    },
    {
        "glCreateAndTexStorage2DSharedImageCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glCreateAndTexStorage2DSharedImageCHROMIUM),
    },
    {
        "glBeginSharedImageAccessDirectCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glBeginSharedImageAccessDirectCHROMIUM),
    },
    {
        "glEndSharedImageAccessDirectCHROMIUM",
        reinterpret_cast<GLES2FunctionPointer>(
            glEndSharedImageAccessDirectCHROMIUM),
    },
    {
        "glCopySharedImageINTERNAL",
        reinterpret_cast<GLES2FunctionPointer>(glCopySharedImageINTERNAL),
    },
    {
        "glCopySharedImageToTextureINTERNAL",
        reinterpret_cast<GLES2FunctionPointer>(
            glCopySharedImageToTextureINTERNAL),
    },
    {
        "glReadbackARGBImagePixelsINTERNAL",
        reinterpret_cast<GLES2FunctionPointer>(
            glReadbackARGBImagePixelsINTERNAL),
    },
    {
        "glWritePixelsYUVINTERNAL",
        reinterpret_cast<GLES2FunctionPointer>(glWritePixelsYUVINTERNAL),
    },
    {
        "glEnableiOES",
        reinterpret_cast<GLES2FunctionPointer>(glEnableiOES),
    },
    {
        "glDisableiOES",
        reinterpret_cast<GLES2FunctionPointer>(glDisableiOES),
    },
    {
        "glBlendEquationiOES",
        reinterpret_cast<GLES2FunctionPointer>(glBlendEquationiOES),
    },
    {
        "glBlendEquationSeparateiOES",
        reinterpret_cast<GLES2FunctionPointer>(glBlendEquationSeparateiOES),
    },
    {
        "glBlendFunciOES",
        reinterpret_cast<GLES2FunctionPointer>(glBlendFunciOES),
    },
    {
        "glBlendFuncSeparateiOES",
        reinterpret_cast<GLES2FunctionPointer>(glBlendFuncSeparateiOES),
    },
    {
        "glColorMaskiOES",
        reinterpret_cast<GLES2FunctionPointer>(glColorMaskiOES),
    },
    {
        "glIsEnablediOES",
        reinterpret_cast<GLES2FunctionPointer>(glIsEnablediOES),
    },
    {
        "glProvokingVertexANGLE",
        reinterpret_cast<GLES2FunctionPointer>(glProvokingVertexANGLE),
    },
    {
        "glFramebufferMemorylessPixelLocalStorageANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glFramebufferMemorylessPixelLocalStorageANGLE),
    },
    {
        "glFramebufferTexturePixelLocalStorageANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glFramebufferTexturePixelLocalStorageANGLE),
    },
    {
        "glFramebufferPixelLocalClearValuefvANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glFramebufferPixelLocalClearValuefvANGLE),
    },
    {
        "glFramebufferPixelLocalClearValueivANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glFramebufferPixelLocalClearValueivANGLE),
    },
    {
        "glFramebufferPixelLocalClearValueuivANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glFramebufferPixelLocalClearValueuivANGLE),
    },
    {
        "glBeginPixelLocalStorageANGLE",
        reinterpret_cast<GLES2FunctionPointer>(glBeginPixelLocalStorageANGLE),
    },
    {
        "glEndPixelLocalStorageANGLE",
        reinterpret_cast<GLES2FunctionPointer>(glEndPixelLocalStorageANGLE),
    },
    {
        "glPixelLocalStorageBarrierANGLE",
        reinterpret_cast<GLES2FunctionPointer>(glPixelLocalStorageBarrierANGLE),
    },
    {
        "glFramebufferPixelLocalStorageInterruptANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glFramebufferPixelLocalStorageInterruptANGLE),
    },
    {
        "glFramebufferPixelLocalStorageRestoreANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glFramebufferPixelLocalStorageRestoreANGLE),
    },
    {
        "glGetFramebufferPixelLocalStorageParameterfvANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glGetFramebufferPixelLocalStorageParameterfvANGLE),
    },
    {
        "glGetFramebufferPixelLocalStorageParameterivANGLE",
        reinterpret_cast<GLES2FunctionPointer>(
            glGetFramebufferPixelLocalStorageParameterivANGLE),
    },
    {
        "glClipControlEXT",
        reinterpret_cast<GLES2FunctionPointer>(glClipControlEXT),
    },
    {
        "glPolygonModeANGLE",
        reinterpret_cast<GLES2FunctionPointer>(glPolygonModeANGLE),
    },
    {
        "glPolygonOffsetClampEXT",
        reinterpret_cast<GLES2FunctionPointer>(glPolygonOffsetClampEXT),
    },
    {
        nullptr,
        nullptr,
    },
};

}  // namespace gles2
#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_C_LIB_AUTOGEN_H_
