// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "ppapi/shared_impl/ppb_opengles2_shared.h"

#include "base/logging.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {

namespace {

typedef thunk::EnterResource<thunk::PPB_Graphics3D_API> Enter3D;

gpu::gles2::GLES2Implementation* ToGles2Impl(Enter3D* enter) {
  DCHECK(enter);
  DCHECK(enter->succeeded());
  return static_cast<PPB_Graphics3D_Shared*>(enter->object())->gles2_impl();
}

void ActiveTexture(PP_Resource context_id, GLenum texture) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ActiveTexture(texture);
  }
}

void AttachShader(PP_Resource context_id, GLuint program, GLuint shader) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->AttachShader(program, shader);
  }
}

void BindAttribLocation(PP_Resource context_id,
                        GLuint program,
                        GLuint index,
                        const char* name) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BindAttribLocation(program, index, name);
  }
}

void BindBuffer(PP_Resource context_id, GLenum target, GLuint buffer) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BindBuffer(target, buffer);
  }
}

void BindFramebuffer(PP_Resource context_id,
                     GLenum target,
                     GLuint framebuffer) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BindFramebuffer(target, framebuffer);
  }
}

void BindRenderbuffer(PP_Resource context_id,
                      GLenum target,
                      GLuint renderbuffer) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BindRenderbuffer(target, renderbuffer);
  }
}

void BindTexture(PP_Resource context_id, GLenum target, GLuint texture) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BindTexture(target, texture);
  }
}

void BlendColor(PP_Resource context_id,
                GLclampf red,
                GLclampf green,
                GLclampf blue,
                GLclampf alpha) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BlendColor(red, green, blue, alpha);
  }
}

void BlendEquation(PP_Resource context_id, GLenum mode) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BlendEquation(mode);
  }
}

void BlendEquationSeparate(PP_Resource context_id,
                           GLenum modeRGB,
                           GLenum modeAlpha) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BlendEquationSeparate(modeRGB, modeAlpha);
  }
}

void BlendFunc(PP_Resource context_id, GLenum sfactor, GLenum dfactor) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BlendFunc(sfactor, dfactor);
  }
}

void BlendFuncSeparate(PP_Resource context_id,
                       GLenum srcRGB,
                       GLenum dstRGB,
                       GLenum srcAlpha,
                       GLenum dstAlpha) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
  }
}

void BufferData(PP_Resource context_id,
                GLenum target,
                GLsizeiptr size,
                const void* data,
                GLenum usage) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BufferData(target, size, data, usage);
  }
}

void BufferSubData(PP_Resource context_id,
                   GLenum target,
                   GLintptr offset,
                   GLsizeiptr size,
                   const void* data) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BufferSubData(target, offset, size, data);
  }
}

GLenum CheckFramebufferStatus(PP_Resource context_id, GLenum target) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->CheckFramebufferStatus(target);
  } else {
    return 0;
  }
}

void Clear(PP_Resource context_id, GLbitfield mask) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Clear(mask);
  }
}

void ClearColor(PP_Resource context_id,
                GLclampf red,
                GLclampf green,
                GLclampf blue,
                GLclampf alpha) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ClearColor(red, green, blue, alpha);
  }
}

void ClearDepthf(PP_Resource context_id, GLclampf depth) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ClearDepthf(depth);
  }
}

void ClearStencil(PP_Resource context_id, GLint s) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ClearStencil(s);
  }
}

void ColorMask(PP_Resource context_id,
               GLboolean red,
               GLboolean green,
               GLboolean blue,
               GLboolean alpha) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ColorMask(red, green, blue, alpha);
  }
}

void CompileShader(PP_Resource context_id, GLuint shader) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->CompileShader(shader);
  }
}

void CompressedTexImage2D(PP_Resource context_id,
                          GLenum target,
                          GLint level,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLsizei imageSize,
                          const void* data) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->CompressedTexImage2D(
        target, level, internalformat, width, height, border, imageSize, data);
  }
}

void CompressedTexSubImage2D(PP_Resource context_id,
                             GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLsizei imageSize,
                             const void* data) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->CompressedTexSubImage2D(target, level, xoffset,
                                                 yoffset, width, height, format,
                                                 imageSize, data);
  }
}

void CopyTexImage2D(PP_Resource context_id,
                    GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height,
                    GLint border) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->CopyTexImage2D(target, level, internalformat, x, y,
                                        width, height, border);
  }
}

void CopyTexSubImage2D(PP_Resource context_id,
                       GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->CopyTexSubImage2D(target, level, xoffset, yoffset, x,
                                           y, width, height);
  }
}

GLuint CreateProgram(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->CreateProgram();
  } else {
    return 0;
  }
}

GLuint CreateShader(PP_Resource context_id, GLenum type) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->CreateShader(type);
  } else {
    return 0;
  }
}

void CullFace(PP_Resource context_id, GLenum mode) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->CullFace(mode);
  }
}

void DeleteBuffers(PP_Resource context_id, GLsizei n, const GLuint* buffers) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DeleteBuffers(n, buffers);
  }
}

void DeleteFramebuffers(PP_Resource context_id,
                        GLsizei n,
                        const GLuint* framebuffers) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DeleteFramebuffers(n, framebuffers);
  }
}

void DeleteProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DeleteProgram(program);
  }
}

void DeleteRenderbuffers(PP_Resource context_id,
                         GLsizei n,
                         const GLuint* renderbuffers) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DeleteRenderbuffers(n, renderbuffers);
  }
}

void DeleteShader(PP_Resource context_id, GLuint shader) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DeleteShader(shader);
  }
}

void DeleteTextures(PP_Resource context_id, GLsizei n, const GLuint* textures) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DeleteTextures(n, textures);
  }
}

void DepthFunc(PP_Resource context_id, GLenum func) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DepthFunc(func);
  }
}

void DepthMask(PP_Resource context_id, GLboolean flag) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DepthMask(flag);
  }
}

void DepthRangef(PP_Resource context_id, GLclampf zNear, GLclampf zFar) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DepthRangef(zNear, zFar);
  }
}

void DetachShader(PP_Resource context_id, GLuint program, GLuint shader) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DetachShader(program, shader);
  }
}

void Disable(PP_Resource context_id, GLenum cap) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Disable(cap);
  }
}

void DisableVertexAttribArray(PP_Resource context_id, GLuint index) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DisableVertexAttribArray(index);
  }
}

void DrawArrays(PP_Resource context_id,
                GLenum mode,
                GLint first,
                GLsizei count) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DrawArrays(mode, first, count);
  }
}

void DrawElements(PP_Resource context_id,
                  GLenum mode,
                  GLsizei count,
                  GLenum type,
                  const void* indices) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DrawElements(mode, count, type, indices);
  }
}

void Enable(PP_Resource context_id, GLenum cap) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Enable(cap);
  }
}

void EnableVertexAttribArray(PP_Resource context_id, GLuint index) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->EnableVertexAttribArray(index);
  }
}

void Finish(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Finish();
  }
}

void Flush(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Flush();
  }
}

void FramebufferRenderbuffer(PP_Resource context_id,
                             GLenum target,
                             GLenum attachment,
                             GLenum renderbuffertarget,
                             GLuint renderbuffer) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->FramebufferRenderbuffer(
        target, attachment, renderbuffertarget, renderbuffer);
  }
}

void FramebufferTexture2D(PP_Resource context_id,
                          GLenum target,
                          GLenum attachment,
                          GLenum textarget,
                          GLuint texture,
                          GLint level) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->FramebufferTexture2D(target, attachment, textarget,
                                              texture, level);
  }
}

void FrontFace(PP_Resource context_id, GLenum mode) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->FrontFace(mode);
  }
}

void GenBuffers(PP_Resource context_id, GLsizei n, GLuint* buffers) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GenBuffers(n, buffers);
  }
}

void GenerateMipmap(PP_Resource context_id, GLenum target) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GenerateMipmap(target);
  }
}

void GenFramebuffers(PP_Resource context_id, GLsizei n, GLuint* framebuffers) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GenFramebuffers(n, framebuffers);
  }
}

void GenRenderbuffers(PP_Resource context_id,
                      GLsizei n,
                      GLuint* renderbuffers) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GenRenderbuffers(n, renderbuffers);
  }
}

void GenTextures(PP_Resource context_id, GLsizei n, GLuint* textures) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GenTextures(n, textures);
  }
}

void GetActiveAttrib(PP_Resource context_id,
                     GLuint program,
                     GLuint index,
                     GLsizei bufsize,
                     GLsizei* length,
                     GLint* size,
                     GLenum* type,
                     char* name) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetActiveAttrib(program, index, bufsize, length, size,
                                         type, name);
  }
}

void GetActiveUniform(PP_Resource context_id,
                      GLuint program,
                      GLuint index,
                      GLsizei bufsize,
                      GLsizei* length,
                      GLint* size,
                      GLenum* type,
                      char* name) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetActiveUniform(program, index, bufsize, length, size,
                                          type, name);
  }
}

void GetAttachedShaders(PP_Resource context_id,
                        GLuint program,
                        GLsizei maxcount,
                        GLsizei* count,
                        GLuint* shaders) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetAttachedShaders(program, maxcount, count, shaders);
  }
}

GLint GetAttribLocation(PP_Resource context_id,
                        GLuint program,
                        const char* name) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->GetAttribLocation(program, name);
  } else {
    return -1;
  }
}

void GetBooleanv(PP_Resource context_id, GLenum pname, GLboolean* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetBooleanv(pname, params);
  }
}

void GetBufferParameteriv(PP_Resource context_id,
                          GLenum target,
                          GLenum pname,
                          GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetBufferParameteriv(target, pname, params);
  }
}

GLenum GetError(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->GetError();
  } else {
    return 0;
  }
}

void GetFloatv(PP_Resource context_id, GLenum pname, GLfloat* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetFloatv(pname, params);
  }
}

void GetFramebufferAttachmentParameteriv(PP_Resource context_id,
                                         GLenum target,
                                         GLenum attachment,
                                         GLenum pname,
                                         GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetFramebufferAttachmentParameteriv(target, attachment,
                                                             pname, params);
  }
}

void GetIntegerv(PP_Resource context_id, GLenum pname, GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetIntegerv(pname, params);
  }
}

void GetProgramiv(PP_Resource context_id,
                  GLuint program,
                  GLenum pname,
                  GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetProgramiv(program, pname, params);
  }
}

void GetProgramInfoLog(PP_Resource context_id,
                       GLuint program,
                       GLsizei bufsize,
                       GLsizei* length,
                       char* infolog) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetProgramInfoLog(program, bufsize, length, infolog);
  }
}

void GetRenderbufferParameteriv(PP_Resource context_id,
                                GLenum target,
                                GLenum pname,
                                GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetRenderbufferParameteriv(target, pname, params);
  }
}

void GetShaderiv(PP_Resource context_id,
                 GLuint shader,
                 GLenum pname,
                 GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetShaderiv(shader, pname, params);
  }
}

void GetShaderInfoLog(PP_Resource context_id,
                      GLuint shader,
                      GLsizei bufsize,
                      GLsizei* length,
                      char* infolog) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetShaderInfoLog(shader, bufsize, length, infolog);
  }
}

void GetShaderPrecisionFormat(PP_Resource context_id,
                              GLenum shadertype,
                              GLenum precisiontype,
                              GLint* range,
                              GLint* precision) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetShaderPrecisionFormat(shadertype, precisiontype,
                                                  range, precision);
  }
}

void GetShaderSource(PP_Resource context_id,
                     GLuint shader,
                     GLsizei bufsize,
                     GLsizei* length,
                     char* source) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetShaderSource(shader, bufsize, length, source);
  }
}

const GLubyte* GetString(PP_Resource context_id, GLenum name) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->GetString(name);
  } else {
    return nullptr;
  }
}

void GetTexParameterfv(PP_Resource context_id,
                       GLenum target,
                       GLenum pname,
                       GLfloat* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetTexParameterfv(target, pname, params);
  }
}

void GetTexParameteriv(PP_Resource context_id,
                       GLenum target,
                       GLenum pname,
                       GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetTexParameteriv(target, pname, params);
  }
}

void GetUniformfv(PP_Resource context_id,
                  GLuint program,
                  GLint location,
                  GLfloat* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetUniformfv(program, location, params);
  }
}

void GetUniformiv(PP_Resource context_id,
                  GLuint program,
                  GLint location,
                  GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetUniformiv(program, location, params);
  }
}

GLint GetUniformLocation(PP_Resource context_id,
                         GLuint program,
                         const char* name) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->GetUniformLocation(program, name);
  } else {
    return -1;
  }
}

void GetVertexAttribfv(PP_Resource context_id,
                       GLuint index,
                       GLenum pname,
                       GLfloat* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetVertexAttribfv(index, pname, params);
  }
}

void GetVertexAttribiv(PP_Resource context_id,
                       GLuint index,
                       GLenum pname,
                       GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetVertexAttribiv(index, pname, params);
  }
}

void GetVertexAttribPointerv(PP_Resource context_id,
                             GLuint index,
                             GLenum pname,
                             void** pointer) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetVertexAttribPointerv(index, pname, pointer);
  }
}

void Hint(PP_Resource context_id, GLenum target, GLenum mode) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Hint(target, mode);
  }
}

GLboolean IsBuffer(PP_Resource context_id, GLuint buffer) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->IsBuffer(buffer);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsEnabled(PP_Resource context_id, GLenum cap) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->IsEnabled(cap);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsFramebuffer(PP_Resource context_id, GLuint framebuffer) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->IsFramebuffer(framebuffer);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->IsProgram(program);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsRenderbuffer(PP_Resource context_id, GLuint renderbuffer) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->IsRenderbuffer(renderbuffer);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsShader(PP_Resource context_id, GLuint shader) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->IsShader(shader);
  } else {
    return GL_FALSE;
  }
}

GLboolean IsTexture(PP_Resource context_id, GLuint texture) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->IsTexture(texture);
  } else {
    return GL_FALSE;
  }
}

void LineWidth(PP_Resource context_id, GLfloat width) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->LineWidth(width);
  }
}

void LinkProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->LinkProgram(program);
  }
}

void PixelStorei(PP_Resource context_id, GLenum pname, GLint param) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->PixelStorei(pname, param);
  }
}

void PolygonOffset(PP_Resource context_id, GLfloat factor, GLfloat units) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->PolygonOffset(factor, units);
  }
}

void ReadPixels(PP_Resource context_id,
                GLint x,
                GLint y,
                GLsizei width,
                GLsizei height,
                GLenum format,
                GLenum type,
                void* pixels) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ReadPixels(x, y, width, height, format, type, pixels);
  }
}

void ReleaseShaderCompiler(PP_Resource context_id) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ReleaseShaderCompiler();
  }
}

void RenderbufferStorage(PP_Resource context_id,
                         GLenum target,
                         GLenum internalformat,
                         GLsizei width,
                         GLsizei height) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->RenderbufferStorage(target, internalformat, width,
                                             height);
  }
}

void SampleCoverage(PP_Resource context_id, GLclampf value, GLboolean invert) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->SampleCoverage(value, invert);
  }
}

void Scissor(PP_Resource context_id,
             GLint x,
             GLint y,
             GLsizei width,
             GLsizei height) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Scissor(x, y, width, height);
  }
}

void ShaderBinary(PP_Resource context_id,
                  GLsizei n,
                  const GLuint* shaders,
                  GLenum binaryformat,
                  const void* binary,
                  GLsizei length) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ShaderBinary(n, shaders, binaryformat, binary, length);
  }
}

void ShaderSource(PP_Resource context_id,
                  GLuint shader,
                  GLsizei count,
                  const char** str,
                  const GLint* length) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ShaderSource(shader, count, str, length);
  }
}

void StencilFunc(PP_Resource context_id, GLenum func, GLint ref, GLuint mask) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->StencilFunc(func, ref, mask);
  }
}

void StencilFuncSeparate(PP_Resource context_id,
                         GLenum face,
                         GLenum func,
                         GLint ref,
                         GLuint mask) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->StencilFuncSeparate(face, func, ref, mask);
  }
}

void StencilMask(PP_Resource context_id, GLuint mask) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->StencilMask(mask);
  }
}

void StencilMaskSeparate(PP_Resource context_id, GLenum face, GLuint mask) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->StencilMaskSeparate(face, mask);
  }
}

void StencilOp(PP_Resource context_id,
               GLenum fail,
               GLenum zfail,
               GLenum zpass) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->StencilOp(fail, zfail, zpass);
  }
}

void StencilOpSeparate(PP_Resource context_id,
                       GLenum face,
                       GLenum fail,
                       GLenum zfail,
                       GLenum zpass) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->StencilOpSeparate(face, fail, zfail, zpass);
  }
}

void TexImage2D(PP_Resource context_id,
                GLenum target,
                GLint level,
                GLint internalformat,
                GLsizei width,
                GLsizei height,
                GLint border,
                GLenum format,
                GLenum type,
                const void* pixels) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->TexImage2D(target, level, internalformat, width,
                                    height, border, format, type, pixels);
  }
}

void TexParameterf(PP_Resource context_id,
                   GLenum target,
                   GLenum pname,
                   GLfloat param) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->TexParameterf(target, pname, param);
  }
}

void TexParameterfv(PP_Resource context_id,
                    GLenum target,
                    GLenum pname,
                    const GLfloat* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->TexParameterfv(target, pname, params);
  }
}

void TexParameteri(PP_Resource context_id,
                   GLenum target,
                   GLenum pname,
                   GLint param) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->TexParameteri(target, pname, param);
  }
}

void TexParameteriv(PP_Resource context_id,
                    GLenum target,
                    GLenum pname,
                    const GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->TexParameteriv(target, pname, params);
  }
}

void TexSubImage2D(PP_Resource context_id,
                   GLenum target,
                   GLint level,
                   GLint xoffset,
                   GLint yoffset,
                   GLsizei width,
                   GLsizei height,
                   GLenum format,
                   GLenum type,
                   const void* pixels) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->TexSubImage2D(target, level, xoffset, yoffset, width,
                                       height, format, type, pixels);
  }
}

void Uniform1f(PP_Resource context_id, GLint location, GLfloat x) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform1f(location, x);
  }
}

void Uniform1fv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLfloat* v) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform1fv(location, count, v);
  }
}

void Uniform1i(PP_Resource context_id, GLint location, GLint x) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform1i(location, x);
  }
}

void Uniform1iv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLint* v) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform1iv(location, count, v);
  }
}

void Uniform2f(PP_Resource context_id, GLint location, GLfloat x, GLfloat y) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform2f(location, x, y);
  }
}

void Uniform2fv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLfloat* v) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform2fv(location, count, v);
  }
}

void Uniform2i(PP_Resource context_id, GLint location, GLint x, GLint y) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform2i(location, x, y);
  }
}

void Uniform2iv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLint* v) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform2iv(location, count, v);
  }
}

void Uniform3f(PP_Resource context_id,
               GLint location,
               GLfloat x,
               GLfloat y,
               GLfloat z) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform3f(location, x, y, z);
  }
}

void Uniform3fv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLfloat* v) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform3fv(location, count, v);
  }
}

void Uniform3i(PP_Resource context_id,
               GLint location,
               GLint x,
               GLint y,
               GLint z) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform3i(location, x, y, z);
  }
}

void Uniform3iv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLint* v) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform3iv(location, count, v);
  }
}

void Uniform4f(PP_Resource context_id,
               GLint location,
               GLfloat x,
               GLfloat y,
               GLfloat z,
               GLfloat w) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform4f(location, x, y, z, w);
  }
}

void Uniform4fv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLfloat* v) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform4fv(location, count, v);
  }
}

void Uniform4i(PP_Resource context_id,
               GLint location,
               GLint x,
               GLint y,
               GLint z,
               GLint w) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform4i(location, x, y, z, w);
  }
}

void Uniform4iv(PP_Resource context_id,
                GLint location,
                GLsizei count,
                const GLint* v) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Uniform4iv(location, count, v);
  }
}

void UniformMatrix2fv(PP_Resource context_id,
                      GLint location,
                      GLsizei count,
                      GLboolean transpose,
                      const GLfloat* value) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->UniformMatrix2fv(location, count, transpose, value);
  }
}

void UniformMatrix3fv(PP_Resource context_id,
                      GLint location,
                      GLsizei count,
                      GLboolean transpose,
                      const GLfloat* value) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->UniformMatrix3fv(location, count, transpose, value);
  }
}

void UniformMatrix4fv(PP_Resource context_id,
                      GLint location,
                      GLsizei count,
                      GLboolean transpose,
                      const GLfloat* value) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->UniformMatrix4fv(location, count, transpose, value);
  }
}

void UseProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->UseProgram(program);
  }
}

void ValidateProgram(PP_Resource context_id, GLuint program) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->ValidateProgram(program);
  }
}

void VertexAttrib1f(PP_Resource context_id, GLuint indx, GLfloat x) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttrib1f(indx, x);
  }
}

void VertexAttrib1fv(PP_Resource context_id,
                     GLuint indx,
                     const GLfloat* values) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttrib1fv(indx, values);
  }
}

void VertexAttrib2f(PP_Resource context_id, GLuint indx, GLfloat x, GLfloat y) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttrib2f(indx, x, y);
  }
}

void VertexAttrib2fv(PP_Resource context_id,
                     GLuint indx,
                     const GLfloat* values) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttrib2fv(indx, values);
  }
}

void VertexAttrib3f(PP_Resource context_id,
                    GLuint indx,
                    GLfloat x,
                    GLfloat y,
                    GLfloat z) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttrib3f(indx, x, y, z);
  }
}

void VertexAttrib3fv(PP_Resource context_id,
                     GLuint indx,
                     const GLfloat* values) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttrib3fv(indx, values);
  }
}

void VertexAttrib4f(PP_Resource context_id,
                    GLuint indx,
                    GLfloat x,
                    GLfloat y,
                    GLfloat z,
                    GLfloat w) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttrib4f(indx, x, y, z, w);
  }
}

void VertexAttrib4fv(PP_Resource context_id,
                     GLuint indx,
                     const GLfloat* values) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttrib4fv(indx, values);
  }
}

void VertexAttribPointer(PP_Resource context_id,
                         GLuint indx,
                         GLint size,
                         GLenum type,
                         GLboolean normalized,
                         GLsizei stride,
                         const void* ptr) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttribPointer(indx, size, type, normalized,
                                             stride, ptr);
  }
}

void Viewport(PP_Resource context_id,
              GLint x,
              GLint y,
              GLsizei width,
              GLsizei height) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->Viewport(x, y, width, height);
  }
}

void BlitFramebufferEXT(PP_Resource context_id,
                        GLint srcX0,
                        GLint srcY0,
                        GLint srcX1,
                        GLint srcY1,
                        GLint dstX0,
                        GLint dstY0,
                        GLint dstX1,
                        GLint dstY1,
                        GLbitfield mask,
                        GLenum filter) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BlitFramebufferCHROMIUM(
        srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  }
}

void RenderbufferStorageMultisampleEXT(PP_Resource context_id,
                                       GLenum target,
                                       GLsizei samples,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->RenderbufferStorageMultisampleCHROMIUM(
        target, samples, internalformat, width, height);
  }
}

void GenQueriesEXT(PP_Resource context_id, GLsizei n, GLuint* queries) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GenQueriesEXT(n, queries);
  }
}

void DeleteQueriesEXT(PP_Resource context_id,
                      GLsizei n,
                      const GLuint* queries) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DeleteQueriesEXT(n, queries);
  }
}

GLboolean IsQueryEXT(PP_Resource context_id, GLuint id) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->IsQueryEXT(id);
  } else {
    return GL_FALSE;
  }
}

void BeginQueryEXT(PP_Resource context_id, GLenum target, GLuint id) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BeginQueryEXT(target, id);
  }
}

void EndQueryEXT(PP_Resource context_id, GLenum target) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->EndQueryEXT(target);
  }
}

void GetQueryivEXT(PP_Resource context_id,
                   GLenum target,
                   GLenum pname,
                   GLint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetQueryivEXT(target, pname, params);
  }
}

void GetQueryObjectuivEXT(PP_Resource context_id,
                          GLuint id,
                          GLenum pname,
                          GLuint* params) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GetQueryObjectuivEXT(id, pname, params);
  }
}

void GenVertexArraysOES(PP_Resource context_id, GLsizei n, GLuint* arrays) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->GenVertexArraysOES(n, arrays);
  }
}

void DeleteVertexArraysOES(PP_Resource context_id,
                           GLsizei n,
                           const GLuint* arrays) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DeleteVertexArraysOES(n, arrays);
  }
}

GLboolean IsVertexArrayOES(PP_Resource context_id, GLuint array) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->IsVertexArrayOES(array);
  } else {
    return GL_FALSE;
  }
}

void BindVertexArrayOES(PP_Resource context_id, GLuint array) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->BindVertexArrayOES(array);
  }
}

GLboolean EnableFeatureCHROMIUM(PP_Resource context_id, const char* feature) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->EnableFeatureCHROMIUM(feature);
  } else {
    return GL_FALSE;
  }
}

void* MapBufferSubDataCHROMIUM(PP_Resource context_id,
                               GLuint target,
                               GLintptr offset,
                               GLsizeiptr size,
                               GLenum access) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->MapBufferSubDataCHROMIUM(target, offset, size,
                                                         access);
  } else {
    return nullptr;
  }
}

void UnmapBufferSubDataCHROMIUM(PP_Resource context_id, const void* mem) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->UnmapBufferSubDataCHROMIUM(mem);
  }
}

void* MapTexSubImage2DCHROMIUM(PP_Resource context_id,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               GLenum access) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    return ToGles2Impl(&enter)->MapTexSubImage2DCHROMIUM(
        target, level, xoffset, yoffset, width, height, format, type, access);
  } else {
    return nullptr;
  }
}

void UnmapTexSubImage2DCHROMIUM(PP_Resource context_id, const void* mem) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->UnmapTexSubImage2DCHROMIUM(mem);
  }
}

void DrawArraysInstancedANGLE(PP_Resource context_id,
                              GLenum mode,
                              GLint first,
                              GLsizei count,
                              GLsizei primcount) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DrawArraysInstancedANGLE(mode, first, count,
                                                  primcount);
  }
}

void DrawElementsInstancedANGLE(PP_Resource context_id,
                                GLenum mode,
                                GLsizei count,
                                GLenum type,
                                const void* indices,
                                GLsizei primcount) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DrawElementsInstancedANGLE(mode, count, type, indices,
                                                    primcount);
  }
}

void VertexAttribDivisorANGLE(PP_Resource context_id,
                              GLuint index,
                              GLuint divisor) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->VertexAttribDivisorANGLE(index, divisor);
  }
}

void DrawBuffersEXT(PP_Resource context_id, GLsizei count, const GLenum* bufs) {
  Enter3D enter(context_id, true);
  if (enter.succeeded()) {
    ToGles2Impl(&enter)->DrawBuffersEXT(count, bufs);
  }
}

}  // namespace
const PPB_OpenGLES2* PPB_OpenGLES2_Shared::GetInterface() {
  static const struct PPB_OpenGLES2 ppb_opengles2 = {
      &ActiveTexture,
      &AttachShader,
      &BindAttribLocation,
      &BindBuffer,
      &BindFramebuffer,
      &BindRenderbuffer,
      &BindTexture,
      &BlendColor,
      &BlendEquation,
      &BlendEquationSeparate,
      &BlendFunc,
      &BlendFuncSeparate,
      &BufferData,
      &BufferSubData,
      &CheckFramebufferStatus,
      &Clear,
      &ClearColor,
      &ClearDepthf,
      &ClearStencil,
      &ColorMask,
      &CompileShader,
      &CompressedTexImage2D,
      &CompressedTexSubImage2D,
      &CopyTexImage2D,
      &CopyTexSubImage2D,
      &CreateProgram,
      &CreateShader,
      &CullFace,
      &DeleteBuffers,
      &DeleteFramebuffers,
      &DeleteProgram,
      &DeleteRenderbuffers,
      &DeleteShader,
      &DeleteTextures,
      &DepthFunc,
      &DepthMask,
      &DepthRangef,
      &DetachShader,
      &Disable,
      &DisableVertexAttribArray,
      &DrawArrays,
      &DrawElements,
      &Enable,
      &EnableVertexAttribArray,
      &Finish,
      &Flush,
      &FramebufferRenderbuffer,
      &FramebufferTexture2D,
      &FrontFace,
      &GenBuffers,
      &GenerateMipmap,
      &GenFramebuffers,
      &GenRenderbuffers,
      &GenTextures,
      &GetActiveAttrib,
      &GetActiveUniform,
      &GetAttachedShaders,
      &GetAttribLocation,
      &GetBooleanv,
      &GetBufferParameteriv,
      &GetError,
      &GetFloatv,
      &GetFramebufferAttachmentParameteriv,
      &GetIntegerv,
      &GetProgramiv,
      &GetProgramInfoLog,
      &GetRenderbufferParameteriv,
      &GetShaderiv,
      &GetShaderInfoLog,
      &GetShaderPrecisionFormat,
      &GetShaderSource,
      &GetString,
      &GetTexParameterfv,
      &GetTexParameteriv,
      &GetUniformfv,
      &GetUniformiv,
      &GetUniformLocation,
      &GetVertexAttribfv,
      &GetVertexAttribiv,
      &GetVertexAttribPointerv,
      &Hint,
      &IsBuffer,
      &IsEnabled,
      &IsFramebuffer,
      &IsProgram,
      &IsRenderbuffer,
      &IsShader,
      &IsTexture,
      &LineWidth,
      &LinkProgram,
      &PixelStorei,
      &PolygonOffset,
      &ReadPixels,
      &ReleaseShaderCompiler,
      &RenderbufferStorage,
      &SampleCoverage,
      &Scissor,
      &ShaderBinary,
      &ShaderSource,
      &StencilFunc,
      &StencilFuncSeparate,
      &StencilMask,
      &StencilMaskSeparate,
      &StencilOp,
      &StencilOpSeparate,
      &TexImage2D,
      &TexParameterf,
      &TexParameterfv,
      &TexParameteri,
      &TexParameteriv,
      &TexSubImage2D,
      &Uniform1f,
      &Uniform1fv,
      &Uniform1i,
      &Uniform1iv,
      &Uniform2f,
      &Uniform2fv,
      &Uniform2i,
      &Uniform2iv,
      &Uniform3f,
      &Uniform3fv,
      &Uniform3i,
      &Uniform3iv,
      &Uniform4f,
      &Uniform4fv,
      &Uniform4i,
      &Uniform4iv,
      &UniformMatrix2fv,
      &UniformMatrix3fv,
      &UniformMatrix4fv,
      &UseProgram,
      &ValidateProgram,
      &VertexAttrib1f,
      &VertexAttrib1fv,
      &VertexAttrib2f,
      &VertexAttrib2fv,
      &VertexAttrib3f,
      &VertexAttrib3fv,
      &VertexAttrib4f,
      &VertexAttrib4fv,
      &VertexAttribPointer,
      &Viewport};
  return &ppb_opengles2;
}
const PPB_OpenGLES2InstancedArrays*
PPB_OpenGLES2_Shared::GetInstancedArraysInterface() {
  static const struct PPB_OpenGLES2InstancedArrays ppb_opengles2 = {
      &DrawArraysInstancedANGLE, &DrawElementsInstancedANGLE,
      &VertexAttribDivisorANGLE};
  return &ppb_opengles2;
}
const PPB_OpenGLES2FramebufferBlit*
PPB_OpenGLES2_Shared::GetFramebufferBlitInterface() {
  static const struct PPB_OpenGLES2FramebufferBlit ppb_opengles2 = {
      &BlitFramebufferEXT};
  return &ppb_opengles2;
}
const PPB_OpenGLES2FramebufferMultisample*
PPB_OpenGLES2_Shared::GetFramebufferMultisampleInterface() {
  static const struct PPB_OpenGLES2FramebufferMultisample ppb_opengles2 = {
      &RenderbufferStorageMultisampleEXT};
  return &ppb_opengles2;
}
const PPB_OpenGLES2ChromiumEnableFeature*
PPB_OpenGLES2_Shared::GetChromiumEnableFeatureInterface() {
  static const struct PPB_OpenGLES2ChromiumEnableFeature ppb_opengles2 = {
      &EnableFeatureCHROMIUM};
  return &ppb_opengles2;
}
const PPB_OpenGLES2ChromiumMapSub*
PPB_OpenGLES2_Shared::GetChromiumMapSubInterface() {
  static const struct PPB_OpenGLES2ChromiumMapSub ppb_opengles2 = {
      &MapBufferSubDataCHROMIUM, &UnmapBufferSubDataCHROMIUM,
      &MapTexSubImage2DCHROMIUM, &UnmapTexSubImage2DCHROMIUM};
  return &ppb_opengles2;
}
const PPB_OpenGLES2Query* PPB_OpenGLES2_Shared::GetQueryInterface() {
  static const struct PPB_OpenGLES2Query ppb_opengles2 = {
      &GenQueriesEXT, &DeleteQueriesEXT, &IsQueryEXT,          &BeginQueryEXT,
      &EndQueryEXT,   &GetQueryivEXT,    &GetQueryObjectuivEXT};
  return &ppb_opengles2;
}
const PPB_OpenGLES2VertexArrayObject*
PPB_OpenGLES2_Shared::GetVertexArrayObjectInterface() {
  static const struct PPB_OpenGLES2VertexArrayObject ppb_opengles2 = {
      &GenVertexArraysOES, &DeleteVertexArraysOES, &IsVertexArrayOES,
      &BindVertexArrayOES};
  return &ppb_opengles2;
}
const PPB_OpenGLES2DrawBuffers_Dev*
PPB_OpenGLES2_Shared::GetDrawBuffersInterface() {
  static const struct PPB_OpenGLES2DrawBuffers_Dev ppb_opengles2 = {
      &DrawBuffersEXT};
  return &ppb_opengles2;
}
}  // namespace ppapi
