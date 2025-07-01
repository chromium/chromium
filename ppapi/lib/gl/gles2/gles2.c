// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"

void GL_APIENTRY glActiveTexture(GLenum texture) {
  glGetInterfacePPAPI()->ActiveTexture(glGetCurrentContextPPAPI(), texture);
}

void GL_APIENTRY glAttachShader(GLuint program, GLuint shader) {
  glGetInterfacePPAPI()->AttachShader(glGetCurrentContextPPAPI(), program,
                                      shader);
}

void GL_APIENTRY glBindAttribLocation(GLuint program,
                                      GLuint index,
                                      const char* name) {
  glGetInterfacePPAPI()->BindAttribLocation(glGetCurrentContextPPAPI(), program,
                                            index, name);
}

void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer) {
  glGetInterfacePPAPI()->BindBuffer(glGetCurrentContextPPAPI(), target, buffer);
}

void GL_APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer) {
  glGetInterfacePPAPI()->BindFramebuffer(glGetCurrentContextPPAPI(), target,
                                         framebuffer);
}

void GL_APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
  glGetInterfacePPAPI()->BindRenderbuffer(glGetCurrentContextPPAPI(), target,
                                          renderbuffer);
}

void GL_APIENTRY glBindTexture(GLenum target, GLuint texture) {
  glGetInterfacePPAPI()->BindTexture(glGetCurrentContextPPAPI(), target,
                                     texture);
}

void GL_APIENTRY glBlendColor(GLclampf red,
                              GLclampf green,
                              GLclampf blue,
                              GLclampf alpha) {
  glGetInterfacePPAPI()->BlendColor(glGetCurrentContextPPAPI(), red, green,
                                    blue, alpha);
}

void GL_APIENTRY glBlendEquation(GLenum mode) {
  glGetInterfacePPAPI()->BlendEquation(glGetCurrentContextPPAPI(), mode);
}

void GL_APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  glGetInterfacePPAPI()->BlendEquationSeparate(glGetCurrentContextPPAPI(),
                                               modeRGB, modeAlpha);
}

void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) {
  glGetInterfacePPAPI()->BlendFunc(glGetCurrentContextPPAPI(), sfactor,
                                   dfactor);
}

void GL_APIENTRY glBlendFuncSeparate(GLenum srcRGB,
                                     GLenum dstRGB,
                                     GLenum srcAlpha,
                                     GLenum dstAlpha) {
  glGetInterfacePPAPI()->BlendFuncSeparate(glGetCurrentContextPPAPI(), srcRGB,
                                           dstRGB, srcAlpha, dstAlpha);
}

void GL_APIENTRY glBufferData(GLenum target,
                              GLsizeiptr size,
                              const void* data,
                              GLenum usage) {
  glGetInterfacePPAPI()->BufferData(glGetCurrentContextPPAPI(), target, size,
                                    data, usage);
}

void GL_APIENTRY glBufferSubData(GLenum target,
                                 GLintptr offset,
                                 GLsizeiptr size,
                                 const void* data) {
  glGetInterfacePPAPI()->BufferSubData(glGetCurrentContextPPAPI(), target,
                                       offset, size, data);
}

GLenum GL_APIENTRY glCheckFramebufferStatus(GLenum target) {
  return glGetInterfacePPAPI()->CheckFramebufferStatus(
      glGetCurrentContextPPAPI(), target);
}

void GL_APIENTRY glClear(GLbitfield mask) {
  glGetInterfacePPAPI()->Clear(glGetCurrentContextPPAPI(), mask);
}

void GL_APIENTRY glClearColor(GLclampf red,
                              GLclampf green,
                              GLclampf blue,
                              GLclampf alpha) {
  glGetInterfacePPAPI()->ClearColor(glGetCurrentContextPPAPI(), red, green,
                                    blue, alpha);
}

void GL_APIENTRY glClearDepthf(GLclampf depth) {
  glGetInterfacePPAPI()->ClearDepthf(glGetCurrentContextPPAPI(), depth);
}

void GL_APIENTRY glClearStencil(GLint s) {
  glGetInterfacePPAPI()->ClearStencil(glGetCurrentContextPPAPI(), s);
}

void GL_APIENTRY glColorMask(GLboolean red,
                             GLboolean green,
                             GLboolean blue,
                             GLboolean alpha) {
  glGetInterfacePPAPI()->ColorMask(glGetCurrentContextPPAPI(), red, green, blue,
                                   alpha);
}

void GL_APIENTRY glCompileShader(GLuint shader) {
  glGetInterfacePPAPI()->CompileShader(glGetCurrentContextPPAPI(), shader);
}

void GL_APIENTRY glCompressedTexImage2D(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border,
                                        GLsizei imageSize,
                                        const void* data) {
  glGetInterfacePPAPI()->CompressedTexImage2D(
      glGetCurrentContextPPAPI(), target, level, internalformat, width, height,
      border, imageSize, data);
}

void GL_APIENTRY glCompressedTexSubImage2D(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLsizei imageSize,
                                           const void* data) {
  glGetInterfacePPAPI()->CompressedTexSubImage2D(
      glGetCurrentContextPPAPI(), target, level, xoffset, yoffset, width,
      height, format, imageSize, data);
}

void GL_APIENTRY glCopyTexImage2D(GLenum target,
                                  GLint level,
                                  GLenum internalformat,
                                  GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height,
                                  GLint border) {
  glGetInterfacePPAPI()->CopyTexImage2D(glGetCurrentContextPPAPI(), target,
                                        level, internalformat, x, y, width,
                                        height, border);
}

void GL_APIENTRY glCopyTexSubImage2D(GLenum target,
                                     GLint level,
                                     GLint xoffset,
                                     GLint yoffset,
                                     GLint x,
                                     GLint y,
                                     GLsizei width,
                                     GLsizei height) {
  glGetInterfacePPAPI()->CopyTexSubImage2D(glGetCurrentContextPPAPI(), target,
                                           level, xoffset, yoffset, x, y, width,
                                           height);
}

GLuint GL_APIENTRY glCreateProgram() {
  return glGetInterfacePPAPI()->CreateProgram(glGetCurrentContextPPAPI());
}

GLuint GL_APIENTRY glCreateShader(GLenum type) {
  return glGetInterfacePPAPI()->CreateShader(glGetCurrentContextPPAPI(), type);
}

void GL_APIENTRY glCullFace(GLenum mode) {
  glGetInterfacePPAPI()->CullFace(glGetCurrentContextPPAPI(), mode);
}

void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint* buffers) {
  glGetInterfacePPAPI()->DeleteBuffers(glGetCurrentContextPPAPI(), n, buffers);
}

void GL_APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  glGetInterfacePPAPI()->DeleteFramebuffers(glGetCurrentContextPPAPI(), n,
                                            framebuffers);
}

void GL_APIENTRY glDeleteProgram(GLuint program) {
  glGetInterfacePPAPI()->DeleteProgram(glGetCurrentContextPPAPI(), program);
}

void GL_APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  glGetInterfacePPAPI()->DeleteRenderbuffers(glGetCurrentContextPPAPI(), n,
                                             renderbuffers);
}

void GL_APIENTRY glDeleteShader(GLuint shader) {
  glGetInterfacePPAPI()->DeleteShader(glGetCurrentContextPPAPI(), shader);
}

void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint* textures) {
  glGetInterfacePPAPI()->DeleteTextures(glGetCurrentContextPPAPI(), n,
                                        textures);
}

void GL_APIENTRY glDepthFunc(GLenum func) {
  glGetInterfacePPAPI()->DepthFunc(glGetCurrentContextPPAPI(), func);
}

void GL_APIENTRY glDepthMask(GLboolean flag) {
  glGetInterfacePPAPI()->DepthMask(glGetCurrentContextPPAPI(), flag);
}

void GL_APIENTRY glDepthRangef(GLclampf zNear, GLclampf zFar) {
  glGetInterfacePPAPI()->DepthRangef(glGetCurrentContextPPAPI(), zNear, zFar);
}

void GL_APIENTRY glDetachShader(GLuint program, GLuint shader) {
  glGetInterfacePPAPI()->DetachShader(glGetCurrentContextPPAPI(), program,
                                      shader);
}

void GL_APIENTRY glDisable(GLenum cap) {
  glGetInterfacePPAPI()->Disable(glGetCurrentContextPPAPI(), cap);
}

void GL_APIENTRY glDisableVertexAttribArray(GLuint index) {
  glGetInterfacePPAPI()->DisableVertexAttribArray(glGetCurrentContextPPAPI(),
                                                  index);
}

void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  glGetInterfacePPAPI()->DrawArrays(glGetCurrentContextPPAPI(), mode, first,
                                    count);
}

void GL_APIENTRY glDrawElements(GLenum mode,
                                GLsizei count,
                                GLenum type,
                                const void* indices) {
  glGetInterfacePPAPI()->DrawElements(glGetCurrentContextPPAPI(), mode, count,
                                      type, indices);
}

void GL_APIENTRY glEnable(GLenum cap) {
  glGetInterfacePPAPI()->Enable(glGetCurrentContextPPAPI(), cap);
}

void GL_APIENTRY glEnableVertexAttribArray(GLuint index) {
  glGetInterfacePPAPI()->EnableVertexAttribArray(glGetCurrentContextPPAPI(),
                                                 index);
}

void GL_APIENTRY glFinish() {
  glGetInterfacePPAPI()->Finish(glGetCurrentContextPPAPI());
}

void GL_APIENTRY glFlush() {
  glGetInterfacePPAPI()->Flush(glGetCurrentContextPPAPI());
}

void GL_APIENTRY glFramebufferRenderbuffer(GLenum target,
                                           GLenum attachment,
                                           GLenum renderbuffertarget,
                                           GLuint renderbuffer) {
  glGetInterfacePPAPI()->FramebufferRenderbuffer(
      glGetCurrentContextPPAPI(), target, attachment, renderbuffertarget,
      renderbuffer);
}

void GL_APIENTRY glFramebufferTexture2D(GLenum target,
                                        GLenum attachment,
                                        GLenum textarget,
                                        GLuint texture,
                                        GLint level) {
  glGetInterfacePPAPI()->FramebufferTexture2D(glGetCurrentContextPPAPI(),
                                              target, attachment, textarget,
                                              texture, level);
}

void GL_APIENTRY glFrontFace(GLenum mode) {
  glGetInterfacePPAPI()->FrontFace(glGetCurrentContextPPAPI(), mode);
}

void GL_APIENTRY glGenBuffers(GLsizei n, GLuint* buffers) {
  glGetInterfacePPAPI()->GenBuffers(glGetCurrentContextPPAPI(), n, buffers);
}

void GL_APIENTRY glGenerateMipmap(GLenum target) {
  glGetInterfacePPAPI()->GenerateMipmap(glGetCurrentContextPPAPI(), target);
}

void GL_APIENTRY glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  glGetInterfacePPAPI()->GenFramebuffers(glGetCurrentContextPPAPI(), n,
                                         framebuffers);
}

void GL_APIENTRY glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  glGetInterfacePPAPI()->GenRenderbuffers(glGetCurrentContextPPAPI(), n,
                                          renderbuffers);
}

void GL_APIENTRY glGenTextures(GLsizei n, GLuint* textures) {
  glGetInterfacePPAPI()->GenTextures(glGetCurrentContextPPAPI(), n, textures);
}

void GL_APIENTRY glGetActiveAttrib(GLuint program,
                                   GLuint index,
                                   GLsizei bufsize,
                                   GLsizei* length,
                                   GLint* size,
                                   GLenum* type,
                                   char* name) {
  glGetInterfacePPAPI()->GetActiveAttrib(glGetCurrentContextPPAPI(), program,
                                         index, bufsize, length, size, type,
                                         name);
}

void GL_APIENTRY glGetActiveUniform(GLuint program,
                                    GLuint index,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* size,
                                    GLenum* type,
                                    char* name) {
  glGetInterfacePPAPI()->GetActiveUniform(glGetCurrentContextPPAPI(), program,
                                          index, bufsize, length, size, type,
                                          name);
}

void GL_APIENTRY glGetAttachedShaders(GLuint program,
                                      GLsizei maxcount,
                                      GLsizei* count,
                                      GLuint* shaders) {
  glGetInterfacePPAPI()->GetAttachedShaders(glGetCurrentContextPPAPI(), program,
                                            maxcount, count, shaders);
}

GLint GL_APIENTRY glGetAttribLocation(GLuint program, const char* name) {
  return glGetInterfacePPAPI()->GetAttribLocation(glGetCurrentContextPPAPI(),
                                                  program, name);
}

void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean* params) {
  glGetInterfacePPAPI()->GetBooleanv(glGetCurrentContextPPAPI(), pname, params);
}

void GL_APIENTRY glGetBufferParameteriv(GLenum target,
                                        GLenum pname,
                                        GLint* params) {
  glGetInterfacePPAPI()->GetBufferParameteriv(glGetCurrentContextPPAPI(),
                                              target, pname, params);
}

GLenum GL_APIENTRY glGetError() {
  return glGetInterfacePPAPI()->GetError(glGetCurrentContextPPAPI());
}

void GL_APIENTRY glGetFloatv(GLenum pname, GLfloat* params) {
  glGetInterfacePPAPI()->GetFloatv(glGetCurrentContextPPAPI(), pname, params);
}

void GL_APIENTRY glGetFramebufferAttachmentParameteriv(GLenum target,
                                                       GLenum attachment,
                                                       GLenum pname,
                                                       GLint* params) {
  glGetInterfacePPAPI()->GetFramebufferAttachmentParameteriv(
      glGetCurrentContextPPAPI(), target, attachment, pname, params);
}

void GL_APIENTRY glGetIntegerv(GLenum pname, GLint* params) {
  glGetInterfacePPAPI()->GetIntegerv(glGetCurrentContextPPAPI(), pname, params);
}

void GL_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
  glGetInterfacePPAPI()->GetProgramiv(glGetCurrentContextPPAPI(), program,
                                      pname, params);
}

void GL_APIENTRY glGetProgramInfoLog(GLuint program,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* infolog) {
  glGetInterfacePPAPI()->GetProgramInfoLog(glGetCurrentContextPPAPI(), program,
                                           bufsize, length, infolog);
}

void GL_APIENTRY glGetRenderbufferParameteriv(GLenum target,
                                              GLenum pname,
                                              GLint* params) {
  glGetInterfacePPAPI()->GetRenderbufferParameteriv(glGetCurrentContextPPAPI(),
                                                    target, pname, params);
}

void GL_APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  glGetInterfacePPAPI()->GetShaderiv(glGetCurrentContextPPAPI(), shader, pname,
                                     params);
}

void GL_APIENTRY glGetShaderInfoLog(GLuint shader,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    char* infolog) {
  glGetInterfacePPAPI()->GetShaderInfoLog(glGetCurrentContextPPAPI(), shader,
                                          bufsize, length, infolog);
}

void GL_APIENTRY glGetShaderPrecisionFormat(GLenum shadertype,
                                            GLenum precisiontype,
                                            GLint* range,
                                            GLint* precision) {
  glGetInterfacePPAPI()->GetShaderPrecisionFormat(
      glGetCurrentContextPPAPI(), shadertype, precisiontype, range, precision);
}

void GL_APIENTRY glGetShaderSource(GLuint shader,
                                   GLsizei bufsize,
                                   GLsizei* length,
                                   char* source) {
  glGetInterfacePPAPI()->GetShaderSource(glGetCurrentContextPPAPI(), shader,
                                         bufsize, length, source);
}

const GLubyte* GL_APIENTRY glGetString(GLenum name) {
  return glGetInterfacePPAPI()->GetString(glGetCurrentContextPPAPI(), name);
}

void GL_APIENTRY glGetTexParameterfv(GLenum target,
                                     GLenum pname,
                                     GLfloat* params) {
  glGetInterfacePPAPI()->GetTexParameterfv(glGetCurrentContextPPAPI(), target,
                                           pname, params);
}

void GL_APIENTRY glGetTexParameteriv(GLenum target,
                                     GLenum pname,
                                     GLint* params) {
  glGetInterfacePPAPI()->GetTexParameteriv(glGetCurrentContextPPAPI(), target,
                                           pname, params);
}

void GL_APIENTRY glGetUniformfv(GLuint program,
                                GLint location,
                                GLfloat* params) {
  glGetInterfacePPAPI()->GetUniformfv(glGetCurrentContextPPAPI(), program,
                                      location, params);
}

void GL_APIENTRY glGetUniformiv(GLuint program, GLint location, GLint* params) {
  glGetInterfacePPAPI()->GetUniformiv(glGetCurrentContextPPAPI(), program,
                                      location, params);
}

GLint GL_APIENTRY glGetUniformLocation(GLuint program, const char* name) {
  return glGetInterfacePPAPI()->GetUniformLocation(glGetCurrentContextPPAPI(),
                                                   program, name);
}

void GL_APIENTRY glGetVertexAttribfv(GLuint index,
                                     GLenum pname,
                                     GLfloat* params) {
  glGetInterfacePPAPI()->GetVertexAttribfv(glGetCurrentContextPPAPI(), index,
                                           pname, params);
}

void GL_APIENTRY glGetVertexAttribiv(GLuint index,
                                     GLenum pname,
                                     GLint* params) {
  glGetInterfacePPAPI()->GetVertexAttribiv(glGetCurrentContextPPAPI(), index,
                                           pname, params);
}

void GL_APIENTRY glGetVertexAttribPointerv(GLuint index,
                                           GLenum pname,
                                           void** pointer) {
  glGetInterfacePPAPI()->GetVertexAttribPointerv(glGetCurrentContextPPAPI(),
                                                 index, pname, pointer);
}

void GL_APIENTRY glHint(GLenum target, GLenum mode) {
  glGetInterfacePPAPI()->Hint(glGetCurrentContextPPAPI(), target, mode);
}

GLboolean GL_APIENTRY glIsBuffer(GLuint buffer) {
  return glGetInterfacePPAPI()->IsBuffer(glGetCurrentContextPPAPI(), buffer);
}

GLboolean GL_APIENTRY glIsEnabled(GLenum cap) {
  return glGetInterfacePPAPI()->IsEnabled(glGetCurrentContextPPAPI(), cap);
}

GLboolean GL_APIENTRY glIsFramebuffer(GLuint framebuffer) {
  return glGetInterfacePPAPI()->IsFramebuffer(glGetCurrentContextPPAPI(),
                                              framebuffer);
}

GLboolean GL_APIENTRY glIsProgram(GLuint program) {
  return glGetInterfacePPAPI()->IsProgram(glGetCurrentContextPPAPI(), program);
}

GLboolean GL_APIENTRY glIsRenderbuffer(GLuint renderbuffer) {
  return glGetInterfacePPAPI()->IsRenderbuffer(glGetCurrentContextPPAPI(),
                                               renderbuffer);
}

GLboolean GL_APIENTRY glIsShader(GLuint shader) {
  return glGetInterfacePPAPI()->IsShader(glGetCurrentContextPPAPI(), shader);
}

GLboolean GL_APIENTRY glIsTexture(GLuint texture) {
  return glGetInterfacePPAPI()->IsTexture(glGetCurrentContextPPAPI(), texture);
}

void GL_APIENTRY glLineWidth(GLfloat width) {
  glGetInterfacePPAPI()->LineWidth(glGetCurrentContextPPAPI(), width);
}

void GL_APIENTRY glLinkProgram(GLuint program) {
  glGetInterfacePPAPI()->LinkProgram(glGetCurrentContextPPAPI(), program);
}

void GL_APIENTRY glPixelStorei(GLenum pname, GLint param) {
  glGetInterfacePPAPI()->PixelStorei(glGetCurrentContextPPAPI(), pname, param);
}

void GL_APIENTRY glPolygonOffset(GLfloat factor, GLfloat units) {
  glGetInterfacePPAPI()->PolygonOffset(glGetCurrentContextPPAPI(), factor,
                                       units);
}

void GL_APIENTRY glReadPixels(GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLenum format,
                              GLenum type,
                              void* pixels) {
  glGetInterfacePPAPI()->ReadPixels(glGetCurrentContextPPAPI(), x, y, width,
                                    height, format, type, pixels);
}

void GL_APIENTRY glReleaseShaderCompiler() {
  glGetInterfacePPAPI()->ReleaseShaderCompiler(glGetCurrentContextPPAPI());
}

void GL_APIENTRY glRenderbufferStorage(GLenum target,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height) {
  glGetInterfacePPAPI()->RenderbufferStorage(glGetCurrentContextPPAPI(), target,
                                             internalformat, width, height);
}

void GL_APIENTRY glSampleCoverage(GLclampf value, GLboolean invert) {
  glGetInterfacePPAPI()->SampleCoverage(glGetCurrentContextPPAPI(), value,
                                        invert);
}

void GL_APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  glGetInterfacePPAPI()->Scissor(glGetCurrentContextPPAPI(), x, y, width,
                                 height);
}

void GL_APIENTRY glShaderBinary(GLsizei n,
                                const GLuint* shaders,
                                GLenum binaryformat,
                                const void* binary,
                                GLsizei length) {
  glGetInterfacePPAPI()->ShaderBinary(glGetCurrentContextPPAPI(), n, shaders,
                                      binaryformat, binary, length);
}

void GL_APIENTRY glShaderSource(GLuint shader,
                                GLsizei count,
                                const char** str,
                                const GLint* length) {
  glGetInterfacePPAPI()->ShaderSource(glGetCurrentContextPPAPI(), shader, count,
                                      str, length);
}

void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask) {
  glGetInterfacePPAPI()->StencilFunc(glGetCurrentContextPPAPI(), func, ref,
                                     mask);
}

void GL_APIENTRY glStencilFuncSeparate(GLenum face,
                                       GLenum func,
                                       GLint ref,
                                       GLuint mask) {
  glGetInterfacePPAPI()->StencilFuncSeparate(glGetCurrentContextPPAPI(), face,
                                             func, ref, mask);
}

void GL_APIENTRY glStencilMask(GLuint mask) {
  glGetInterfacePPAPI()->StencilMask(glGetCurrentContextPPAPI(), mask);
}

void GL_APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask) {
  glGetInterfacePPAPI()->StencilMaskSeparate(glGetCurrentContextPPAPI(), face,
                                             mask);
}

void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  glGetInterfacePPAPI()->StencilOp(glGetCurrentContextPPAPI(), fail, zfail,
                                   zpass);
}

void GL_APIENTRY glStencilOpSeparate(GLenum face,
                                     GLenum fail,
                                     GLenum zfail,
                                     GLenum zpass) {
  glGetInterfacePPAPI()->StencilOpSeparate(glGetCurrentContextPPAPI(), face,
                                           fail, zfail, zpass);
}

void GL_APIENTRY glTexImage2D(GLenum target,
                              GLint level,
                              GLint internalformat,
                              GLsizei width,
                              GLsizei height,
                              GLint border,
                              GLenum format,
                              GLenum type,
                              const void* pixels) {
  glGetInterfacePPAPI()->TexImage2D(glGetCurrentContextPPAPI(), target, level,
                                    internalformat, width, height, border,
                                    format, type, pixels);
}

void GL_APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
  glGetInterfacePPAPI()->TexParameterf(glGetCurrentContextPPAPI(), target,
                                       pname, param);
}

void GL_APIENTRY glTexParameterfv(GLenum target,
                                  GLenum pname,
                                  const GLfloat* params) {
  glGetInterfacePPAPI()->TexParameterfv(glGetCurrentContextPPAPI(), target,
                                        pname, params);
}

void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {
  glGetInterfacePPAPI()->TexParameteri(glGetCurrentContextPPAPI(), target,
                                       pname, param);
}

void GL_APIENTRY glTexParameteriv(GLenum target,
                                  GLenum pname,
                                  const GLint* params) {
  glGetInterfacePPAPI()->TexParameteriv(glGetCurrentContextPPAPI(), target,
                                        pname, params);
}

void GL_APIENTRY glTexSubImage2D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLenum type,
                                 const void* pixels) {
  glGetInterfacePPAPI()->TexSubImage2D(glGetCurrentContextPPAPI(), target,
                                       level, xoffset, yoffset, width, height,
                                       format, type, pixels);
}

void GL_APIENTRY glUniform1f(GLint location, GLfloat x) {
  glGetInterfacePPAPI()->Uniform1f(glGetCurrentContextPPAPI(), location, x);
}

void GL_APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  glGetInterfacePPAPI()->Uniform1fv(glGetCurrentContextPPAPI(), location, count,
                                    v);
}

void GL_APIENTRY glUniform1i(GLint location, GLint x) {
  glGetInterfacePPAPI()->Uniform1i(glGetCurrentContextPPAPI(), location, x);
}

void GL_APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint* v) {
  glGetInterfacePPAPI()->Uniform1iv(glGetCurrentContextPPAPI(), location, count,
                                    v);
}

void GL_APIENTRY glUniform2f(GLint location, GLfloat x, GLfloat y) {
  glGetInterfacePPAPI()->Uniform2f(glGetCurrentContextPPAPI(), location, x, y);
}

void GL_APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  glGetInterfacePPAPI()->Uniform2fv(glGetCurrentContextPPAPI(), location, count,
                                    v);
}

void GL_APIENTRY glUniform2i(GLint location, GLint x, GLint y) {
  glGetInterfacePPAPI()->Uniform2i(glGetCurrentContextPPAPI(), location, x, y);
}

void GL_APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint* v) {
  glGetInterfacePPAPI()->Uniform2iv(glGetCurrentContextPPAPI(), location, count,
                                    v);
}

void GL_APIENTRY glUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  glGetInterfacePPAPI()->Uniform3f(glGetCurrentContextPPAPI(), location, x, y,
                                   z);
}

void GL_APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  glGetInterfacePPAPI()->Uniform3fv(glGetCurrentContextPPAPI(), location, count,
                                    v);
}

void GL_APIENTRY glUniform3i(GLint location, GLint x, GLint y, GLint z) {
  glGetInterfacePPAPI()->Uniform3i(glGetCurrentContextPPAPI(), location, x, y,
                                   z);
}

void GL_APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint* v) {
  glGetInterfacePPAPI()->Uniform3iv(glGetCurrentContextPPAPI(), location, count,
                                    v);
}

void GL_APIENTRY
glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  glGetInterfacePPAPI()->Uniform4f(glGetCurrentContextPPAPI(), location, x, y,
                                   z, w);
}

void GL_APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  glGetInterfacePPAPI()->Uniform4fv(glGetCurrentContextPPAPI(), location, count,
                                    v);
}

void GL_APIENTRY
glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  glGetInterfacePPAPI()->Uniform4i(glGetCurrentContextPPAPI(), location, x, y,
                                   z, w);
}

void GL_APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint* v) {
  glGetInterfacePPAPI()->Uniform4iv(glGetCurrentContextPPAPI(), location, count,
                                    v);
}

void GL_APIENTRY glUniformMatrix2fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) {
  glGetInterfacePPAPI()->UniformMatrix2fv(glGetCurrentContextPPAPI(), location,
                                          count, transpose, value);
}

void GL_APIENTRY glUniformMatrix3fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) {
  glGetInterfacePPAPI()->UniformMatrix3fv(glGetCurrentContextPPAPI(), location,
                                          count, transpose, value);
}

void GL_APIENTRY glUniformMatrix4fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat* value) {
  glGetInterfacePPAPI()->UniformMatrix4fv(glGetCurrentContextPPAPI(), location,
                                          count, transpose, value);
}

void GL_APIENTRY glUseProgram(GLuint program) {
  glGetInterfacePPAPI()->UseProgram(glGetCurrentContextPPAPI(), program);
}

void GL_APIENTRY glValidateProgram(GLuint program) {
  glGetInterfacePPAPI()->ValidateProgram(glGetCurrentContextPPAPI(), program);
}

void GL_APIENTRY glVertexAttrib1f(GLuint indx, GLfloat x) {
  glGetInterfacePPAPI()->VertexAttrib1f(glGetCurrentContextPPAPI(), indx, x);
}

void GL_APIENTRY glVertexAttrib1fv(GLuint indx, const GLfloat* values) {
  glGetInterfacePPAPI()->VertexAttrib1fv(glGetCurrentContextPPAPI(), indx,
                                         values);
}

void GL_APIENTRY glVertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  glGetInterfacePPAPI()->VertexAttrib2f(glGetCurrentContextPPAPI(), indx, x, y);
}

void GL_APIENTRY glVertexAttrib2fv(GLuint indx, const GLfloat* values) {
  glGetInterfacePPAPI()->VertexAttrib2fv(glGetCurrentContextPPAPI(), indx,
                                         values);
}

void GL_APIENTRY glVertexAttrib3f(GLuint indx,
                                  GLfloat x,
                                  GLfloat y,
                                  GLfloat z) {
  glGetInterfacePPAPI()->VertexAttrib3f(glGetCurrentContextPPAPI(), indx, x, y,
                                        z);
}

void GL_APIENTRY glVertexAttrib3fv(GLuint indx, const GLfloat* values) {
  glGetInterfacePPAPI()->VertexAttrib3fv(glGetCurrentContextPPAPI(), indx,
                                         values);
}

void GL_APIENTRY
glVertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  glGetInterfacePPAPI()->VertexAttrib4f(glGetCurrentContextPPAPI(), indx, x, y,
                                        z, w);
}

void GL_APIENTRY glVertexAttrib4fv(GLuint indx, const GLfloat* values) {
  glGetInterfacePPAPI()->VertexAttrib4fv(glGetCurrentContextPPAPI(), indx,
                                         values);
}

void GL_APIENTRY glVertexAttribPointer(GLuint indx,
                                       GLint size,
                                       GLenum type,
                                       GLboolean normalized,
                                       GLsizei stride,
                                       const void* ptr) {
  glGetInterfacePPAPI()->VertexAttribPointer(
      glGetCurrentContextPPAPI(), indx, size, type, normalized, stride, ptr);
}

void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  glGetInterfacePPAPI()->Viewport(glGetCurrentContextPPAPI(), x, y, width,
                                  height);
}

void GL_APIENTRY glBlitFramebufferEXT(GLint srcX0,
                                      GLint srcY0,
                                      GLint srcX1,
                                      GLint srcY1,
                                      GLint dstX0,
                                      GLint dstY0,
                                      GLint dstX1,
                                      GLint dstY1,
                                      GLbitfield mask,
                                      GLenum filter) {
  const struct PPB_OpenGLES2FramebufferBlit* ext =
      glGetFramebufferBlitInterfacePPAPI();
  if (ext)
    ext->BlitFramebufferEXT(glGetCurrentContextPPAPI(), srcX0, srcY0, srcX1,
                            srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void GL_APIENTRY glRenderbufferStorageMultisampleEXT(GLenum target,
                                                     GLsizei samples,
                                                     GLenum internalformat,
                                                     GLsizei width,
                                                     GLsizei height) {
  const struct PPB_OpenGLES2FramebufferMultisample* ext =
      glGetFramebufferMultisampleInterfacePPAPI();
  if (ext)
    ext->RenderbufferStorageMultisampleEXT(glGetCurrentContextPPAPI(), target,
                                           samples, internalformat, width,
                                           height);
}

void GL_APIENTRY glGenQueriesEXT(GLsizei n, GLuint* queries) {
  const struct PPB_OpenGLES2Query* ext = glGetQueryInterfacePPAPI();
  if (ext)
    ext->GenQueriesEXT(glGetCurrentContextPPAPI(), n, queries);
}

void GL_APIENTRY glDeleteQueriesEXT(GLsizei n, const GLuint* queries) {
  const struct PPB_OpenGLES2Query* ext = glGetQueryInterfacePPAPI();
  if (ext)
    ext->DeleteQueriesEXT(glGetCurrentContextPPAPI(), n, queries);
}

GLboolean GL_APIENTRY glIsQueryEXT(GLuint id) {
  const struct PPB_OpenGLES2Query* ext = glGetQueryInterfacePPAPI();
  if (ext)
    return ext->IsQueryEXT(glGetCurrentContextPPAPI(), id);
  return 0;
}

void GL_APIENTRY glBeginQueryEXT(GLenum target, GLuint id) {
  const struct PPB_OpenGLES2Query* ext = glGetQueryInterfacePPAPI();
  if (ext)
    ext->BeginQueryEXT(glGetCurrentContextPPAPI(), target, id);
}

void GL_APIENTRY glEndQueryEXT(GLenum target) {
  const struct PPB_OpenGLES2Query* ext = glGetQueryInterfacePPAPI();
  if (ext)
    ext->EndQueryEXT(glGetCurrentContextPPAPI(), target);
}

void GL_APIENTRY glGetQueryivEXT(GLenum target, GLenum pname, GLint* params) {
  const struct PPB_OpenGLES2Query* ext = glGetQueryInterfacePPAPI();
  if (ext)
    ext->GetQueryivEXT(glGetCurrentContextPPAPI(), target, pname, params);
}

void GL_APIENTRY glGetQueryObjectuivEXT(GLuint id,
                                        GLenum pname,
                                        GLuint* params) {
  const struct PPB_OpenGLES2Query* ext = glGetQueryInterfacePPAPI();
  if (ext)
    ext->GetQueryObjectuivEXT(glGetCurrentContextPPAPI(), id, pname, params);
}

void GL_APIENTRY glGenVertexArraysOES(GLsizei n, GLuint* arrays) {
  const struct PPB_OpenGLES2VertexArrayObject* ext =
      glGetVertexArrayObjectInterfacePPAPI();
  if (ext)
    ext->GenVertexArraysOES(glGetCurrentContextPPAPI(), n, arrays);
}

void GL_APIENTRY glDeleteVertexArraysOES(GLsizei n, const GLuint* arrays) {
  const struct PPB_OpenGLES2VertexArrayObject* ext =
      glGetVertexArrayObjectInterfacePPAPI();
  if (ext)
    ext->DeleteVertexArraysOES(glGetCurrentContextPPAPI(), n, arrays);
}

GLboolean GL_APIENTRY glIsVertexArrayOES(GLuint array) {
  const struct PPB_OpenGLES2VertexArrayObject* ext =
      glGetVertexArrayObjectInterfacePPAPI();
  if (ext)
    return ext->IsVertexArrayOES(glGetCurrentContextPPAPI(), array);
  return 0;
}

void GL_APIENTRY glBindVertexArrayOES(GLuint array) {
  const struct PPB_OpenGLES2VertexArrayObject* ext =
      glGetVertexArrayObjectInterfacePPAPI();
  if (ext)
    ext->BindVertexArrayOES(glGetCurrentContextPPAPI(), array);
}

GLboolean GL_APIENTRY glEnableFeatureCHROMIUM(const char* feature) {
  const struct PPB_OpenGLES2ChromiumEnableFeature* ext =
      glGetChromiumEnableFeatureInterfacePPAPI();
  if (ext)
    return ext->EnableFeatureCHROMIUM(glGetCurrentContextPPAPI(), feature);
  return 0;
}

void* GL_APIENTRY glMapBufferSubDataCHROMIUM(GLuint target,
                                             GLintptr offset,
                                             GLsizeiptr size,
                                             GLenum access) {
  const struct PPB_OpenGLES2ChromiumMapSub* ext =
      glGetChromiumMapSubInterfacePPAPI();
  if (ext)
    return ext->MapBufferSubDataCHROMIUM(glGetCurrentContextPPAPI(), target,
                                         offset, size, access);
  return 0;
}

void GL_APIENTRY glUnmapBufferSubDataCHROMIUM(const void* mem) {
  const struct PPB_OpenGLES2ChromiumMapSub* ext =
      glGetChromiumMapSubInterfacePPAPI();
  if (ext)
    ext->UnmapBufferSubDataCHROMIUM(glGetCurrentContextPPAPI(), mem);
}

void* GL_APIENTRY glMapTexSubImage2DCHROMIUM(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLenum type,
                                             GLenum access) {
  const struct PPB_OpenGLES2ChromiumMapSub* ext =
      glGetChromiumMapSubInterfacePPAPI();
  if (ext)
    return ext->MapTexSubImage2DCHROMIUM(glGetCurrentContextPPAPI(), target,
                                         level, xoffset, yoffset, width, height,
                                         format, type, access);
  return 0;
}

void GL_APIENTRY glUnmapTexSubImage2DCHROMIUM(const void* mem) {
  const struct PPB_OpenGLES2ChromiumMapSub* ext =
      glGetChromiumMapSubInterfacePPAPI();
  if (ext)
    ext->UnmapTexSubImage2DCHROMIUM(glGetCurrentContextPPAPI(), mem);
}

void GL_APIENTRY glDrawArraysInstancedANGLE(GLenum mode,
                                            GLint first,
                                            GLsizei count,
                                            GLsizei primcount) {
  const struct PPB_OpenGLES2InstancedArrays* ext =
      glGetInstancedArraysInterfacePPAPI();
  if (ext)
    ext->DrawArraysInstancedANGLE(glGetCurrentContextPPAPI(), mode, first,
                                  count, primcount);
}

void GL_APIENTRY glDrawElementsInstancedANGLE(GLenum mode,
                                              GLsizei count,
                                              GLenum type,
                                              const void* indices,
                                              GLsizei primcount) {
  const struct PPB_OpenGLES2InstancedArrays* ext =
      glGetInstancedArraysInterfacePPAPI();
  if (ext)
    ext->DrawElementsInstancedANGLE(glGetCurrentContextPPAPI(), mode, count,
                                    type, indices, primcount);
}

void GL_APIENTRY glVertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
  const struct PPB_OpenGLES2InstancedArrays* ext =
      glGetInstancedArraysInterfacePPAPI();
  if (ext)
    ext->VertexAttribDivisorANGLE(glGetCurrentContextPPAPI(), index, divisor);
}

void GL_APIENTRY glDrawBuffersEXT(GLsizei count, const GLenum* bufs) {
  const struct PPB_OpenGLES2DrawBuffers_Dev* ext =
      glGetDrawBuffersInterfacePPAPI();
  if (ext)
    ext->DrawBuffersEXT(glGetCurrentContextPPAPI(), count, bufs);
}
