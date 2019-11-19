// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// This file is included by gles2_trace_implementation.h
#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_TRACE_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_TRACE_IMPLEMENTATION_AUTOGEN_H_

void ActiveTexture(GLenum texture) override;
void AttachShader(GLuint program, GLuint shader) override;
void BindAttribLocation(GLuint program,
                        GLuint index,
                        const char* name) override;
void BindBuffer(GLenum target, GLuint buffer) override;
void BindBufferBase(GLenum target, GLuint index, GLuint buffer) override;
void BindBufferRange(GLenum target,
                     GLuint index,
                     GLuint buffer,
                     GLintptr offset,
                     GLsizeiptr size) override;
void BindFramebuffer(GLenum target, GLuint framebuffer) override;
void BindRenderbuffer(GLenum target, GLuint renderbuffer) override;
void BindSampler(GLuint unit, GLuint sampler) override;
void BindTexture(GLenum target, GLuint texture) override;
void BindTransformFeedback(GLenum target, GLuint transformfeedback) override;
void BlendColor(GLclampf red,
                GLclampf green,
                GLclampf blue,
                GLclampf alpha) override;
void BlendEquation(GLenum mode) override;
void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) override;
void BlendFunc(GLenum sfactor, GLenum dfactor) override;
void BlendFuncSeparate(GLenum srcRGB,
                       GLenum dstRGB,
                       GLenum srcAlpha,
                       GLenum dstAlpha) override;
void BufferData(GLenum target,
                GLsizeiptr size,
                const void* data,
                GLenum usage) override;
void BufferSubData(GLenum target,
                   GLintptr offset,
                   GLsizeiptr size,
                   const void* data) override;
GLenum CheckFramebufferStatus(GLenum target) override;
void Clear(GLbitfield mask) override;
void ClearBufferfi(GLenum buffer,
                   GLint drawbuffers,
                   GLfloat depth,
                   GLint stencil) override;
void ClearBufferfv(GLenum buffer,
                   GLint drawbuffers,
                   const GLfloat* value) override;
void ClearBufferiv(GLenum buffer,
                   GLint drawbuffers,
                   const GLint* value) override;
void ClearBufferuiv(GLenum buffer,
                    GLint drawbuffers,
                    const GLuint* value) override;
void ClearColor(GLclampf red,
                GLclampf green,
                GLclampf blue,
                GLclampf alpha) override;
void ClearDepthf(GLclampf depth) override;
void ClearStencil(GLint s) override;
GLenum ClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) override;
void ColorMask(GLboolean red,
               GLboolean green,
               GLboolean blue,
               GLboolean alpha) override;
void CompileShader(GLuint shader) override;
void CompressedTexImage2D(GLenum target,
                          GLint level,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLsizei imageSize,
                          const void* data) override;
void CompressedTexSubImage2D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLsizei imageSize,
                             const void* data) override;
void CompressedTexImage3D(GLenum target,
                          GLint level,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei depth,
                          GLint border,
                          GLsizei imageSize,
                          const void* data) override;
void CompressedTexSubImage3D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLint zoffset,
                             GLsizei width,
                             GLsizei height,
                             GLsizei depth,
                             GLenum format,
                             GLsizei imageSize,
                             const void* data) override;
void CopyBufferSubData(GLenum readtarget,
                       GLenum writetarget,
                       GLintptr readoffset,
                       GLintptr writeoffset,
                       GLsizeiptr size) override;
void CopyTexImage2D(GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height,
                    GLint border) override;
void CopyTexSubImage2D(GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height) override;
void CopyTexSubImage3D(GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLint zoffset,
                       GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height) override;
GLuint CreateProgram() override;
GLuint CreateShader(GLenum type) override;
void CullFace(GLenum mode) override;
void DeleteBuffers(GLsizei n, const GLuint* buffers) override;
void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) override;
void DeleteProgram(GLuint program) override;
void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) override;
void DeleteSamplers(GLsizei n, const GLuint* samplers) override;
void DeleteSync(GLsync sync) override;
void DeleteShader(GLuint shader) override;
void DeleteTextures(GLsizei n, const GLuint* textures) override;
void DeleteTransformFeedbacks(GLsizei n, const GLuint* ids) override;
void DepthFunc(GLenum func) override;
void DepthMask(GLboolean flag) override;
void DepthRangef(GLclampf zNear, GLclampf zFar) override;
void DetachShader(GLuint program, GLuint shader) override;
void Disable(GLenum cap) override;
void DisableVertexAttribArray(GLuint index) override;
void DrawArrays(GLenum mode, GLint first, GLsizei count) override;
void DrawElements(GLenum mode,
                  GLsizei count,
                  GLenum type,
                  const void* indices) override;
void DrawRangeElements(GLenum mode,
                       GLuint start,
                       GLuint end,
                       GLsizei count,
                       GLenum type,
                       const void* indices) override;
void Enable(GLenum cap) override;
void EnableVertexAttribArray(GLuint index) override;
GLsync FenceSync(GLenum condition, GLbitfield flags) override;
void Finish() override;
void Flush() override;
void FramebufferRenderbuffer(GLenum target,
                             GLenum attachment,
                             GLenum renderbuffertarget,
                             GLuint renderbuffer) override;
void FramebufferTexture2D(GLenum target,
                          GLenum attachment,
                          GLenum textarget,
                          GLuint texture,
                          GLint level) override;
void FramebufferTextureLayer(GLenum target,
                             GLenum attachment,
                             GLuint texture,
                             GLint level,
                             GLint layer) override;
void FrontFace(GLenum mode) override;
void GenBuffers(GLsizei n, GLuint* buffers) override;
void GenerateMipmap(GLenum target) override;
void GenFramebuffers(GLsizei n, GLuint* framebuffers) override;
void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override;
void GenSamplers(GLsizei n, GLuint* samplers) override;
void GenTextures(GLsizei n, GLuint* textures) override;
void GenTransformFeedbacks(GLsizei n, GLuint* ids) override;
void GetActiveAttrib(GLuint program,
                     GLuint index,
                     GLsizei bufsize,
                     GLsizei* length,
                     GLint* size,
                     GLenum* type,
                     char* name) override;
void GetActiveUniform(GLuint program,
                      GLuint index,
                      GLsizei bufsize,
                      GLsizei* length,
                      GLint* size,
                      GLenum* type,
                      char* name) override;
void GetActiveUniformBlockiv(GLuint program,
                             GLuint index,
                             GLenum pname,
                             GLint* params) override;
void GetActiveUniformBlockName(GLuint program,
                               GLuint index,
                               GLsizei bufsize,
                               GLsizei* length,
                               char* name) override;
void GetActiveUniformsiv(GLuint program,
                         GLsizei count,
                         const GLuint* indices,
                         GLenum pname,
                         GLint* params) override;
void GetAttachedShaders(GLuint program,
                        GLsizei maxcount,
                        GLsizei* count,
                        GLuint* shaders) override;
GLint GetAttribLocation(GLuint program, const char* name) override;
void GetBooleanv(GLenum pname, GLboolean* params) override;
void GetBufferParameteri64v(GLenum target,
                            GLenum pname,
                            GLint64* params) override;
void GetBufferParameteriv(GLenum target, GLenum pname, GLint* params) override;
GLenum GetError() override;
void GetFloatv(GLenum pname, GLfloat* params) override;
GLint GetFragDataLocation(GLuint program, const char* name) override;
void GetFramebufferAttachmentParameteriv(GLenum target,
                                         GLenum attachment,
                                         GLenum pname,
                                         GLint* params) override;
void GetInteger64v(GLenum pname, GLint64* params) override;
void GetIntegeri_v(GLenum pname, GLuint index, GLint* data) override;
void GetInteger64i_v(GLenum pname, GLuint index, GLint64* data) override;
void GetIntegerv(GLenum pname, GLint* params) override;
void GetInternalformativ(GLenum target,
                         GLenum format,
                         GLenum pname,
                         GLsizei bufSize,
                         GLint* params) override;
void GetProgramiv(GLuint program, GLenum pname, GLint* params) override;
void GetProgramInfoLog(GLuint program,
                       GLsizei bufsize,
                       GLsizei* length,
                       char* infolog) override;
void GetRenderbufferParameteriv(GLenum target,
                                GLenum pname,
                                GLint* params) override;
void GetSamplerParameterfv(GLuint sampler,
                           GLenum pname,
                           GLfloat* params) override;
void GetSamplerParameteriv(GLuint sampler,
                           GLenum pname,
                           GLint* params) override;
void GetShaderiv(GLuint shader, GLenum pname, GLint* params) override;
void GetShaderInfoLog(GLuint shader,
                      GLsizei bufsize,
                      GLsizei* length,
                      char* infolog) override;
void GetShaderPrecisionFormat(GLenum shadertype,
                              GLenum precisiontype,
                              GLint* range,
                              GLint* precision) override;
void GetShaderSource(GLuint shader,
                     GLsizei bufsize,
                     GLsizei* length,
                     char* source) override;
const GLubyte* GetString(GLenum name) override;
const GLubyte* GetStringi(GLenum name, GLuint index) override;
void GetSynciv(GLsync sync,
               GLenum pname,
               GLsizei bufsize,
               GLsizei* length,
               GLint* values) override;
void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) override;
void GetTexParameteriv(GLenum target, GLenum pname, GLint* params) override;
void GetTransformFeedbackVarying(GLuint program,
                                 GLuint index,
                                 GLsizei bufsize,
                                 GLsizei* length,
                                 GLsizei* size,
                                 GLenum* type,
                                 char* name) override;
GLuint GetUniformBlockIndex(GLuint program, const char* name) override;
void GetUniformfv(GLuint program, GLint location, GLfloat* params) override;
void GetUniformiv(GLuint program, GLint location, GLint* params) override;
void GetUniformuiv(GLuint program, GLint location, GLuint* params) override;
void GetUniformIndices(GLuint program,
                       GLsizei count,
                       const char* const* names,
                       GLuint* indices) override;
GLint GetUniformLocation(GLuint program, const char* name) override;
void GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) override;
void GetVertexAttribiv(GLuint index, GLenum pname, GLint* params) override;
void GetVertexAttribIiv(GLuint index, GLenum pname, GLint* params) override;
void GetVertexAttribIuiv(GLuint index, GLenum pname, GLuint* params) override;
void GetVertexAttribPointerv(GLuint index,
                             GLenum pname,
                             void** pointer) override;
void Hint(GLenum target, GLenum mode) override;
void InvalidateFramebuffer(GLenum target,
                           GLsizei count,
                           const GLenum* attachments) override;
void InvalidateSubFramebuffer(GLenum target,
                              GLsizei count,
                              const GLenum* attachments,
                              GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height) override;
GLboolean IsBuffer(GLuint buffer) override;
GLboolean IsEnabled(GLenum cap) override;
GLboolean IsFramebuffer(GLuint framebuffer) override;
GLboolean IsProgram(GLuint program) override;
GLboolean IsRenderbuffer(GLuint renderbuffer) override;
GLboolean IsSampler(GLuint sampler) override;
GLboolean IsShader(GLuint shader) override;
GLboolean IsSync(GLsync sync) override;
GLboolean IsTexture(GLuint texture) override;
GLboolean IsTransformFeedback(GLuint transformfeedback) override;
void LineWidth(GLfloat width) override;
void LinkProgram(GLuint program) override;
void PauseTransformFeedback() override;
void PixelStorei(GLenum pname, GLint param) override;
void PolygonOffset(GLfloat factor, GLfloat units) override;
void ReadBuffer(GLenum src) override;
void ReadPixels(GLint x,
                GLint y,
                GLsizei width,
                GLsizei height,
                GLenum format,
                GLenum type,
                void* pixels) override;
void ReleaseShaderCompiler() override;
void RenderbufferStorage(GLenum target,
                         GLenum internalformat,
                         GLsizei width,
                         GLsizei height) override;
void ResumeTransformFeedback() override;
void SampleCoverage(GLclampf value, GLboolean invert) override;
void SamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) override;
void SamplerParameterfv(GLuint sampler,
                        GLenum pname,
                        const GLfloat* params) override;
void SamplerParameteri(GLuint sampler, GLenum pname, GLint param) override;
void SamplerParameteriv(GLuint sampler,
                        GLenum pname,
                        const GLint* params) override;
void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) override;
void ShaderBinary(GLsizei n,
                  const GLuint* shaders,
                  GLenum binaryformat,
                  const void* binary,
                  GLsizei length) override;
void ShaderSource(GLuint shader,
                  GLsizei count,
                  const GLchar* const* str,
                  const GLint* length) override;
void ShallowFinishCHROMIUM() override;
void ShallowFlushCHROMIUM() override;
void OrderingBarrierCHROMIUM() override;
void MultiDrawArraysWEBGL(GLenum mode,
                          const GLint* firsts,
                          const GLsizei* counts,
                          GLsizei drawcount) override;
void MultiDrawArraysInstancedWEBGL(GLenum mode,
                                   const GLint* firsts,
                                   const GLsizei* counts,
                                   const GLsizei* instance_counts,
                                   GLsizei drawcount) override;
void MultiDrawArraysInstancedBaseInstanceWEBGL(GLenum mode,
                                               const GLint* firsts,
                                               const GLsizei* counts,
                                               const GLsizei* instance_counts,
                                               const GLuint* baseinstances,
                                               GLsizei drawcount) override;
void MultiDrawElementsWEBGL(GLenum mode,
                            const GLsizei* counts,
                            GLenum type,
                            const GLsizei* offsets,
                            GLsizei drawcount) override;
void MultiDrawElementsInstancedWEBGL(GLenum mode,
                                     const GLsizei* counts,
                                     GLenum type,
                                     const GLsizei* offsets,
                                     const GLsizei* instance_counts,
                                     GLsizei drawcount) override;
void MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLsizei* offsets,
    const GLsizei* instance_counts,
    const GLint* basevertices,
    const GLuint* baseinstances,
    GLsizei drawcount) override;
void StencilFunc(GLenum func, GLint ref, GLuint mask) override;
void StencilFuncSeparate(GLenum face,
                         GLenum func,
                         GLint ref,
                         GLuint mask) override;
void StencilMask(GLuint mask) override;
void StencilMaskSeparate(GLenum face, GLuint mask) override;
void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) override;
void StencilOpSeparate(GLenum face,
                       GLenum fail,
                       GLenum zfail,
                       GLenum zpass) override;
void TexImage2D(GLenum target,
                GLint level,
                GLint internalformat,
                GLsizei width,
                GLsizei height,
                GLint border,
                GLenum format,
                GLenum type,
                const void* pixels) override;
void TexImage3D(GLenum target,
                GLint level,
                GLint internalformat,
                GLsizei width,
                GLsizei height,
                GLsizei depth,
                GLint border,
                GLenum format,
                GLenum type,
                const void* pixels) override;
void TexParameterf(GLenum target, GLenum pname, GLfloat param) override;
void TexParameterfv(GLenum target,
                    GLenum pname,
                    const GLfloat* params) override;
void TexParameteri(GLenum target, GLenum pname, GLint param) override;
void TexParameteriv(GLenum target, GLenum pname, const GLint* params) override;
void TexStorage3D(GLenum target,
                  GLsizei levels,
                  GLenum internalFormat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth) override;
void TexSubImage2D(GLenum target,
                   GLint level,
                   GLint xoffset,
                   GLint yoffset,
                   GLsizei width,
                   GLsizei height,
                   GLenum format,
                   GLenum type,
                   const void* pixels) override;
void TexSubImage3D(GLenum target,
                   GLint level,
                   GLint xoffset,
                   GLint yoffset,
                   GLint zoffset,
                   GLsizei width,
                   GLsizei height,
                   GLsizei depth,
                   GLenum format,
                   GLenum type,
                   const void* pixels) override;
void TransformFeedbackVaryings(GLuint program,
                               GLsizei count,
                               const char* const* varyings,
                               GLenum buffermode) override;
void Uniform1f(GLint location, GLfloat x) override;
void Uniform1fv(GLint location, GLsizei count, const GLfloat* v) override;
void Uniform1i(GLint location, GLint x) override;
void Uniform1iv(GLint location, GLsizei count, const GLint* v) override;
void Uniform1ui(GLint location, GLuint x) override;
void Uniform1uiv(GLint location, GLsizei count, const GLuint* v) override;
void Uniform2f(GLint location, GLfloat x, GLfloat y) override;
void Uniform2fv(GLint location, GLsizei count, const GLfloat* v) override;
void Uniform2i(GLint location, GLint x, GLint y) override;
void Uniform2iv(GLint location, GLsizei count, const GLint* v) override;
void Uniform2ui(GLint location, GLuint x, GLuint y) override;
void Uniform2uiv(GLint location, GLsizei count, const GLuint* v) override;
void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) override;
void Uniform3fv(GLint location, GLsizei count, const GLfloat* v) override;
void Uniform3i(GLint location, GLint x, GLint y, GLint z) override;
void Uniform3iv(GLint location, GLsizei count, const GLint* v) override;
void Uniform3ui(GLint location, GLuint x, GLuint y, GLuint z) override;
void Uniform3uiv(GLint location, GLsizei count, const GLuint* v) override;
void Uniform4f(GLint location,
               GLfloat x,
               GLfloat y,
               GLfloat z,
               GLfloat w) override;
void Uniform4fv(GLint location, GLsizei count, const GLfloat* v) override;
void Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) override;
void Uniform4iv(GLint location, GLsizei count, const GLint* v) override;
void Uniform4ui(GLint location,
                GLuint x,
                GLuint y,
                GLuint z,
                GLuint w) override;
void Uniform4uiv(GLint location, GLsizei count, const GLuint* v) override;
void UniformBlockBinding(GLuint program, GLuint index, GLuint binding) override;
void UniformMatrix2fv(GLint location,
                      GLsizei count,
                      GLboolean transpose,
                      const GLfloat* value) override;
void UniformMatrix2x3fv(GLint location,
                        GLsizei count,
                        GLboolean transpose,
                        const GLfloat* value) override;
void UniformMatrix2x4fv(GLint location,
                        GLsizei count,
                        GLboolean transpose,
                        const GLfloat* value) override;
void UniformMatrix3fv(GLint location,
                      GLsizei count,
                      GLboolean transpose,
                      const GLfloat* value) override;
void UniformMatrix3x2fv(GLint location,
                        GLsizei count,
                        GLboolean transpose,
                        const GLfloat* value) override;
void UniformMatrix3x4fv(GLint location,
                        GLsizei count,
                        GLboolean transpose,
                        const GLfloat* value) override;
void UniformMatrix4fv(GLint location,
                      GLsizei count,
                      GLboolean transpose,
                      const GLfloat* value) override;
void UniformMatrix4x2fv(GLint location,
                        GLsizei count,
                        GLboolean transpose,
                        const GLfloat* value) override;
void UniformMatrix4x3fv(GLint location,
                        GLsizei count,
                        GLboolean transpose,
                        const GLfloat* value) override;
void UseProgram(GLuint program) override;
void ValidateProgram(GLuint program) override;
void VertexAttrib1f(GLuint indx, GLfloat x) override;
void VertexAttrib1fv(GLuint indx, const GLfloat* values) override;
void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) override;
void VertexAttrib2fv(GLuint indx, const GLfloat* values) override;
void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) override;
void VertexAttrib3fv(GLuint indx, const GLfloat* values) override;
void VertexAttrib4f(GLuint indx,
                    GLfloat x,
                    GLfloat y,
                    GLfloat z,
                    GLfloat w) override;
void VertexAttrib4fv(GLuint indx, const GLfloat* values) override;
void VertexAttribI4i(GLuint indx, GLint x, GLint y, GLint z, GLint w) override;
void VertexAttribI4iv(GLuint indx, const GLint* values) override;
void VertexAttribI4ui(GLuint indx,
                      GLuint x,
                      GLuint y,
                      GLuint z,
                      GLuint w) override;
void VertexAttribI4uiv(GLuint indx, const GLuint* values) override;
void VertexAttribIPointer(GLuint indx,
                          GLint size,
                          GLenum type,
                          GLsizei stride,
                          const void* ptr) override;
void VertexAttribPointer(GLuint indx,
                         GLint size,
                         GLenum type,
                         GLboolean normalized,
                         GLsizei stride,
                         const void* ptr) override;
void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) override;
void WaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) override;
void BlitFramebufferCHROMIUM(GLint srcX0,
                             GLint srcY0,
                             GLint srcX1,
                             GLint srcY1,
                             GLint dstX0,
                             GLint dstY0,
                             GLint dstX1,
                             GLint dstY1,
                             GLbitfield mask,
                             GLenum filter) override;
void RenderbufferStorageMultisampleCHROMIUM(GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) override;
void RenderbufferStorageMultisampleAdvancedAMD(GLenum target,
                                               GLsizei samples,
                                               GLsizei storageSamples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height) override;
void RenderbufferStorageMultisampleEXT(GLenum target,
                                       GLsizei samples,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height) override;
void FramebufferTexture2DMultisampleEXT(GLenum target,
                                        GLenum attachment,
                                        GLenum textarget,
                                        GLuint texture,
                                        GLint level,
                                        GLsizei samples) override;
void TexStorage2DEXT(GLenum target,
                     GLsizei levels,
                     GLenum internalFormat,
                     GLsizei width,
                     GLsizei height) override;
void GenQueriesEXT(GLsizei n, GLuint* queries) override;
void DeleteQueriesEXT(GLsizei n, const GLuint* queries) override;
void QueryCounterEXT(GLuint id, GLenum target) override;
GLboolean IsQueryEXT(GLuint id) override;
void BeginQueryEXT(GLenum target, GLuint id) override;
void BeginTransformFeedback(GLenum primitivemode) override;
void EndQueryEXT(GLenum target) override;
void EndTransformFeedback() override;
void GetQueryivEXT(GLenum target, GLenum pname, GLint* params) override;
void GetQueryObjectivEXT(GLuint id, GLenum pname, GLint* params) override;
void GetQueryObjectuivEXT(GLuint id, GLenum pname, GLuint* params) override;
void GetQueryObjecti64vEXT(GLuint id, GLenum pname, GLint64* params) override;
void GetQueryObjectui64vEXT(GLuint id, GLenum pname, GLuint64* params) override;
void SetDisjointValueSyncCHROMIUM() override;
void InsertEventMarkerEXT(GLsizei length, const GLchar* marker) override;
void PushGroupMarkerEXT(GLsizei length, const GLchar* marker) override;
void PopGroupMarkerEXT() override;
void GenVertexArraysOES(GLsizei n, GLuint* arrays) override;
void DeleteVertexArraysOES(GLsizei n, const GLuint* arrays) override;
GLboolean IsVertexArrayOES(GLuint array) override;
void BindVertexArrayOES(GLuint array) override;
void FramebufferParameteri(GLenum target, GLenum pname, GLint param) override;
void BindImageTexture(GLuint unit,
                      GLuint texture,
                      GLint level,
                      GLboolean layered,
                      GLint layer,
                      GLenum access,
                      GLenum format) override;
void DispatchCompute(GLuint num_groups_x,
                     GLuint num_groups_y,
                     GLuint num_groups_z) override;
void DispatchComputeIndirect(GLintptr offset) override;
void DrawArraysIndirect(GLenum mode, const void* offset) override;
void DrawElementsIndirect(GLenum mode,
                          GLenum type,
                          const void* offset) override;
void GetProgramInterfaceiv(GLuint program,
                           GLenum program_interface,
                           GLenum pname,
                           GLint* params) override;
GLuint GetProgramResourceIndex(GLuint program,
                               GLenum program_interface,
                               const char* name) override;
void GetProgramResourceName(GLuint program,
                            GLenum program_interface,
                            GLuint index,
                            GLsizei bufsize,
                            GLsizei* length,
                            char* name) override;
void GetProgramResourceiv(GLuint program,
                          GLenum program_interface,
                          GLuint index,
                          GLsizei prop_count,
                          const GLenum* props,
                          GLsizei bufsize,
                          GLsizei* length,
                          GLint* params) override;
GLint GetProgramResourceLocation(GLuint program,
                                 GLenum program_interface,
                                 const char* name) override;
void MemoryBarrierEXT(GLbitfield barriers) override;
void MemoryBarrierByRegion(GLbitfield barriers) override;
void SwapBuffers(GLuint64 swap_id, GLbitfield flags) override;
GLuint GetMaxValueInBufferCHROMIUM(GLuint buffer_id,
                                   GLsizei count,
                                   GLenum type,
                                   GLuint offset) override;
GLboolean EnableFeatureCHROMIUM(const char* feature) override;
void* MapBufferCHROMIUM(GLuint target, GLenum access) override;
GLboolean UnmapBufferCHROMIUM(GLuint target) override;
void* MapBufferSubDataCHROMIUM(GLuint target,
                               GLintptr offset,
                               GLsizeiptr size,
                               GLenum access) override;
void UnmapBufferSubDataCHROMIUM(const void* mem) override;
void* MapBufferRange(GLenum target,
                     GLintptr offset,
                     GLsizeiptr size,
                     GLbitfield access) override;
GLboolean UnmapBuffer(GLenum target) override;
void FlushMappedBufferRange(GLenum target,
                            GLintptr offset,
                            GLsizeiptr size) override;
void* MapTexSubImage2DCHROMIUM(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               GLenum access) override;
void UnmapTexSubImage2DCHROMIUM(const void* mem) override;
void ResizeCHROMIUM(GLuint width,
                    GLuint height,
                    GLfloat scale_factor,
                    GLenum color_space,
                    GLboolean alpha) override;
const GLchar* GetRequestableExtensionsCHROMIUM() override;
void RequestExtensionCHROMIUM(const char* extension) override;
void GetProgramInfoCHROMIUM(GLuint program,
                            GLsizei bufsize,
                            GLsizei* size,
                            void* info) override;
void GetUniformBlocksCHROMIUM(GLuint program,
                              GLsizei bufsize,
                              GLsizei* size,
                              void* info) override;
void GetTransformFeedbackVaryingsCHROMIUM(GLuint program,
                                          GLsizei bufsize,
                                          GLsizei* size,
                                          void* info) override;
void GetUniformsES3CHROMIUM(GLuint program,
                            GLsizei bufsize,
                            GLsizei* size,
                            void* info) override;
GLuint CreateImageCHROMIUM(ClientBuffer buffer,
                           GLsizei width,
                           GLsizei height,
                           GLenum internalformat) override;
void DestroyImageCHROMIUM(GLuint image_id) override;
void DescheduleUntilFinishedCHROMIUM() override;
void GetTranslatedShaderSourceANGLE(GLuint shader,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    char* source) override;
void PostSubBufferCHROMIUM(GLuint64 swap_id,
                           GLint x,
                           GLint y,
                           GLint width,
                           GLint height,
                           GLbitfield flags) override;
void CopyTextureCHROMIUM(GLuint source_id,
                         GLint source_level,
                         GLenum dest_target,
                         GLuint dest_id,
                         GLint dest_level,
                         GLint internalformat,
                         GLenum dest_type,
                         GLboolean unpack_flip_y,
                         GLboolean unpack_premultiply_alpha,
                         GLboolean unpack_unmultiply_alpha) override;
void CopySubTextureCHROMIUM(GLuint source_id,
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
                            GLboolean unpack_unmultiply_alpha) override;
void DrawArraysInstancedANGLE(GLenum mode,
                              GLint first,
                              GLsizei count,
                              GLsizei primcount) override;
void DrawArraysInstancedBaseInstanceANGLE(GLenum mode,
                                          GLint first,
                                          GLsizei count,
                                          GLsizei primcount,
                                          GLuint baseinstance) override;
void DrawElementsInstancedANGLE(GLenum mode,
                                GLsizei count,
                                GLenum type,
                                const void* indices,
                                GLsizei primcount) override;
void DrawElementsInstancedBaseVertexBaseInstanceANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint basevertex,
    GLuint baseinstance) override;
void VertexAttribDivisorANGLE(GLuint index, GLuint divisor) override;
void ProduceTextureDirectCHROMIUM(GLuint texture, GLbyte* mailbox) override;
GLuint CreateAndConsumeTextureCHROMIUM(const GLbyte* mailbox) override;
void BindUniformLocationCHROMIUM(GLuint program,
                                 GLint location,
                                 const char* name) override;
void BindTexImage2DCHROMIUM(GLenum target, GLint imageId) override;
void BindTexImage2DWithInternalformatCHROMIUM(GLenum target,
                                              GLenum internalformat,
                                              GLint imageId) override;
void ReleaseTexImage2DCHROMIUM(GLenum target, GLint imageId) override;
void TraceBeginCHROMIUM(const char* category_name,
                        const char* trace_name) override;
void TraceEndCHROMIUM() override;
void DiscardFramebufferEXT(GLenum target,
                           GLsizei count,
                           const GLenum* attachments) override;
void LoseContextCHROMIUM(GLenum current, GLenum other) override;
void UnpremultiplyAndDitherCopyCHROMIUM(GLuint source_id,
                                        GLuint dest_id,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height) override;
void DrawBuffersEXT(GLsizei count, const GLenum* bufs) override;
void DiscardBackbufferCHROMIUM() override;
void ScheduleOverlayPlaneCHROMIUM(GLint plane_z_order,
                                  GLenum plane_transform,
                                  GLuint overlay_texture_id,
                                  GLint bounds_x,
                                  GLint bounds_y,
                                  GLint bounds_width,
                                  GLint bounds_height,
                                  GLfloat uv_x,
                                  GLfloat uv_y,
                                  GLfloat uv_width,
                                  GLfloat uv_height,
                                  GLboolean enable_blend,
                                  GLuint gpu_fence_id) override;
void ScheduleCALayerSharedStateCHROMIUM(GLfloat opacity,
                                        GLboolean is_clipped,
                                        const GLfloat* clip_rect,
                                        const GLfloat* rounded_corner_bounds,
                                        GLint sorting_context_id,
                                        const GLfloat* transform) override;
void ScheduleCALayerCHROMIUM(GLuint contents_texture_id,
                             const GLfloat* contents_rect,
                             GLuint background_color,
                             GLuint edge_aa_mask,
                             const GLfloat* bounds_rect,
                             GLuint filter) override;
void ScheduleCALayerInUseQueryCHROMIUM(GLsizei count,
                                       const GLuint* textures) override;
void CommitOverlayPlanesCHROMIUM(GLuint64 swap_id, GLbitfield flags) override;
void FlushDriverCachesCHROMIUM() override;
GLuint GetLastFlushIdCHROMIUM() override;
void ScheduleDCLayerCHROMIUM(GLuint texture_0,
                             GLuint texture_1,
                             GLint z_order,
                             GLint content_x,
                             GLint content_y,
                             GLint content_width,
                             GLint content_height,
                             GLint quad_x,
                             GLint quad_y,
                             GLint quad_width,
                             GLint quad_height,
                             GLfloat transform_c1r1,
                             GLfloat transform_c2r1,
                             GLfloat transform_c1r2,
                             GLfloat transform_c2r2,
                             GLfloat transform_tx,
                             GLfloat transform_ty,
                             GLboolean is_clipped,
                             GLint clip_x,
                             GLint clip_y,
                             GLint clip_width,
                             GLint clip_height,
                             GLuint protected_video_type) override;
void SetActiveURLCHROMIUM(const char* url) override;
void MatrixLoadfCHROMIUM(GLenum matrixMode, const GLfloat* m) override;
void MatrixLoadIdentityCHROMIUM(GLenum matrixMode) override;
GLuint GenPathsCHROMIUM(GLsizei range) override;
void DeletePathsCHROMIUM(GLuint path, GLsizei range) override;
GLboolean IsPathCHROMIUM(GLuint path) override;
void PathCommandsCHROMIUM(GLuint path,
                          GLsizei numCommands,
                          const GLubyte* commands,
                          GLsizei numCoords,
                          GLenum coordType,
                          const GLvoid* coords) override;
void PathParameterfCHROMIUM(GLuint path, GLenum pname, GLfloat value) override;
void PathParameteriCHROMIUM(GLuint path, GLenum pname, GLint value) override;
void PathStencilFuncCHROMIUM(GLenum func, GLint ref, GLuint mask) override;
void StencilFillPathCHROMIUM(GLuint path,
                             GLenum fillMode,
                             GLuint mask) override;
void StencilStrokePathCHROMIUM(GLuint path,
                               GLint reference,
                               GLuint mask) override;
void CoverFillPathCHROMIUM(GLuint path, GLenum coverMode) override;
void CoverStrokePathCHROMIUM(GLuint path, GLenum coverMode) override;
void StencilThenCoverFillPathCHROMIUM(GLuint path,
                                      GLenum fillMode,
                                      GLuint mask,
                                      GLenum coverMode) override;
void StencilThenCoverStrokePathCHROMIUM(GLuint path,
                                        GLint reference,
                                        GLuint mask,
                                        GLenum coverMode) override;
void StencilFillPathInstancedCHROMIUM(GLsizei numPaths,
                                      GLenum pathNameType,
                                      const GLvoid* paths,
                                      GLuint pathBase,
                                      GLenum fillMode,
                                      GLuint mask,
                                      GLenum transformType,
                                      const GLfloat* transformValues) override;
void StencilStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) override;
void CoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                    GLenum pathNameType,
                                    const GLvoid* paths,
                                    GLuint pathBase,
                                    GLenum coverMode,
                                    GLenum transformType,
                                    const GLfloat* transformValues) override;
void CoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                      GLenum pathNameType,
                                      const GLvoid* paths,
                                      GLuint pathBase,
                                      GLenum coverMode,
                                      GLenum transformType,
                                      const GLfloat* transformValues) override;
void StencilThenCoverFillPathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) override;
void StencilThenCoverStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) override;
void BindFragmentInputLocationCHROMIUM(GLuint program,
                                       GLint location,
                                       const char* name) override;
void ProgramPathFragmentInputGenCHROMIUM(GLuint program,
                                         GLint location,
                                         GLenum genMode,
                                         GLint components,
                                         const GLfloat* coeffs) override;
void ContextVisibilityHintCHROMIUM(GLboolean visibility) override;
void CoverageModulationCHROMIUM(GLenum components) override;
GLenum GetGraphicsResetStatusKHR() override;
void BlendBarrierKHR() override;
void BindFragDataLocationIndexedEXT(GLuint program,
                                    GLuint colorNumber,
                                    GLuint index,
                                    const char* name) override;
void BindFragDataLocationEXT(GLuint program,
                             GLuint colorNumber,
                             const char* name) override;
GLint GetFragDataIndexEXT(GLuint program, const char* name) override;
void UniformMatrix4fvStreamTextureMatrixCHROMIUM(
    GLint location,
    GLboolean transpose,
    const GLfloat* transform) override;
void OverlayPromotionHintCHROMIUM(GLuint texture,
                                  GLboolean promotion_hint,
                                  GLint display_x,
                                  GLint display_y,
                                  GLint display_width,
                                  GLint display_height) override;
void SwapBuffersWithBoundsCHROMIUM(GLuint64 swap_id,
                                   GLsizei count,
                                   const GLint* rects,
                                   GLbitfield flags) override;
void SetDrawRectangleCHROMIUM(GLint x,
                              GLint y,
                              GLint width,
                              GLint height) override;
void SetEnableDCLayersCHROMIUM(GLboolean enabled) override;
void InitializeDiscardableTextureCHROMIUM(GLuint texture_id) override;
void UnlockDiscardableTextureCHROMIUM(GLuint texture_id) override;
bool LockDiscardableTextureCHROMIUM(GLuint texture_id) override;
void TexStorage2DImageCHROMIUM(GLenum target,
                               GLenum internalFormat,
                               GLenum bufferUsage,
                               GLsizei width,
                               GLsizei height) override;
void SetColorSpaceMetadataCHROMIUM(GLuint texture_id,
                                   GLColorSpace color_space) override;
void WindowRectanglesEXT(GLenum mode, GLsizei count, const GLint* box) override;
GLuint CreateGpuFenceCHROMIUM() override;
GLuint CreateClientGpuFenceCHROMIUM(ClientGpuFence source) override;
void WaitGpuFenceCHROMIUM(GLuint gpu_fence_id) override;
void DestroyGpuFenceCHROMIUM(GLuint gpu_fence_id) override;
void InvalidateReadbackBufferShadowDataCHROMIUM(GLuint buffer_id) override;
void FramebufferTextureMultiviewOVR(GLenum target,
                                    GLenum attachment,
                                    GLuint texture,
                                    GLint level,
                                    GLint baseViewIndex,
                                    GLsizei numViews) override;
void MaxShaderCompilerThreadsKHR(GLuint count) override;
GLuint CreateAndTexStorage2DSharedImageCHROMIUM(const GLbyte* mailbox) override;
GLuint CreateAndTexStorage2DSharedImageWithInternalFormatCHROMIUM(
    const GLbyte* mailbox,
    GLenum internalformat) override;
void BeginSharedImageAccessDirectCHROMIUM(GLuint texture, GLenum mode) override;
void EndSharedImageAccessDirectCHROMIUM(GLuint texture) override;
#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_TRACE_IMPLEMENTATION_AUTOGEN_H_
