// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_DOER_PROTOTYPES_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_DOER_PROTOTYPES_H_

error::Error DoActiveTexture(GLenum texture);
error::Error DoAttachShader(GLuint program, GLuint shader);
error::Error DoBindAttribLocation(GLuint program,
                                  GLuint index,
                                  const char* name);
error::Error DoBindBuffer(GLenum target, GLuint buffer);
error::Error DoBindBufferBase(GLenum target, GLuint index, GLuint buffer);
error::Error DoBindBufferRange(GLenum target,
                               GLuint index,
                               GLuint buffer,
                               GLintptr offset,
                               GLsizeiptr size);
error::Error DoBindFramebuffer(GLenum target, GLuint framebuffer);
error::Error DoBindImageTexture(GLuint unit,
                                GLuint texture,
                                GLint level,
                                GLboolean layered,
                                GLint layer,
                                GLenum access,
                                GLenum format);
error::Error DoBindRenderbuffer(GLenum target, GLuint renderbuffer);
error::Error DoBindSampler(GLuint unit, GLuint sampler);
error::Error DoBindTexture(GLenum target, GLuint texture);
error::Error DoBindTransformFeedback(GLenum target, GLuint transformfeedback);
error::Error DoBlendColor(GLclampf red,
                          GLclampf green,
                          GLclampf blue,
                          GLclampf alpha);
error::Error DoBlendEquation(GLenum mode);
error::Error DoBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
error::Error DoBlendFunc(GLenum sfactor, GLenum dfactor);
error::Error DoBlendFuncSeparate(GLenum srcRGB,
                                 GLenum dstRGB,
                                 GLenum srcAlpha,
                                 GLenum dstAlpha);
error::Error DoBufferData(GLenum target,
                          GLsizeiptr size,
                          const void* data,
                          GLenum usage);
error::Error DoBufferSubData(GLenum target,
                             GLintptr offset,
                             GLsizeiptr size,
                             const void* data);
error::Error DoCheckFramebufferStatus(GLenum target, uint32_t* result);
error::Error DoClear(GLbitfield mask);
error::Error DoClearBufferfi(GLenum buffer,
                             GLint drawbuffers,
                             GLfloat depth,
                             GLint stencil);
error::Error DoClearBufferfv(GLenum buffer,
                             GLint drawbuffers,
                             const volatile GLfloat* value);
error::Error DoClearBufferiv(GLenum buffer,
                             GLint drawbuffers,
                             const volatile GLint* value);
error::Error DoClearBufferuiv(GLenum buffer,
                              GLint drawbuffers,
                              const volatile GLuint* value);
error::Error DoClearColor(GLclampf red,
                          GLclampf green,
                          GLclampf blue,
                          GLclampf alpha);
error::Error DoClearDepthf(GLclampf depth);
error::Error DoClearStencil(GLint s);
error::Error DoClientWaitSync(GLuint sync,
                              GLbitfield flags,
                              GLuint64 timeout,
                              GLenum* result);
error::Error DoColorMask(GLboolean red,
                         GLboolean green,
                         GLboolean blue,
                         GLboolean alpha);
error::Error DoCompileShader(GLuint shader);
error::Error DoCompressedTexImage2D(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border,
                                    GLsizei image_size,
                                    GLsizei data_size,
                                    const void* data);
error::Error DoCompressedTexSubImage2D(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLenum format,
                                       GLsizei image_size,
                                       GLsizei data_size,
                                       const void* data);
error::Error DoCompressedTexImage3D(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLsizei depth,
                                    GLint border,
                                    GLsizei image_size,
                                    GLsizei data_size,
                                    const void* data);
error::Error DoCompressedTexSubImage3D(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLenum format,
                                       GLsizei image_size,
                                       GLsizei data_size,
                                       const void* data);
error::Error DoContextVisibilityHintCHROMIUM(GLboolean visibility);
error::Error DoCopyBufferSubData(GLenum readtarget,
                                 GLenum writetarget,
                                 GLintptr readoffset,
                                 GLintptr writeoffset,
                                 GLsizeiptr size);
error::Error DoCopyTexImage2D(GLenum target,
                              GLint level,
                              GLenum internalformat,
                              GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLint border);
error::Error DoCopyTexSubImage2D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height);
error::Error DoCopyTexSubImage3D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height);
error::Error DoCreateProgram(GLuint client_id);
error::Error DoCreateShader(GLenum type, GLuint client_id);
error::Error DoCullFace(GLenum mode);
error::Error DoDeleteBuffers(GLsizei n, const volatile GLuint* buffers);
error::Error DoDeleteFramebuffers(GLsizei n,
                                  const volatile GLuint* framebuffers);
error::Error DoDeleteProgram(GLuint program);
error::Error DoDeleteRenderbuffers(GLsizei n,
                                   const volatile GLuint* renderbuffers);
error::Error DoDeleteSamplers(GLsizei n, const volatile GLuint* samplers);
error::Error DoDeleteSync(GLuint sync);
error::Error DoDeleteShader(GLuint shader);
error::Error DoDeleteTextures(GLsizei n, const volatile GLuint* textures);
error::Error DoDeleteTransformFeedbacks(GLsizei n, const volatile GLuint* ids);
error::Error DoDepthFunc(GLenum func);
error::Error DoDepthMask(GLboolean flag);
error::Error DoDepthRangef(GLclampf zNear, GLclampf zFar);
error::Error DoDetachShader(GLuint program, GLuint shader);
error::Error DoDisable(GLenum cap);
error::Error DoDisableVertexAttribArray(GLuint index);
error::Error DoDispatchCompute(GLuint num_groups_x,
                               GLuint num_groups_y,
                               GLuint num_groups_z);
error::Error DoDispatchComputeIndirect(GLintptr offset);
error::Error DoDrawArrays(GLenum mode, GLint first, GLsizei count);
error::Error DoDrawArraysIndirect(GLenum mode, const void* offset);
error::Error DoDrawElements(GLenum mode,
                            GLsizei count,
                            GLenum type,
                            const void* indices);
error::Error DoDrawElementsIndirect(GLenum mode,
                                    GLenum type,
                                    const void* offset);
error::Error DoEnable(GLenum cap);
error::Error DoEnableVertexAttribArray(GLuint index);
error::Error DoFenceSync(GLenum condition, GLbitfield flags, GLuint client_id);
error::Error DoFinish();
error::Error DoFlush();
error::Error DoFlushMappedBufferRange(GLenum target,
                                      GLintptr offset,
                                      GLsizeiptr size);
error::Error DoFramebufferParameteri(GLenum target, GLenum pname, GLint param);
error::Error DoFramebufferRenderbuffer(GLenum target,
                                       GLenum attachment,
                                       GLenum renderbuffertarget,
                                       GLuint renderbuffer);
error::Error DoFramebufferTexture2D(GLenum target,
                                    GLenum attachment,
                                    GLenum textarget,
                                    GLuint texture,
                                    GLint level);
error::Error DoFramebufferTextureLayer(GLenum target,
                                       GLenum attachment,
                                       GLuint texture,
                                       GLint level,
                                       GLint layer);
error::Error DoFramebufferTextureMultiviewOVR(GLenum target,
                                              GLenum attachment,
                                              GLuint texture,
                                              GLint level,
                                              GLint base_view_index,
                                              GLsizei num_views);
error::Error DoFrontFace(GLenum mode);
error::Error DoGenBuffers(GLsizei n, volatile GLuint* buffers);
error::Error DoGenerateMipmap(GLenum target);
error::Error DoGenFramebuffers(GLsizei n, volatile GLuint* framebuffers);
error::Error DoGenRenderbuffers(GLsizei n, volatile GLuint* renderbuffers);
error::Error DoGenSamplers(GLsizei n, volatile GLuint* samplers);
error::Error DoGenTextures(GLsizei n, volatile GLuint* textures);
error::Error DoGenTransformFeedbacks(GLsizei n, volatile GLuint* ids);
error::Error DoGetActiveAttrib(GLuint program,
                               GLuint index,
                               GLint* size,
                               GLenum* type,
                               std::string* name,
                               int32_t* success);
error::Error DoGetActiveUniform(GLuint program,
                                GLuint index,
                                GLint* size,
                                GLenum* type,
                                std::string* name,
                                int32_t* success);
error::Error DoGetActiveUniformBlockiv(GLuint program,
                                       GLuint index,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLint* params);
error::Error DoGetActiveUniformBlockName(GLuint program,
                                         GLuint index,
                                         std::string* name);
error::Error DoGetActiveUniformsiv(GLuint program,
                                   GLsizei count,
                                   const GLuint* indices,
                                   GLenum pname,
                                   GLint* params);
error::Error DoGetAttachedShaders(GLuint program,
                                  GLsizei maxcount,
                                  GLsizei* count,
                                  GLuint* shaders);
error::Error DoGetAttribLocation(GLuint program,
                                 const char* name,
                                 GLint* result);
error::Error DoGetBooleanv(GLenum pname,
                           GLsizei bufsize,
                           GLsizei* length,
                           GLboolean* params);
error::Error DoGetBufferParameteri64v(GLenum target,
                                      GLenum pname,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      GLint64* params);
error::Error DoGetBufferParameteriv(GLenum target,
                                    GLenum pname,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* params);
error::Error DoGetError(uint32_t* result);
error::Error DoGetFloatv(GLenum pname,
                         GLsizei bufsize,
                         GLsizei* length,
                         GLfloat* params);
error::Error DoGetFragDataLocation(GLuint program,
                                   const char* name,
                                   GLint* result);
error::Error DoGetFramebufferAttachmentParameteriv(GLenum target,
                                                   GLenum attachment,
                                                   GLenum pname,
                                                   GLsizei bufsize,
                                                   GLsizei* length,
                                                   GLint* params);
error::Error DoGetInteger64v(GLenum pname,
                             GLsizei bufsize,
                             GLsizei* length,
                             GLint64* params);
error::Error DoGetIntegeri_v(GLenum pname,
                             GLuint index,
                             GLsizei bufsize,
                             GLsizei* length,
                             GLint* data);
error::Error DoGetInteger64i_v(GLenum pname,
                               GLuint index,
                               GLsizei bufsize,
                               GLsizei* length,
                               GLint64* data);
error::Error DoGetIntegerv(GLenum pname,
                           GLsizei bufsize,
                           GLsizei* length,
                           GLint* params);
error::Error DoGetInternalformativ(GLenum target,
                                   GLenum format,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   GLint* params);
error::Error DoGetProgramiv(GLuint program,
                            GLenum pname,
                            GLsizei bufsize,
                            GLsizei* length,
                            GLint* params);
error::Error DoGetProgramInfoLog(GLuint program, std::string* infolog);
error::Error DoGetProgramInterfaceiv(GLuint program,
                                     GLenum program_interface,
                                     GLenum pname,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* params);
error::Error DoGetProgramResourceiv(GLuint program,
                                    GLenum program_interface,
                                    GLuint index,
                                    GLsizei prop_count,
                                    const GLenum* props,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* params);
error::Error DoGetProgramResourceIndex(GLuint program,
                                       GLenum program_interface,
                                       const char* name,
                                       GLuint* index);
error::Error DoGetProgramResourceLocation(GLuint program,
                                          GLenum program_interface,
                                          const char* name,
                                          GLint* location);
error::Error DoGetProgramResourceName(GLuint program,
                                      GLenum program_interface,
                                      GLuint index,
                                      std::string* name);
error::Error DoGetRenderbufferParameteriv(GLenum target,
                                          GLenum pname,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          GLint* params);
error::Error DoGetSamplerParameterfv(GLuint sampler,
                                     GLenum pname,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLfloat* params);
error::Error DoGetSamplerParameteriv(GLuint sampler,
                                     GLenum pname,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* params);
error::Error DoGetShaderiv(GLuint shader,
                           GLenum pname,
                           GLsizei bufsize,
                           GLsizei* length,
                           GLint* params);
error::Error DoGetShaderInfoLog(GLuint shader, std::string* infolog);
error::Error DoGetShaderPrecisionFormat(GLenum shadertype,
                                        GLenum precisiontype,
                                        GLint* range,
                                        GLint* precision,
                                        int32_t* success);
error::Error DoGetShaderSource(GLuint shader, std::string* source);
error::Error DoGetString(GLenum name, uint32_t bucket_id);
error::Error DoGetSynciv(GLuint sync,
                         GLenum pname,
                         GLsizei bufsize,
                         GLsizei* length,
                         GLint* values);
error::Error DoGetTexParameterfv(GLenum target,
                                 GLenum pname,
                                 GLsizei bufsize,
                                 GLsizei* length,
                                 GLfloat* params);
error::Error DoGetTexParameteriv(GLenum target,
                                 GLenum pname,
                                 GLsizei bufsize,
                                 GLsizei* length,
                                 GLint* params);
error::Error DoGetTransformFeedbackVarying(GLuint program,
                                           GLuint index,
                                           GLsizei* size,
                                           GLenum* type,
                                           std::string* name,
                                           int32_t* success);
error::Error DoGetUniformBlockIndex(GLuint program,
                                    const char* name,
                                    GLint* index);
error::Error DoGetUniformfv(GLuint program,
                            GLint location,
                            GLsizei bufsize,
                            GLsizei* length,
                            GLfloat* params);
error::Error DoGetUniformiv(GLuint program,
                            GLint location,
                            GLsizei bufsize,
                            GLsizei* length,
                            GLint* params);
error::Error DoGetUniformuiv(GLuint program,
                             GLint location,
                             GLsizei bufsize,
                             GLsizei* length,
                             GLuint* params);
error::Error DoGetUniformIndices(GLuint program,
                                 GLsizei count,
                                 const char* const* names,
                                 GLsizei bufSize,
                                 GLuint* indices);
error::Error DoGetUniformLocation(GLuint program,
                                  const char* name,
                                  GLint* location);
error::Error DoGetVertexAttribfv(GLuint index,
                                 GLenum pname,
                                 GLsizei bufsize,
                                 GLsizei* length,
                                 GLfloat* params);
error::Error DoGetVertexAttribiv(GLuint index,
                                 GLenum pname,
                                 GLsizei bufsize,
                                 GLsizei* length,
                                 GLint* params);
error::Error DoGetVertexAttribIiv(GLuint index,
                                  GLenum pname,
                                  GLsizei bufsize,
                                  GLsizei* length,
                                  GLint* params);
error::Error DoGetVertexAttribIuiv(GLuint index,
                                   GLenum pname,
                                   GLsizei bufsize,
                                   GLsizei* length,
                                   GLuint* params);
error::Error DoGetVertexAttribPointerv(GLuint index,
                                       GLenum pname,
                                       GLsizei bufsize,
                                       GLsizei* length,
                                       GLuint* pointer);
error::Error DoHint(GLenum target, GLenum mode);
error::Error DoInvalidateFramebuffer(GLenum target,
                                     GLsizei count,
                                     const volatile GLenum* attachments);
error::Error DoInvalidateSubFramebuffer(GLenum target,
                                        GLsizei count,
                                        const volatile GLenum* attachments,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height);
error::Error DoIsBuffer(GLuint buffer, uint32_t* result);
error::Error DoIsEnabled(GLenum cap, uint32_t* result);
error::Error DoIsFramebuffer(GLuint framebuffer, uint32_t* result);
error::Error DoIsProgram(GLuint program, uint32_t* result);
error::Error DoIsRenderbuffer(GLuint renderbuffer, uint32_t* result);
error::Error DoIsSampler(GLuint sampler, uint32_t* result);
error::Error DoIsShader(GLuint shader, uint32_t* result);
error::Error DoIsSync(GLuint sync, uint32_t* result);
error::Error DoIsTexture(GLuint texture, uint32_t* result);
error::Error DoIsTransformFeedback(GLuint transformfeedback, uint32_t* result);
error::Error DoLineWidth(GLfloat width);
error::Error DoLinkProgram(GLuint program);
error::Error DoMemoryBarrierEXT(GLbitfield barriers);
error::Error DoMemoryBarrierByRegion(GLbitfield barriers);
error::Error DoMultiDrawBeginCHROMIUM(GLsizei drawcount);
error::Error DoMultiDrawEndCHROMIUM();
error::Error DoPauseTransformFeedback();
error::Error DoPixelStorei(GLenum pname, GLint param);
error::Error DoPolygonOffset(GLfloat factor, GLfloat units);
error::Error DoReadBuffer(GLenum src);
error::Error DoReadPixels(GLint x,
                          GLint y,
                          GLsizei width,
                          GLsizei height,
                          GLenum format,
                          GLenum type,
                          GLsizei bufsize,
                          GLsizei* length,
                          GLsizei* columns,
                          GLsizei* rows,
                          void* pixels,
                          int32_t* success);
error::Error DoReadPixelsAsync(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               GLsizei bufsize,
                               GLsizei* length,
                               GLsizei* columns,
                               GLsizei* rows,
                               uint32_t pixels_shm_id,
                               uint32_t pixels_shm_offset,
                               uint32_t result_shm_id,
                               uint32_t result_shm_offset);
error::Error DoReleaseShaderCompiler();
error::Error DoRenderbufferStorage(GLenum target,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height);
error::Error DoResumeTransformFeedback();
error::Error DoSampleCoverage(GLclampf value, GLboolean invert);
error::Error DoSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param);
error::Error DoSamplerParameterfv(GLuint sampler,
                                  GLenum pname,
                                  const volatile GLfloat* params);
error::Error DoSamplerParameteri(GLuint sampler, GLenum pname, GLint param);
error::Error DoSamplerParameteriv(GLuint sampler,
                                  GLenum pname,
                                  const volatile GLint* params);
error::Error DoScissor(GLint x, GLint y, GLsizei width, GLsizei height);
error::Error DoShaderBinary(GLsizei n,
                            const GLuint* shaders,
                            GLenum binaryformat,
                            const void* binary,
                            GLsizei length);
error::Error DoShaderSource(GLuint shader,
                            GLsizei count,
                            const char** str,
                            const GLint* length);
error::Error DoStencilFunc(GLenum func, GLint ref, GLuint mask);
error::Error DoStencilFuncSeparate(GLenum face,
                                   GLenum func,
                                   GLint ref,
                                   GLuint mask);
error::Error DoStencilMask(GLuint mask);
error::Error DoStencilMaskSeparate(GLenum face, GLuint mask);
error::Error DoStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
error::Error DoStencilOpSeparate(GLenum face,
                                 GLenum fail,
                                 GLenum zfail,
                                 GLenum zpass);
error::Error DoTexImage2D(GLenum target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          GLsizei image_size,
                          const void* pixels);
error::Error DoTexStorage2DImageCHROMIUM(GLenum target,
                                         GLenum internalformat,
                                         GLenum bufferusage,
                                         GLsizei width,
                                         GLsizei height);
error::Error DoTexImage3D(GLenum target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei depth,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          GLsizei image_size,
                          const void* pixels);
error::Error DoTexParameterf(GLenum target, GLenum pname, GLfloat param);
error::Error DoTexParameterfv(GLenum target,
                              GLenum pname,
                              const volatile GLfloat* params);
error::Error DoTexParameteri(GLenum target, GLenum pname, GLint param);
error::Error DoTexParameteriv(GLenum target,
                              GLenum pname,
                              const volatile GLint* params);
error::Error DoTexStorage3D(GLenum target,
                            GLsizei levels,
                            GLenum internalFormat,
                            GLsizei width,
                            GLsizei height,
                            GLsizei depth);
error::Error DoTexSubImage2D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLenum type,
                             GLsizei image_size,
                             const void* pixels);
error::Error DoTexSubImage3D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLint zoffset,
                             GLsizei width,
                             GLsizei height,
                             GLsizei depth,
                             GLenum format,
                             GLenum type,
                             GLsizei image_size,
                             const void* pixels);
error::Error DoTransformFeedbackVaryings(GLuint program,
                                         GLsizei count,
                                         const char** varyings,
                                         GLenum buffermode);
error::Error DoUniform1f(GLint location, GLfloat x);
error::Error DoUniform1fv(GLint location,
                          GLsizei count,
                          const volatile GLfloat* v);
error::Error DoUniform1i(GLint location, GLint x);
error::Error DoUniform1iv(GLint location,
                          GLsizei count,
                          const volatile GLint* v);
error::Error DoUniform1ui(GLint location, GLuint x);
error::Error DoUniform1uiv(GLint location,
                           GLsizei count,
                           const volatile GLuint* v);
error::Error DoUniform2f(GLint location, GLfloat x, GLfloat y);
error::Error DoUniform2fv(GLint location,
                          GLsizei count,
                          const volatile GLfloat* v);
error::Error DoUniform2i(GLint location, GLint x, GLint y);
error::Error DoUniform2iv(GLint location,
                          GLsizei count,
                          const volatile GLint* v);
error::Error DoUniform2ui(GLint location, GLuint x, GLuint y);
error::Error DoUniform2uiv(GLint location,
                           GLsizei count,
                           const volatile GLuint* v);
error::Error DoUniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z);
error::Error DoUniform3fv(GLint location,
                          GLsizei count,
                          const volatile GLfloat* v);
error::Error DoUniform3i(GLint location, GLint x, GLint y, GLint z);
error::Error DoUniform3iv(GLint location,
                          GLsizei count,
                          const volatile GLint* v);
error::Error DoUniform3ui(GLint location, GLuint x, GLuint y, GLuint z);
error::Error DoUniform3uiv(GLint location,
                           GLsizei count,
                           const volatile GLuint* v);
error::Error DoUniform4f(GLint location,
                         GLfloat x,
                         GLfloat y,
                         GLfloat z,
                         GLfloat w);
error::Error DoUniform4fv(GLint location,
                          GLsizei count,
                          const volatile GLfloat* v);
error::Error DoUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w);
error::Error DoUniform4iv(GLint location,
                          GLsizei count,
                          const volatile GLint* v);
error::Error DoUniform4ui(GLint location,
                          GLuint x,
                          GLuint y,
                          GLuint z,
                          GLuint w);
error::Error DoUniform4uiv(GLint location,
                           GLsizei count,
                           const volatile GLuint* v);
error::Error DoUniformBlockBinding(GLuint program,
                                   GLuint index,
                                   GLuint binding);
error::Error DoUniformMatrix2fv(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const volatile GLfloat* value);
error::Error DoUniformMatrix2x3fv(GLint location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const volatile GLfloat* value);
error::Error DoUniformMatrix2x4fv(GLint location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const volatile GLfloat* value);
error::Error DoUniformMatrix3fv(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const volatile GLfloat* value);
error::Error DoUniformMatrix3x2fv(GLint location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const volatile GLfloat* value);
error::Error DoUniformMatrix3x4fv(GLint location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const volatile GLfloat* value);
error::Error DoUniformMatrix4fv(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const volatile GLfloat* value);
error::Error DoUniformMatrix4x2fv(GLint location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const volatile GLfloat* value);
error::Error DoUniformMatrix4x3fv(GLint location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const volatile GLfloat* value);
error::Error DoUseProgram(GLuint program);
error::Error DoValidateProgram(GLuint program);
error::Error DoVertexAttrib1f(GLuint indx, GLfloat x);
error::Error DoVertexAttrib1fv(GLuint indx, const volatile GLfloat* values);
error::Error DoVertexAttrib2f(GLuint indx, GLfloat x, GLfloat y);
error::Error DoVertexAttrib2fv(GLuint indx, const volatile GLfloat* values);
error::Error DoVertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z);
error::Error DoVertexAttrib3fv(GLuint indx, const volatile GLfloat* values);
error::Error DoVertexAttrib4f(GLuint indx,
                              GLfloat x,
                              GLfloat y,
                              GLfloat z,
                              GLfloat w);
error::Error DoVertexAttrib4fv(GLuint indx, const volatile GLfloat* values);
error::Error DoVertexAttribI4i(GLuint indx, GLint x, GLint y, GLint z, GLint w);
error::Error DoVertexAttribI4iv(GLuint indx, const volatile GLint* values);
error::Error DoVertexAttribI4ui(GLuint indx,
                                GLuint x,
                                GLuint y,
                                GLuint z,
                                GLuint w);
error::Error DoVertexAttribI4uiv(GLuint indx, const volatile GLuint* values);
error::Error DoVertexAttribIPointer(GLuint indx,
                                    GLint size,
                                    GLenum type,
                                    GLsizei stride,
                                    const void* ptr);
error::Error DoVertexAttribPointer(GLuint indx,
                                   GLint size,
                                   GLenum type,
                                   GLboolean normalized,
                                   GLsizei stride,
                                   const void* ptr);
error::Error DoViewport(GLint x, GLint y, GLsizei width, GLsizei height);
error::Error DoWaitSync(GLuint sync, GLbitfield flags, GLuint64 timeout);
error::Error DoBlitFramebufferCHROMIUM(GLint srcX0,
                                       GLint srcY0,
                                       GLint srcX1,
                                       GLint srcY1,
                                       GLint dstX0,
                                       GLint dstY0,
                                       GLint dstX1,
                                       GLint dstY1,
                                       GLbitfield mask,
                                       GLenum filter);
error::Error DoRenderbufferStorageMultisampleCHROMIUM(GLenum target,
                                                      GLsizei samples,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height);
error::Error DoRenderbufferStorageMultisampleAdvancedAMD(GLenum target,
                                                         GLsizei samples,
                                                         GLsizei storageSamples,
                                                         GLenum internalformat,
                                                         GLsizei width,
                                                         GLsizei height);
error::Error DoRenderbufferStorageMultisampleEXT(GLenum target,
                                                 GLsizei samples,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height);
error::Error DoFramebufferTexture2DMultisampleEXT(GLenum target,
                                                  GLenum attachment,
                                                  GLenum textarget,
                                                  GLuint texture,
                                                  GLint level,
                                                  GLsizei samples);
error::Error DoTexStorage2DEXT(GLenum target,
                               GLsizei levels,
                               GLenum internalFormat,
                               GLsizei width,
                               GLsizei height);
error::Error DoGenQueriesEXT(GLsizei n, volatile GLuint* queries);
error::Error DoDeleteQueriesEXT(GLsizei n, const volatile GLuint* queries);
error::Error DoQueryCounterEXT(GLuint id,
                               GLenum target,
                               int32_t sync_shm_id,
                               uint32_t sync_shm_offset,
                               uint32_t submit_count);
error::Error DoBeginQueryEXT(GLenum target,
                             GLuint id,
                             int32_t sync_shm_id,
                             uint32_t sync_shm_offset);
error::Error DoBeginTransformFeedback(GLenum primitivemode);
error::Error DoEndQueryEXT(GLenum target, uint32_t submit_count);
error::Error DoEndTransformFeedback();
error::Error DoSetDisjointValueSyncCHROMIUM(DisjointValueSync* sync);
error::Error DoInsertEventMarkerEXT(GLsizei length, const char* marker);
error::Error DoPushGroupMarkerEXT(GLsizei length, const char* marker);
error::Error DoPopGroupMarkerEXT();
error::Error DoGenVertexArraysOES(GLsizei n, volatile GLuint* arrays);
error::Error DoDeleteVertexArraysOES(GLsizei n, const volatile GLuint* arrays);
error::Error DoIsVertexArrayOES(GLuint array, uint32_t* result);
error::Error DoBindVertexArrayOES(GLuint array);
error::Error DoSwapBuffers(uint64_t swap_id, GLbitfield flags);
error::Error DoGetMaxValueInBufferCHROMIUM(GLuint buffer_id,
                                           GLsizei count,
                                           GLenum type,
                                           GLuint offset,
                                           uint32_t* result);
error::Error DoEnableFeatureCHROMIUM(const char* feature);
error::Error DoMapBufferRange(GLenum target,
                              GLintptr offset,
                              GLsizeiptr size,
                              GLbitfield access,
                              void* ptr,
                              int32_t data_shm_id,
                              uint32_t data_shm_offset,
                              uint32_t* result);
error::Error DoUnmapBuffer(GLenum target);
error::Error DoResizeCHROMIUM(GLuint width,
                              GLuint height,
                              GLfloat scale_factor,
                              GLenum color_space,
                              GLboolean alpha);
error::Error DoGetRequestableExtensionsCHROMIUM(const char** extensions);
error::Error DoRequestExtensionCHROMIUM(const char* extension);
error::Error DoGetProgramInfoCHROMIUM(GLuint program,
                                      std::vector<uint8_t>* data);
error::Error DoGetUniformBlocksCHROMIUM(GLuint program,
                                        std::vector<uint8_t>* data);
error::Error DoGetTransformFeedbackVaryingsCHROMIUM(GLuint program,
                                                    std::vector<uint8_t>* data);
error::Error DoGetUniformsES3CHROMIUM(GLuint program,
                                      std::vector<uint8_t>* data);
error::Error DoGetTranslatedShaderSourceANGLE(GLuint shader,
                                              std::string* source);
error::Error DoSwapBuffersWithBoundsCHROMIUM(uint64_t swap_id,
                                             GLsizei count,
                                             const volatile GLint* rects,
                                             GLbitfield flags);
error::Error DoPostSubBufferCHROMIUM(uint64_t swap_id,
                                     GLint x,
                                     GLint y,
                                     GLint width,
                                     GLint height,
                                     GLbitfield flags);
error::Error DoCopyTextureCHROMIUM(GLuint source_id,
                                   GLint source_level,
                                   GLenum dest_target,
                                   GLuint dest_id,
                                   GLint dest_level,
                                   GLint internalformat,
                                   GLenum dest_type,
                                   GLboolean unpack_flip_y,
                                   GLboolean unpack_premultiply_alpha,
                                   GLboolean unpack_unmultiply_alpha);
error::Error DoCopySubTextureCHROMIUM(GLuint source_id,
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
                                      GLboolean unpack_unmultiply_alpha);
error::Error DoDrawArraysInstancedANGLE(GLenum mode,
                                        GLint first,
                                        GLsizei count,
                                        GLsizei primcount);
error::Error DoDrawArraysInstancedBaseInstanceANGLE(GLenum mode,
                                                    GLint first,
                                                    GLsizei count,
                                                    GLsizei primcount,
                                                    GLuint baseinstance);
error::Error DoDrawElementsInstancedANGLE(GLenum mode,
                                          GLsizei count,
                                          GLenum type,
                                          const void* indices,
                                          GLsizei primcount);
error::Error DoDrawElementsInstancedBaseVertexBaseInstanceANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint basevertices,
    GLuint baseinstances);
error::Error DoVertexAttribDivisorANGLE(GLuint index, GLuint divisor);
error::Error DoProduceTextureDirectCHROMIUM(GLuint texture_client_id,
                                            const volatile GLbyte* mailbox);
error::Error DoCreateAndConsumeTextureINTERNAL(GLuint texture_client_id,
                                               const volatile GLbyte* mailbox);
error::Error DoBindUniformLocationCHROMIUM(GLuint program,
                                           GLint location,
                                           const char* name);
error::Error DoBindTexImage2DCHROMIUM(GLenum target, GLint imageId);
error::Error DoBindTexImage2DWithInternalformatCHROMIUM(GLenum target,
                                                        GLenum internalformat,
                                                        GLint imageId);
error::Error DoReleaseTexImage2DCHROMIUM(GLenum target, GLint imageId);
error::Error DoTraceBeginCHROMIUM(const char* category_name,
                                  const char* trace_name);
error::Error DoTraceEndCHROMIUM();
error::Error DoDiscardFramebufferEXT(GLenum target,
                                     GLsizei count,
                                     const volatile GLenum* attachments);
error::Error DoLoseContextCHROMIUM(GLenum current, GLenum other);
error::Error DoDescheduleUntilFinishedCHROMIUM();
error::Error DoWaitSyncTokenCHROMIUM(CommandBufferNamespace namespace_id,
                                     CommandBufferId command_buffer_id,
                                     GLuint64 release_count);
error::Error DoDrawBuffersEXT(GLsizei count, const volatile GLenum* bufs);
error::Error DoDiscardBackbufferCHROMIUM();
error::Error DoScheduleOverlayPlaneCHROMIUM(GLint plane_z_order,
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
                                            bool enable_blend,
                                            GLuint gpu_fence_id);
error::Error DoScheduleCALayerSharedStateCHROMIUM(
    GLfloat opacity,
    GLboolean is_clipped,
    const GLfloat* clip_rect,
    const GLfloat* rounded_corner_bounds,
    GLint sorting_context_id,
    const GLfloat* transform);
error::Error DoScheduleCALayerCHROMIUM(GLuint contents_texture_id,
                                       const GLfloat* contents_rect,
                                       GLuint background_color,
                                       GLuint edge_aa_mask,
                                       GLenum filter,
                                       const GLfloat* bounds_rect);
error::Error DoScheduleCALayerInUseQueryCHROMIUM(
    GLuint n,
    const volatile GLuint* textures);
error::Error DoScheduleDCLayerCHROMIUM(GLuint texture_0,
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
                                       GLuint protected_video_type);
error::Error DoCommitOverlayPlanesCHROMIUM(uint64_t swap_id, GLbitfield flags);
error::Error DoSetColorSpaceMetadataCHROMIUM(GLuint texture_id,
                                             gfx::ColorSpace color_space);
error::Error DoFlushDriverCachesCHROMIUM();
error::Error DoMatrixLoadfCHROMIUM(GLenum matrixMode,
                                   const volatile GLfloat* m);
error::Error DoMatrixLoadIdentityCHROMIUM(GLenum matrixMode);
error::Error DoGenPathsCHROMIUM(GLuint path, GLsizei range);
error::Error DoDeletePathsCHROMIUM(GLuint path, GLsizei range);
error::Error DoIsPathCHROMIUM(GLuint path, uint32_t* result);
error::Error DoPathCommandsCHROMIUM(GLuint path,
                                    GLsizei numCommands,
                                    const GLubyte* commands,
                                    GLsizei numCoords,
                                    GLenum coordType,
                                    const GLvoid* coords,
                                    GLsizei coords_bufsize);
error::Error DoPathParameterfCHROMIUM(GLuint path, GLenum pname, GLfloat value);
error::Error DoPathParameteriCHROMIUM(GLuint path, GLenum pname, GLint value);
error::Error DoPathStencilFuncCHROMIUM(GLenum func, GLint ref, GLuint mask);
error::Error DoStencilFillPathCHROMIUM(GLuint path,
                                       GLenum fillMode,
                                       GLuint mask);
error::Error DoStencilStrokePathCHROMIUM(GLuint path,
                                         GLint reference,
                                         GLuint mask);
error::Error DoCoverFillPathCHROMIUM(GLuint path, GLenum coverMode);
error::Error DoCoverStrokePathCHROMIUM(GLuint path, GLenum coverMode);
error::Error DoStencilThenCoverFillPathCHROMIUM(GLuint path,
                                                GLenum fillMode,
                                                GLuint mask,
                                                GLenum coverMode);
error::Error DoStencilThenCoverStrokePathCHROMIUM(GLuint path,
                                                  GLint reference,
                                                  GLuint mask,
                                                  GLenum coverMode);
error::Error DoStencilFillPathInstancedCHROMIUM(GLsizei numPaths,
                                                GLenum pathNameType,
                                                const GLvoid* paths,
                                                GLsizei pathsBufsize,
                                                GLuint pathBase,
                                                GLenum fillMode,
                                                GLuint mask,
                                                GLenum transformType,
                                                const GLfloat* transformValues,
                                                GLsizei transformValuesBufsize);
error::Error DoStencilStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize);
error::Error DoCoverFillPathInstancedCHROMIUM(GLsizei numPaths,
                                              GLenum pathNameType,
                                              const GLvoid* paths,
                                              GLsizei pathsBufsize,
                                              GLuint pathBase,
                                              GLenum coverMode,
                                              GLenum transformType,
                                              const GLfloat* transformValues,
                                              GLsizei transformValuesBufsize);
error::Error DoCoverStrokePathInstancedCHROMIUM(GLsizei numPaths,
                                                GLenum pathNameType,
                                                const GLvoid* paths,
                                                GLsizei pathsBufsize,
                                                GLuint pathBase,
                                                GLenum coverMode,
                                                GLenum transformType,
                                                const GLfloat* transformValues,
                                                GLsizei transformValuesBufsize);
error::Error DoStencilThenCoverFillPathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize);
error::Error DoStencilThenCoverStrokePathInstancedCHROMIUM(
    GLsizei numPaths,
    GLenum pathNameType,
    const GLvoid* paths,
    GLsizei pathsBufsize,
    GLuint pathBase,
    GLint reference,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues,
    GLsizei transformValuesBufsize);
error::Error DoBindFragmentInputLocationCHROMIUM(GLuint program,
                                                 GLint location,
                                                 const char* name);
error::Error DoProgramPathFragmentInputGenCHROMIUM(GLuint program,
                                                   GLint location,
                                                   GLenum genMode,
                                                   GLint components,
                                                   const GLfloat* coeffs,
                                                   GLsizei coeffsBufsize);
error::Error DoCoverageModulationCHROMIUM(GLenum components);
error::Error DoBlendBarrierKHR();
error::Error DoBindFragDataLocationIndexedEXT(GLuint program,
                                              GLuint colorNumber,
                                              GLuint index,
                                              const char* name);
error::Error DoBindFragDataLocationEXT(GLuint program,
                                       GLuint colorNumber,
                                       const char* name);
error::Error DoGetFragDataIndexEXT(GLuint program,
                                   const char* name,
                                   GLint* index);
error::Error DoUniformMatrix4fvStreamTextureMatrixCHROMIUM(
    GLint location,
    GLboolean transpose,
    const volatile GLfloat* defaultValue);

error::Error DoOverlayPromotionHintCHROMIUM(GLuint texture,
                                            GLboolean promotion_hint,
                                            GLint display_x,
                                            GLint display_y,
                                            GLint display_width,
                                            GLint display_height);
error::Error DoSetDrawRectangleCHROMIUM(GLint x,
                                        GLint y,
                                        GLint width,
                                        GLint height);
error::Error DoSetEnableDCLayersCHROMIUM(GLboolean enable);
error::Error DoBeginRasterCHROMIUM(GLuint texture_id,
                                   GLuint sk_color,
                                   GLuint msaa_sample_count,
                                   GLboolean can_use_lcd_text,
                                   GLint color_type);
error::Error DoRasterCHROMIUM(GLuint raster_shm_id,
                              GLuint raster_shm_offset,
                              GLsizeiptr raster_shm_size,
                              GLuint font_shm_id,
                              GLuint font_shm_offset,
                              GLsizeiptr font_shm_size);
error::Error DoEndRasterCHROMIUM();
error::Error DoCreateTransferCacheEntryINTERNAL(GLuint entry_type,
                                                GLuint entry_id,
                                                GLuint handle_shm_id,
                                                GLuint handle_shm_offset,
                                                GLuint data_shm_id,
                                                GLuint data_shm_offset,
                                                GLuint data_size);
error::Error DoUnlockTransferCacheEntryINTERNAL(GLuint entry_type,
                                                GLuint entry_id);
error::Error DoDeleteTransferCacheEntryINTERNAL(GLuint entry_type,
                                                GLuint entry_id);
error::Error DoWindowRectanglesEXT(GLenum mode,
                                   GLsizei n,
                                   const volatile GLint* box);
error::Error DoCreateGpuFenceINTERNAL(GLuint gpu_fence_id);
error::Error DoWaitGpuFenceCHROMIUM(GLuint gpu_fence_id);
error::Error DoDestroyGpuFenceCHROMIUM(GLuint gpu_fence_id);
error::Error DoUnpremultiplyAndDitherCopyCHROMIUM(GLuint src_texture,
                                                  GLuint dst_texture,
                                                  GLint x,
                                                  GLint y,
                                                  GLsizei width,
                                                  GLsizei height);
error::Error DoSetReadbackBufferShadowAllocationINTERNAL(GLuint buffer_id,
                                                         GLuint shm_id,
                                                         GLuint shm_offset,
                                                         GLuint size);
error::Error DoMaxShaderCompilerThreadsKHR(GLuint count);
error::Error DoInitializeDiscardableTextureCHROMIUM(
    GLuint texture_id,
    ServiceDiscardableHandle&& discardable_handle);
error::Error DoUnlockDiscardableTextureCHROMIUM(GLuint texture_id);
error::Error DoLockDiscardableTextureCHROMIUM(GLuint texture_id);
error::Error DoCreateAndTexStorage2DSharedImageINTERNAL(
    GLuint client_id,
    GLenum internalformat,
    const volatile GLbyte* mailbox);
error::Error DoBeginSharedImageAccessDirectCHROMIUM(GLuint client_id,
                                                    GLenum mode);
error::Error DoEndSharedImageAccessDirectCHROMIUM(GLuint client_id);
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_PASSTHROUGH_DOER_PROTOTYPES_H_
