// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_AUTOGEN_H_

void ActiveTexture(GLenum texture) {
  gles2::cmds::ActiveTexture* c = GetCmdSpace<gles2::cmds::ActiveTexture>();
  if (c) {
    c->Init(texture);
  }
}

void AttachShader(GLuint program, GLuint shader) {
  gles2::cmds::AttachShader* c = GetCmdSpace<gles2::cmds::AttachShader>();
  if (c) {
    c->Init(program, shader);
  }
}

void BindAttribLocationBucket(GLuint program,
                              GLuint index,
                              uint32_t name_bucket_id) {
  gles2::cmds::BindAttribLocationBucket* c =
      GetCmdSpace<gles2::cmds::BindAttribLocationBucket>();
  if (c) {
    c->Init(program, index, name_bucket_id);
  }
}

void BindBuffer(GLenum target, GLuint buffer) {
  gles2::cmds::BindBuffer* c = GetCmdSpace<gles2::cmds::BindBuffer>();
  if (c) {
    c->Init(target, buffer);
  }
}

void BindBufferBase(GLenum target, GLuint index, GLuint buffer) {
  gles2::cmds::BindBufferBase* c = GetCmdSpace<gles2::cmds::BindBufferBase>();
  if (c) {
    c->Init(target, index, buffer);
  }
}

void BindBufferRange(GLenum target,
                     GLuint index,
                     GLuint buffer,
                     GLintptr offset,
                     GLsizeiptr size) {
  gles2::cmds::BindBufferRange* c = GetCmdSpace<gles2::cmds::BindBufferRange>();
  if (c) {
    c->Init(target, index, buffer, offset, size);
  }
}

void BindFramebuffer(GLenum target, GLuint framebuffer) {
  gles2::cmds::BindFramebuffer* c = GetCmdSpace<gles2::cmds::BindFramebuffer>();
  if (c) {
    c->Init(target, framebuffer);
  }
}

void BindRenderbuffer(GLenum target, GLuint renderbuffer) {
  gles2::cmds::BindRenderbuffer* c =
      GetCmdSpace<gles2::cmds::BindRenderbuffer>();
  if (c) {
    c->Init(target, renderbuffer);
  }
}

void BindSampler(GLuint unit, GLuint sampler) {
  gles2::cmds::BindSampler* c = GetCmdSpace<gles2::cmds::BindSampler>();
  if (c) {
    c->Init(unit, sampler);
  }
}

void BindTexture(GLenum target, GLuint texture) {
  gles2::cmds::BindTexture* c = GetCmdSpace<gles2::cmds::BindTexture>();
  if (c) {
    c->Init(target, texture);
  }
}

void BindTransformFeedback(GLenum target, GLuint transformfeedback) {
  gles2::cmds::BindTransformFeedback* c =
      GetCmdSpace<gles2::cmds::BindTransformFeedback>();
  if (c) {
    c->Init(target, transformfeedback);
  }
}

void BlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  gles2::cmds::BlendColor* c = GetCmdSpace<gles2::cmds::BlendColor>();
  if (c) {
    c->Init(red, green, blue, alpha);
  }
}

void BlendEquation(GLenum mode) {
  gles2::cmds::BlendEquation* c = GetCmdSpace<gles2::cmds::BlendEquation>();
  if (c) {
    c->Init(mode);
  }
}

void BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  gles2::cmds::BlendEquationSeparate* c =
      GetCmdSpace<gles2::cmds::BlendEquationSeparate>();
  if (c) {
    c->Init(modeRGB, modeAlpha);
  }
}

void BlendFunc(GLenum sfactor, GLenum dfactor) {
  gles2::cmds::BlendFunc* c = GetCmdSpace<gles2::cmds::BlendFunc>();
  if (c) {
    c->Init(sfactor, dfactor);
  }
}

void BlendFuncSeparate(GLenum srcRGB,
                       GLenum dstRGB,
                       GLenum srcAlpha,
                       GLenum dstAlpha) {
  gles2::cmds::BlendFuncSeparate* c =
      GetCmdSpace<gles2::cmds::BlendFuncSeparate>();
  if (c) {
    c->Init(srcRGB, dstRGB, srcAlpha, dstAlpha);
  }
}

void BufferData(GLenum target,
                GLsizeiptr size,
                uint32_t data_shm_id,
                uint32_t data_shm_offset,
                GLenum usage) {
  gles2::cmds::BufferData* c = GetCmdSpace<gles2::cmds::BufferData>();
  if (c) {
    c->Init(target, size, data_shm_id, data_shm_offset, usage);
  }
}

void BufferSubData(GLenum target,
                   GLintptr offset,
                   GLsizeiptr size,
                   uint32_t data_shm_id,
                   uint32_t data_shm_offset) {
  gles2::cmds::BufferSubData* c = GetCmdSpace<gles2::cmds::BufferSubData>();
  if (c) {
    c->Init(target, offset, size, data_shm_id, data_shm_offset);
  }
}

void CheckFramebufferStatus(GLenum target,
                            uint32_t result_shm_id,
                            uint32_t result_shm_offset) {
  gles2::cmds::CheckFramebufferStatus* c =
      GetCmdSpace<gles2::cmds::CheckFramebufferStatus>();
  if (c) {
    c->Init(target, result_shm_id, result_shm_offset);
  }
}

void Clear(GLbitfield mask) {
  gles2::cmds::Clear* c = GetCmdSpace<gles2::cmds::Clear>();
  if (c) {
    c->Init(mask);
  }
}

void ClearBufferfi(GLenum buffer,
                   GLint drawbuffers,
                   GLfloat depth,
                   GLint stencil) {
  gles2::cmds::ClearBufferfi* c = GetCmdSpace<gles2::cmds::ClearBufferfi>();
  if (c) {
    c->Init(buffer, drawbuffers, depth, stencil);
  }
}

void ClearBufferfvImmediate(GLenum buffer,
                            GLint drawbuffers,
                            const GLfloat* value) {
  const uint32_t size = gles2::cmds::ClearBufferfvImmediate::ComputeSize();
  gles2::cmds::ClearBufferfvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::ClearBufferfvImmediate>(size);
  if (c) {
    c->Init(buffer, drawbuffers, value);
  }
}

void ClearBufferivImmediate(GLenum buffer,
                            GLint drawbuffers,
                            const GLint* value) {
  const uint32_t size = gles2::cmds::ClearBufferivImmediate::ComputeSize();
  gles2::cmds::ClearBufferivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::ClearBufferivImmediate>(size);
  if (c) {
    c->Init(buffer, drawbuffers, value);
  }
}

void ClearBufferuivImmediate(GLenum buffer,
                             GLint drawbuffers,
                             const GLuint* value) {
  const uint32_t size = gles2::cmds::ClearBufferuivImmediate::ComputeSize();
  gles2::cmds::ClearBufferuivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::ClearBufferuivImmediate>(size);
  if (c) {
    c->Init(buffer, drawbuffers, value);
  }
}

void ClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
  gles2::cmds::ClearColor* c = GetCmdSpace<gles2::cmds::ClearColor>();
  if (c) {
    c->Init(red, green, blue, alpha);
  }
}

void ClearDepthf(GLclampf depth) {
  gles2::cmds::ClearDepthf* c = GetCmdSpace<gles2::cmds::ClearDepthf>();
  if (c) {
    c->Init(depth);
  }
}

void ClearStencil(GLint s) {
  gles2::cmds::ClearStencil* c = GetCmdSpace<gles2::cmds::ClearStencil>();
  if (c) {
    c->Init(s);
  }
}

void ClientWaitSync(GLuint sync,
                    GLbitfield flags,
                    GLuint64 timeout,
                    uint32_t result_shm_id,
                    uint32_t result_shm_offset) {
  gles2::cmds::ClientWaitSync* c = GetCmdSpace<gles2::cmds::ClientWaitSync>();
  if (c) {
    c->Init(sync, flags, timeout, result_shm_id, result_shm_offset);
  }
}

void ColorMask(GLboolean red,
               GLboolean green,
               GLboolean blue,
               GLboolean alpha) {
  gles2::cmds::ColorMask* c = GetCmdSpace<gles2::cmds::ColorMask>();
  if (c) {
    c->Init(red, green, blue, alpha);
  }
}

void CompileShader(GLuint shader) {
  gles2::cmds::CompileShader* c = GetCmdSpace<gles2::cmds::CompileShader>();
  if (c) {
    c->Init(shader);
  }
}

void CompressedTexImage2DBucket(GLenum target,
                                GLint level,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLuint bucket_id) {
  gles2::cmds::CompressedTexImage2DBucket* c =
      GetCmdSpace<gles2::cmds::CompressedTexImage2DBucket>();
  if (c) {
    c->Init(target, level, internalformat, width, height, bucket_id);
  }
}

void CompressedTexImage2D(GLenum target,
                          GLint level,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei imageSize,
                          uint32_t data_shm_id,
                          uint32_t data_shm_offset) {
  gles2::cmds::CompressedTexImage2D* c =
      GetCmdSpace<gles2::cmds::CompressedTexImage2D>();
  if (c) {
    c->Init(target, level, internalformat, width, height, imageSize,
            data_shm_id, data_shm_offset);
  }
}

void CompressedTexSubImage2DBucket(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLuint bucket_id) {
  gles2::cmds::CompressedTexSubImage2DBucket* c =
      GetCmdSpace<gles2::cmds::CompressedTexSubImage2DBucket>();
  if (c) {
    c->Init(target, level, xoffset, yoffset, width, height, format, bucket_id);
  }
}

void CompressedTexSubImage2D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLsizei imageSize,
                             uint32_t data_shm_id,
                             uint32_t data_shm_offset) {
  gles2::cmds::CompressedTexSubImage2D* c =
      GetCmdSpace<gles2::cmds::CompressedTexSubImage2D>();
  if (c) {
    c->Init(target, level, xoffset, yoffset, width, height, format, imageSize,
            data_shm_id, data_shm_offset);
  }
}

void CompressedTexImage3DBucket(GLenum target,
                                GLint level,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLuint bucket_id) {
  gles2::cmds::CompressedTexImage3DBucket* c =
      GetCmdSpace<gles2::cmds::CompressedTexImage3DBucket>();
  if (c) {
    c->Init(target, level, internalformat, width, height, depth, bucket_id);
  }
}

void CompressedTexImage3D(GLenum target,
                          GLint level,
                          GLenum internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei depth,
                          GLsizei imageSize,
                          uint32_t data_shm_id,
                          uint32_t data_shm_offset) {
  gles2::cmds::CompressedTexImage3D* c =
      GetCmdSpace<gles2::cmds::CompressedTexImage3D>();
  if (c) {
    c->Init(target, level, internalformat, width, height, depth, imageSize,
            data_shm_id, data_shm_offset);
  }
}

void CompressedTexSubImage3DBucket(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLenum format,
                                   GLuint bucket_id) {
  gles2::cmds::CompressedTexSubImage3DBucket* c =
      GetCmdSpace<gles2::cmds::CompressedTexSubImage3DBucket>();
  if (c) {
    c->Init(target, level, xoffset, yoffset, zoffset, width, height, depth,
            format, bucket_id);
  }
}

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
                             uint32_t data_shm_id,
                             uint32_t data_shm_offset) {
  gles2::cmds::CompressedTexSubImage3D* c =
      GetCmdSpace<gles2::cmds::CompressedTexSubImage3D>();
  if (c) {
    c->Init(target, level, xoffset, yoffset, zoffset, width, height, depth,
            format, imageSize, data_shm_id, data_shm_offset);
  }
}

void CopyBufferSubData(GLenum readtarget,
                       GLenum writetarget,
                       GLintptr readoffset,
                       GLintptr writeoffset,
                       GLsizeiptr size) {
  gles2::cmds::CopyBufferSubData* c =
      GetCmdSpace<gles2::cmds::CopyBufferSubData>();
  if (c) {
    c->Init(readtarget, writetarget, readoffset, writeoffset, size);
  }
}

void CopyTexImage2D(GLenum target,
                    GLint level,
                    GLenum internalformat,
                    GLint x,
                    GLint y,
                    GLsizei width,
                    GLsizei height) {
  gles2::cmds::CopyTexImage2D* c = GetCmdSpace<gles2::cmds::CopyTexImage2D>();
  if (c) {
    c->Init(target, level, internalformat, x, y, width, height);
  }
}

void CopyTexSubImage2D(GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height) {
  gles2::cmds::CopyTexSubImage2D* c =
      GetCmdSpace<gles2::cmds::CopyTexSubImage2D>();
  if (c) {
    c->Init(target, level, xoffset, yoffset, x, y, width, height);
  }
}

void CopyTexSubImage3D(GLenum target,
                       GLint level,
                       GLint xoffset,
                       GLint yoffset,
                       GLint zoffset,
                       GLint x,
                       GLint y,
                       GLsizei width,
                       GLsizei height) {
  gles2::cmds::CopyTexSubImage3D* c =
      GetCmdSpace<gles2::cmds::CopyTexSubImage3D>();
  if (c) {
    c->Init(target, level, xoffset, yoffset, zoffset, x, y, width, height);
  }
}

void CreateProgram(uint32_t client_id) {
  gles2::cmds::CreateProgram* c = GetCmdSpace<gles2::cmds::CreateProgram>();
  if (c) {
    c->Init(client_id);
  }
}

void CreateShader(GLenum type, uint32_t client_id) {
  gles2::cmds::CreateShader* c = GetCmdSpace<gles2::cmds::CreateShader>();
  if (c) {
    c->Init(type, client_id);
  }
}

void CullFace(GLenum mode) {
  gles2::cmds::CullFace* c = GetCmdSpace<gles2::cmds::CullFace>();
  if (c) {
    c->Init(mode);
  }
}

void DeleteBuffersImmediate(GLsizei n, const GLuint* buffers) {
  const uint32_t size = gles2::cmds::DeleteBuffersImmediate::ComputeSize(n);
  gles2::cmds::DeleteBuffersImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteBuffersImmediate>(size);
  if (c) {
    c->Init(n, buffers);
  }
}

void DeleteFramebuffersImmediate(GLsizei n, const GLuint* framebuffers) {
  const uint32_t size =
      gles2::cmds::DeleteFramebuffersImmediate::ComputeSize(n);
  gles2::cmds::DeleteFramebuffersImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteFramebuffersImmediate>(
          size);
  if (c) {
    c->Init(n, framebuffers);
  }
}

void DeleteProgram(GLuint program) {
  gles2::cmds::DeleteProgram* c = GetCmdSpace<gles2::cmds::DeleteProgram>();
  if (c) {
    c->Init(program);
  }
}

void DeleteRenderbuffersImmediate(GLsizei n, const GLuint* renderbuffers) {
  const uint32_t size =
      gles2::cmds::DeleteRenderbuffersImmediate::ComputeSize(n);
  gles2::cmds::DeleteRenderbuffersImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteRenderbuffersImmediate>(
          size);
  if (c) {
    c->Init(n, renderbuffers);
  }
}

void DeleteSamplersImmediate(GLsizei n, const GLuint* samplers) {
  const uint32_t size = gles2::cmds::DeleteSamplersImmediate::ComputeSize(n);
  gles2::cmds::DeleteSamplersImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteSamplersImmediate>(size);
  if (c) {
    c->Init(n, samplers);
  }
}

void DeleteSync(GLuint sync) {
  gles2::cmds::DeleteSync* c = GetCmdSpace<gles2::cmds::DeleteSync>();
  if (c) {
    c->Init(sync);
  }
}

void DeleteShader(GLuint shader) {
  gles2::cmds::DeleteShader* c = GetCmdSpace<gles2::cmds::DeleteShader>();
  if (c) {
    c->Init(shader);
  }
}

void DeleteTexturesImmediate(GLsizei n, const GLuint* textures) {
  const uint32_t size = gles2::cmds::DeleteTexturesImmediate::ComputeSize(n);
  gles2::cmds::DeleteTexturesImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteTexturesImmediate>(size);
  if (c) {
    c->Init(n, textures);
  }
}

void DeleteTransformFeedbacksImmediate(GLsizei n, const GLuint* ids) {
  const uint32_t size =
      gles2::cmds::DeleteTransformFeedbacksImmediate::ComputeSize(n);
  gles2::cmds::DeleteTransformFeedbacksImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::DeleteTransformFeedbacksImmediate>(size);
  if (c) {
    c->Init(n, ids);
  }
}

void DepthFunc(GLenum func) {
  gles2::cmds::DepthFunc* c = GetCmdSpace<gles2::cmds::DepthFunc>();
  if (c) {
    c->Init(func);
  }
}

void DepthMask(GLboolean flag) {
  gles2::cmds::DepthMask* c = GetCmdSpace<gles2::cmds::DepthMask>();
  if (c) {
    c->Init(flag);
  }
}

void DepthRangef(GLclampf zNear, GLclampf zFar) {
  gles2::cmds::DepthRangef* c = GetCmdSpace<gles2::cmds::DepthRangef>();
  if (c) {
    c->Init(zNear, zFar);
  }
}

void DetachShader(GLuint program, GLuint shader) {
  gles2::cmds::DetachShader* c = GetCmdSpace<gles2::cmds::DetachShader>();
  if (c) {
    c->Init(program, shader);
  }
}

void Disable(GLenum cap) {
  gles2::cmds::Disable* c = GetCmdSpace<gles2::cmds::Disable>();
  if (c) {
    c->Init(cap);
  }
}

void DisableVertexAttribArray(GLuint index) {
  gles2::cmds::DisableVertexAttribArray* c =
      GetCmdSpace<gles2::cmds::DisableVertexAttribArray>();
  if (c) {
    c->Init(index);
  }
}

void DrawArrays(GLenum mode, GLint first, GLsizei count) {
  gles2::cmds::DrawArrays* c = GetCmdSpace<gles2::cmds::DrawArrays>();
  if (c) {
    c->Init(mode, first, count);
  }
}

void DrawElements(GLenum mode,
                  GLsizei count,
                  GLenum type,
                  GLuint index_offset) {
  gles2::cmds::DrawElements* c = GetCmdSpace<gles2::cmds::DrawElements>();
  if (c) {
    c->Init(mode, count, type, index_offset);
  }
}

void Enable(GLenum cap) {
  gles2::cmds::Enable* c = GetCmdSpace<gles2::cmds::Enable>();
  if (c) {
    c->Init(cap);
  }
}

void EnableVertexAttribArray(GLuint index) {
  gles2::cmds::EnableVertexAttribArray* c =
      GetCmdSpace<gles2::cmds::EnableVertexAttribArray>();
  if (c) {
    c->Init(index);
  }
}

void FenceSync(uint32_t client_id) {
  gles2::cmds::FenceSync* c = GetCmdSpace<gles2::cmds::FenceSync>();
  if (c) {
    c->Init(client_id);
  }
}

void Finish() {
  gles2::cmds::Finish* c = GetCmdSpace<gles2::cmds::Finish>();
  if (c) {
    c->Init();
  }
}

void Flush() {
  gles2::cmds::Flush* c = GetCmdSpace<gles2::cmds::Flush>();
  if (c) {
    c->Init();
  }
}

void FramebufferRenderbuffer(GLenum target,
                             GLenum attachment,
                             GLenum renderbuffertarget,
                             GLuint renderbuffer) {
  gles2::cmds::FramebufferRenderbuffer* c =
      GetCmdSpace<gles2::cmds::FramebufferRenderbuffer>();
  if (c) {
    c->Init(target, attachment, renderbuffertarget, renderbuffer);
  }
}

void FramebufferTexture2D(GLenum target,
                          GLenum attachment,
                          GLenum textarget,
                          GLuint texture,
                          GLint level) {
  gles2::cmds::FramebufferTexture2D* c =
      GetCmdSpace<gles2::cmds::FramebufferTexture2D>();
  if (c) {
    c->Init(target, attachment, textarget, texture, level);
  }
}

void FramebufferTextureLayer(GLenum target,
                             GLenum attachment,
                             GLuint texture,
                             GLint level,
                             GLint layer) {
  gles2::cmds::FramebufferTextureLayer* c =
      GetCmdSpace<gles2::cmds::FramebufferTextureLayer>();
  if (c) {
    c->Init(target, attachment, texture, level, layer);
  }
}

void FrontFace(GLenum mode) {
  gles2::cmds::FrontFace* c = GetCmdSpace<gles2::cmds::FrontFace>();
  if (c) {
    c->Init(mode);
  }
}

void GenBuffersImmediate(GLsizei n, GLuint* buffers) {
  const uint32_t size = gles2::cmds::GenBuffersImmediate::ComputeSize(n);
  gles2::cmds::GenBuffersImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::GenBuffersImmediate>(size);
  if (c) {
    c->Init(n, buffers);
  }
}

void GenerateMipmap(GLenum target) {
  gles2::cmds::GenerateMipmap* c = GetCmdSpace<gles2::cmds::GenerateMipmap>();
  if (c) {
    c->Init(target);
  }
}

void GenFramebuffersImmediate(GLsizei n, GLuint* framebuffers) {
  const uint32_t size = gles2::cmds::GenFramebuffersImmediate::ComputeSize(n);
  gles2::cmds::GenFramebuffersImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::GenFramebuffersImmediate>(
          size);
  if (c) {
    c->Init(n, framebuffers);
  }
}

void GenRenderbuffersImmediate(GLsizei n, GLuint* renderbuffers) {
  const uint32_t size = gles2::cmds::GenRenderbuffersImmediate::ComputeSize(n);
  gles2::cmds::GenRenderbuffersImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::GenRenderbuffersImmediate>(
          size);
  if (c) {
    c->Init(n, renderbuffers);
  }
}

void GenSamplersImmediate(GLsizei n, GLuint* samplers) {
  const uint32_t size = gles2::cmds::GenSamplersImmediate::ComputeSize(n);
  gles2::cmds::GenSamplersImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::GenSamplersImmediate>(size);
  if (c) {
    c->Init(n, samplers);
  }
}

void GenTexturesImmediate(GLsizei n, GLuint* textures) {
  const uint32_t size = gles2::cmds::GenTexturesImmediate::ComputeSize(n);
  gles2::cmds::GenTexturesImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::GenTexturesImmediate>(size);
  if (c) {
    c->Init(n, textures);
  }
}

void GenTransformFeedbacksImmediate(GLsizei n, GLuint* ids) {
  const uint32_t size =
      gles2::cmds::GenTransformFeedbacksImmediate::ComputeSize(n);
  gles2::cmds::GenTransformFeedbacksImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::GenTransformFeedbacksImmediate>(size);
  if (c) {
    c->Init(n, ids);
  }
}

void GetActiveAttrib(GLuint program,
                     GLuint index,
                     uint32_t name_bucket_id,
                     uint32_t result_shm_id,
                     uint32_t result_shm_offset) {
  gles2::cmds::GetActiveAttrib* c = GetCmdSpace<gles2::cmds::GetActiveAttrib>();
  if (c) {
    c->Init(program, index, name_bucket_id, result_shm_id, result_shm_offset);
  }
}

void GetActiveUniform(GLuint program,
                      GLuint index,
                      uint32_t name_bucket_id,
                      uint32_t result_shm_id,
                      uint32_t result_shm_offset) {
  gles2::cmds::GetActiveUniform* c =
      GetCmdSpace<gles2::cmds::GetActiveUniform>();
  if (c) {
    c->Init(program, index, name_bucket_id, result_shm_id, result_shm_offset);
  }
}

void GetActiveUniformBlockiv(GLuint program,
                             GLuint index,
                             GLenum pname,
                             uint32_t params_shm_id,
                             uint32_t params_shm_offset) {
  gles2::cmds::GetActiveUniformBlockiv* c =
      GetCmdSpace<gles2::cmds::GetActiveUniformBlockiv>();
  if (c) {
    c->Init(program, index, pname, params_shm_id, params_shm_offset);
  }
}

void GetActiveUniformBlockName(GLuint program,
                               GLuint index,
                               uint32_t name_bucket_id,
                               uint32_t result_shm_id,
                               uint32_t result_shm_offset) {
  gles2::cmds::GetActiveUniformBlockName* c =
      GetCmdSpace<gles2::cmds::GetActiveUniformBlockName>();
  if (c) {
    c->Init(program, index, name_bucket_id, result_shm_id, result_shm_offset);
  }
}

void GetActiveUniformsiv(GLuint program,
                         uint32_t indices_bucket_id,
                         GLenum pname,
                         uint32_t params_shm_id,
                         uint32_t params_shm_offset) {
  gles2::cmds::GetActiveUniformsiv* c =
      GetCmdSpace<gles2::cmds::GetActiveUniformsiv>();
  if (c) {
    c->Init(program, indices_bucket_id, pname, params_shm_id,
            params_shm_offset);
  }
}

void GetAttachedShaders(GLuint program,
                        uint32_t result_shm_id,
                        uint32_t result_shm_offset,
                        uint32_t result_size) {
  gles2::cmds::GetAttachedShaders* c =
      GetCmdSpace<gles2::cmds::GetAttachedShaders>();
  if (c) {
    c->Init(program, result_shm_id, result_shm_offset, result_size);
  }
}

void GetAttribLocation(GLuint program,
                       uint32_t name_bucket_id,
                       uint32_t location_shm_id,
                       uint32_t location_shm_offset) {
  gles2::cmds::GetAttribLocation* c =
      GetCmdSpace<gles2::cmds::GetAttribLocation>();
  if (c) {
    c->Init(program, name_bucket_id, location_shm_id, location_shm_offset);
  }
}

void GetBooleanv(GLenum pname,
                 uint32_t params_shm_id,
                 uint32_t params_shm_offset) {
  gles2::cmds::GetBooleanv* c = GetCmdSpace<gles2::cmds::GetBooleanv>();
  if (c) {
    c->Init(pname, params_shm_id, params_shm_offset);
  }
}

void GetBooleani_v(GLenum pname,
                   GLuint index,
                   uint32_t data_shm_id,
                   uint32_t data_shm_offset) {
  gles2::cmds::GetBooleani_v* c = GetCmdSpace<gles2::cmds::GetBooleani_v>();
  if (c) {
    c->Init(pname, index, data_shm_id, data_shm_offset);
  }
}

void GetBufferParameteri64v(GLenum target,
                            GLenum pname,
                            uint32_t params_shm_id,
                            uint32_t params_shm_offset) {
  gles2::cmds::GetBufferParameteri64v* c =
      GetCmdSpace<gles2::cmds::GetBufferParameteri64v>();
  if (c) {
    c->Init(target, pname, params_shm_id, params_shm_offset);
  }
}

void GetBufferParameteriv(GLenum target,
                          GLenum pname,
                          uint32_t params_shm_id,
                          uint32_t params_shm_offset) {
  gles2::cmds::GetBufferParameteriv* c =
      GetCmdSpace<gles2::cmds::GetBufferParameteriv>();
  if (c) {
    c->Init(target, pname, params_shm_id, params_shm_offset);
  }
}

void GetError(uint32_t result_shm_id, uint32_t result_shm_offset) {
  gles2::cmds::GetError* c = GetCmdSpace<gles2::cmds::GetError>();
  if (c) {
    c->Init(result_shm_id, result_shm_offset);
  }
}

void GetFloatv(GLenum pname,
               uint32_t params_shm_id,
               uint32_t params_shm_offset) {
  gles2::cmds::GetFloatv* c = GetCmdSpace<gles2::cmds::GetFloatv>();
  if (c) {
    c->Init(pname, params_shm_id, params_shm_offset);
  }
}

void GetFragDataLocation(GLuint program,
                         uint32_t name_bucket_id,
                         uint32_t location_shm_id,
                         uint32_t location_shm_offset) {
  gles2::cmds::GetFragDataLocation* c =
      GetCmdSpace<gles2::cmds::GetFragDataLocation>();
  if (c) {
    c->Init(program, name_bucket_id, location_shm_id, location_shm_offset);
  }
}

void GetFramebufferAttachmentParameteriv(GLenum target,
                                         GLenum attachment,
                                         GLenum pname,
                                         uint32_t params_shm_id,
                                         uint32_t params_shm_offset) {
  gles2::cmds::GetFramebufferAttachmentParameteriv* c =
      GetCmdSpace<gles2::cmds::GetFramebufferAttachmentParameteriv>();
  if (c) {
    c->Init(target, attachment, pname, params_shm_id, params_shm_offset);
  }
}

void GetInteger64v(GLenum pname,
                   uint32_t params_shm_id,
                   uint32_t params_shm_offset) {
  gles2::cmds::GetInteger64v* c = GetCmdSpace<gles2::cmds::GetInteger64v>();
  if (c) {
    c->Init(pname, params_shm_id, params_shm_offset);
  }
}

void GetIntegeri_v(GLenum pname,
                   GLuint index,
                   uint32_t data_shm_id,
                   uint32_t data_shm_offset) {
  gles2::cmds::GetIntegeri_v* c = GetCmdSpace<gles2::cmds::GetIntegeri_v>();
  if (c) {
    c->Init(pname, index, data_shm_id, data_shm_offset);
  }
}

void GetInteger64i_v(GLenum pname,
                     GLuint index,
                     uint32_t data_shm_id,
                     uint32_t data_shm_offset) {
  gles2::cmds::GetInteger64i_v* c = GetCmdSpace<gles2::cmds::GetInteger64i_v>();
  if (c) {
    c->Init(pname, index, data_shm_id, data_shm_offset);
  }
}

void GetIntegerv(GLenum pname,
                 uint32_t params_shm_id,
                 uint32_t params_shm_offset) {
  gles2::cmds::GetIntegerv* c = GetCmdSpace<gles2::cmds::GetIntegerv>();
  if (c) {
    c->Init(pname, params_shm_id, params_shm_offset);
  }
}

void GetInternalformativ(GLenum target,
                         GLenum format,
                         GLenum pname,
                         uint32_t params_shm_id,
                         uint32_t params_shm_offset) {
  gles2::cmds::GetInternalformativ* c =
      GetCmdSpace<gles2::cmds::GetInternalformativ>();
  if (c) {
    c->Init(target, format, pname, params_shm_id, params_shm_offset);
  }
}

void GetProgramiv(GLuint program,
                  GLenum pname,
                  uint32_t params_shm_id,
                  uint32_t params_shm_offset) {
  gles2::cmds::GetProgramiv* c = GetCmdSpace<gles2::cmds::GetProgramiv>();
  if (c) {
    c->Init(program, pname, params_shm_id, params_shm_offset);
  }
}

void GetProgramInfoLog(GLuint program, uint32_t bucket_id) {
  gles2::cmds::GetProgramInfoLog* c =
      GetCmdSpace<gles2::cmds::GetProgramInfoLog>();
  if (c) {
    c->Init(program, bucket_id);
  }
}

void GetRenderbufferParameteriv(GLenum target,
                                GLenum pname,
                                uint32_t params_shm_id,
                                uint32_t params_shm_offset) {
  gles2::cmds::GetRenderbufferParameteriv* c =
      GetCmdSpace<gles2::cmds::GetRenderbufferParameteriv>();
  if (c) {
    c->Init(target, pname, params_shm_id, params_shm_offset);
  }
}

void GetSamplerParameterfv(GLuint sampler,
                           GLenum pname,
                           uint32_t params_shm_id,
                           uint32_t params_shm_offset) {
  gles2::cmds::GetSamplerParameterfv* c =
      GetCmdSpace<gles2::cmds::GetSamplerParameterfv>();
  if (c) {
    c->Init(sampler, pname, params_shm_id, params_shm_offset);
  }
}

void GetSamplerParameteriv(GLuint sampler,
                           GLenum pname,
                           uint32_t params_shm_id,
                           uint32_t params_shm_offset) {
  gles2::cmds::GetSamplerParameteriv* c =
      GetCmdSpace<gles2::cmds::GetSamplerParameteriv>();
  if (c) {
    c->Init(sampler, pname, params_shm_id, params_shm_offset);
  }
}

void GetShaderiv(GLuint shader,
                 GLenum pname,
                 uint32_t params_shm_id,
                 uint32_t params_shm_offset) {
  gles2::cmds::GetShaderiv* c = GetCmdSpace<gles2::cmds::GetShaderiv>();
  if (c) {
    c->Init(shader, pname, params_shm_id, params_shm_offset);
  }
}

void GetShaderInfoLog(GLuint shader, uint32_t bucket_id) {
  gles2::cmds::GetShaderInfoLog* c =
      GetCmdSpace<gles2::cmds::GetShaderInfoLog>();
  if (c) {
    c->Init(shader, bucket_id);
  }
}

void GetShaderPrecisionFormat(GLenum shadertype,
                              GLenum precisiontype,
                              uint32_t result_shm_id,
                              uint32_t result_shm_offset) {
  gles2::cmds::GetShaderPrecisionFormat* c =
      GetCmdSpace<gles2::cmds::GetShaderPrecisionFormat>();
  if (c) {
    c->Init(shadertype, precisiontype, result_shm_id, result_shm_offset);
  }
}

void GetShaderSource(GLuint shader, uint32_t bucket_id) {
  gles2::cmds::GetShaderSource* c = GetCmdSpace<gles2::cmds::GetShaderSource>();
  if (c) {
    c->Init(shader, bucket_id);
  }
}

void GetString(GLenum name, uint32_t bucket_id) {
  gles2::cmds::GetString* c = GetCmdSpace<gles2::cmds::GetString>();
  if (c) {
    c->Init(name, bucket_id);
  }
}

void GetSynciv(GLuint sync,
               GLenum pname,
               uint32_t values_shm_id,
               uint32_t values_shm_offset) {
  gles2::cmds::GetSynciv* c = GetCmdSpace<gles2::cmds::GetSynciv>();
  if (c) {
    c->Init(sync, pname, values_shm_id, values_shm_offset);
  }
}

void GetTexParameterfv(GLenum target,
                       GLenum pname,
                       uint32_t params_shm_id,
                       uint32_t params_shm_offset) {
  gles2::cmds::GetTexParameterfv* c =
      GetCmdSpace<gles2::cmds::GetTexParameterfv>();
  if (c) {
    c->Init(target, pname, params_shm_id, params_shm_offset);
  }
}

void GetTexParameteriv(GLenum target,
                       GLenum pname,
                       uint32_t params_shm_id,
                       uint32_t params_shm_offset) {
  gles2::cmds::GetTexParameteriv* c =
      GetCmdSpace<gles2::cmds::GetTexParameteriv>();
  if (c) {
    c->Init(target, pname, params_shm_id, params_shm_offset);
  }
}

void GetTransformFeedbackVarying(GLuint program,
                                 GLuint index,
                                 uint32_t name_bucket_id,
                                 uint32_t result_shm_id,
                                 uint32_t result_shm_offset) {
  gles2::cmds::GetTransformFeedbackVarying* c =
      GetCmdSpace<gles2::cmds::GetTransformFeedbackVarying>();
  if (c) {
    c->Init(program, index, name_bucket_id, result_shm_id, result_shm_offset);
  }
}

void GetUniformBlockIndex(GLuint program,
                          uint32_t name_bucket_id,
                          uint32_t index_shm_id,
                          uint32_t index_shm_offset) {
  gles2::cmds::GetUniformBlockIndex* c =
      GetCmdSpace<gles2::cmds::GetUniformBlockIndex>();
  if (c) {
    c->Init(program, name_bucket_id, index_shm_id, index_shm_offset);
  }
}

void GetUniformfv(GLuint program,
                  GLint location,
                  uint32_t params_shm_id,
                  uint32_t params_shm_offset) {
  gles2::cmds::GetUniformfv* c = GetCmdSpace<gles2::cmds::GetUniformfv>();
  if (c) {
    c->Init(program, location, params_shm_id, params_shm_offset);
  }
}

void GetUniformiv(GLuint program,
                  GLint location,
                  uint32_t params_shm_id,
                  uint32_t params_shm_offset) {
  gles2::cmds::GetUniformiv* c = GetCmdSpace<gles2::cmds::GetUniformiv>();
  if (c) {
    c->Init(program, location, params_shm_id, params_shm_offset);
  }
}

void GetUniformuiv(GLuint program,
                   GLint location,
                   uint32_t params_shm_id,
                   uint32_t params_shm_offset) {
  gles2::cmds::GetUniformuiv* c = GetCmdSpace<gles2::cmds::GetUniformuiv>();
  if (c) {
    c->Init(program, location, params_shm_id, params_shm_offset);
  }
}

void GetUniformIndices(GLuint program,
                       uint32_t names_bucket_id,
                       uint32_t indices_shm_id,
                       uint32_t indices_shm_offset) {
  gles2::cmds::GetUniformIndices* c =
      GetCmdSpace<gles2::cmds::GetUniformIndices>();
  if (c) {
    c->Init(program, names_bucket_id, indices_shm_id, indices_shm_offset);
  }
}

void GetUniformLocation(GLuint program,
                        uint32_t name_bucket_id,
                        uint32_t location_shm_id,
                        uint32_t location_shm_offset) {
  gles2::cmds::GetUniformLocation* c =
      GetCmdSpace<gles2::cmds::GetUniformLocation>();
  if (c) {
    c->Init(program, name_bucket_id, location_shm_id, location_shm_offset);
  }
}

void GetVertexAttribfv(GLuint index,
                       GLenum pname,
                       uint32_t params_shm_id,
                       uint32_t params_shm_offset) {
  gles2::cmds::GetVertexAttribfv* c =
      GetCmdSpace<gles2::cmds::GetVertexAttribfv>();
  if (c) {
    c->Init(index, pname, params_shm_id, params_shm_offset);
  }
}

void GetVertexAttribiv(GLuint index,
                       GLenum pname,
                       uint32_t params_shm_id,
                       uint32_t params_shm_offset) {
  gles2::cmds::GetVertexAttribiv* c =
      GetCmdSpace<gles2::cmds::GetVertexAttribiv>();
  if (c) {
    c->Init(index, pname, params_shm_id, params_shm_offset);
  }
}

void GetVertexAttribIiv(GLuint index,
                        GLenum pname,
                        uint32_t params_shm_id,
                        uint32_t params_shm_offset) {
  gles2::cmds::GetVertexAttribIiv* c =
      GetCmdSpace<gles2::cmds::GetVertexAttribIiv>();
  if (c) {
    c->Init(index, pname, params_shm_id, params_shm_offset);
  }
}

void GetVertexAttribIuiv(GLuint index,
                         GLenum pname,
                         uint32_t params_shm_id,
                         uint32_t params_shm_offset) {
  gles2::cmds::GetVertexAttribIuiv* c =
      GetCmdSpace<gles2::cmds::GetVertexAttribIuiv>();
  if (c) {
    c->Init(index, pname, params_shm_id, params_shm_offset);
  }
}

void GetVertexAttribPointerv(GLuint index,
                             GLenum pname,
                             uint32_t pointer_shm_id,
                             uint32_t pointer_shm_offset) {
  gles2::cmds::GetVertexAttribPointerv* c =
      GetCmdSpace<gles2::cmds::GetVertexAttribPointerv>();
  if (c) {
    c->Init(index, pname, pointer_shm_id, pointer_shm_offset);
  }
}

void Hint(GLenum target, GLenum mode) {
  gles2::cmds::Hint* c = GetCmdSpace<gles2::cmds::Hint>();
  if (c) {
    c->Init(target, mode);
  }
}

void InvalidateFramebufferImmediate(GLenum target,
                                    GLsizei count,
                                    const GLenum* attachments) {
  const uint32_t size =
      gles2::cmds::InvalidateFramebufferImmediate::ComputeSize(count);
  gles2::cmds::InvalidateFramebufferImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::InvalidateFramebufferImmediate>(size);
  if (c) {
    c->Init(target, count, attachments);
  }
}

void InvalidateSubFramebufferImmediate(GLenum target,
                                       GLsizei count,
                                       const GLenum* attachments,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  const uint32_t size =
      gles2::cmds::InvalidateSubFramebufferImmediate::ComputeSize(count);
  gles2::cmds::InvalidateSubFramebufferImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::InvalidateSubFramebufferImmediate>(size);
  if (c) {
    c->Init(target, count, attachments, x, y, width, height);
  }
}

void IsBuffer(GLuint buffer,
              uint32_t result_shm_id,
              uint32_t result_shm_offset) {
  gles2::cmds::IsBuffer* c = GetCmdSpace<gles2::cmds::IsBuffer>();
  if (c) {
    c->Init(buffer, result_shm_id, result_shm_offset);
  }
}

void IsEnabled(GLenum cap, uint32_t result_shm_id, uint32_t result_shm_offset) {
  gles2::cmds::IsEnabled* c = GetCmdSpace<gles2::cmds::IsEnabled>();
  if (c) {
    c->Init(cap, result_shm_id, result_shm_offset);
  }
}

void IsFramebuffer(GLuint framebuffer,
                   uint32_t result_shm_id,
                   uint32_t result_shm_offset) {
  gles2::cmds::IsFramebuffer* c = GetCmdSpace<gles2::cmds::IsFramebuffer>();
  if (c) {
    c->Init(framebuffer, result_shm_id, result_shm_offset);
  }
}

void IsProgram(GLuint program,
               uint32_t result_shm_id,
               uint32_t result_shm_offset) {
  gles2::cmds::IsProgram* c = GetCmdSpace<gles2::cmds::IsProgram>();
  if (c) {
    c->Init(program, result_shm_id, result_shm_offset);
  }
}

void IsRenderbuffer(GLuint renderbuffer,
                    uint32_t result_shm_id,
                    uint32_t result_shm_offset) {
  gles2::cmds::IsRenderbuffer* c = GetCmdSpace<gles2::cmds::IsRenderbuffer>();
  if (c) {
    c->Init(renderbuffer, result_shm_id, result_shm_offset);
  }
}

void IsSampler(GLuint sampler,
               uint32_t result_shm_id,
               uint32_t result_shm_offset) {
  gles2::cmds::IsSampler* c = GetCmdSpace<gles2::cmds::IsSampler>();
  if (c) {
    c->Init(sampler, result_shm_id, result_shm_offset);
  }
}

void IsShader(GLuint shader,
              uint32_t result_shm_id,
              uint32_t result_shm_offset) {
  gles2::cmds::IsShader* c = GetCmdSpace<gles2::cmds::IsShader>();
  if (c) {
    c->Init(shader, result_shm_id, result_shm_offset);
  }
}

void IsSync(GLuint sync, uint32_t result_shm_id, uint32_t result_shm_offset) {
  gles2::cmds::IsSync* c = GetCmdSpace<gles2::cmds::IsSync>();
  if (c) {
    c->Init(sync, result_shm_id, result_shm_offset);
  }
}

void IsTexture(GLuint texture,
               uint32_t result_shm_id,
               uint32_t result_shm_offset) {
  gles2::cmds::IsTexture* c = GetCmdSpace<gles2::cmds::IsTexture>();
  if (c) {
    c->Init(texture, result_shm_id, result_shm_offset);
  }
}

void IsTransformFeedback(GLuint transformfeedback,
                         uint32_t result_shm_id,
                         uint32_t result_shm_offset) {
  gles2::cmds::IsTransformFeedback* c =
      GetCmdSpace<gles2::cmds::IsTransformFeedback>();
  if (c) {
    c->Init(transformfeedback, result_shm_id, result_shm_offset);
  }
}

void LineWidth(GLfloat width) {
  gles2::cmds::LineWidth* c = GetCmdSpace<gles2::cmds::LineWidth>();
  if (c) {
    c->Init(width);
  }
}

void LinkProgram(GLuint program) {
  gles2::cmds::LinkProgram* c = GetCmdSpace<gles2::cmds::LinkProgram>();
  if (c) {
    c->Init(program);
  }
}

void PauseTransformFeedback() {
  gles2::cmds::PauseTransformFeedback* c =
      GetCmdSpace<gles2::cmds::PauseTransformFeedback>();
  if (c) {
    c->Init();
  }
}

void PixelStorei(GLenum pname, GLint param) {
  gles2::cmds::PixelStorei* c = GetCmdSpace<gles2::cmds::PixelStorei>();
  if (c) {
    c->Init(pname, param);
  }
}

void PolygonOffset(GLfloat factor, GLfloat units) {
  gles2::cmds::PolygonOffset* c = GetCmdSpace<gles2::cmds::PolygonOffset>();
  if (c) {
    c->Init(factor, units);
  }
}

void ReadBuffer(GLenum src) {
  gles2::cmds::ReadBuffer* c = GetCmdSpace<gles2::cmds::ReadBuffer>();
  if (c) {
    c->Init(src);
  }
}

void ReadPixels(GLint x,
                GLint y,
                GLsizei width,
                GLsizei height,
                GLenum format,
                GLenum type,
                uint32_t pixels_shm_id,
                uint32_t pixels_shm_offset,
                uint32_t result_shm_id,
                uint32_t result_shm_offset,
                GLboolean async) {
  gles2::cmds::ReadPixels* c = GetCmdSpace<gles2::cmds::ReadPixels>();
  if (c) {
    c->Init(x, y, width, height, format, type, pixels_shm_id, pixels_shm_offset,
            result_shm_id, result_shm_offset, async);
  }
}

void ReleaseShaderCompiler() {
  gles2::cmds::ReleaseShaderCompiler* c =
      GetCmdSpace<gles2::cmds::ReleaseShaderCompiler>();
  if (c) {
    c->Init();
  }
}

void RenderbufferStorage(GLenum target,
                         GLenum internalformat,
                         GLsizei width,
                         GLsizei height) {
  gles2::cmds::RenderbufferStorage* c =
      GetCmdSpace<gles2::cmds::RenderbufferStorage>();
  if (c) {
    c->Init(target, internalformat, width, height);
  }
}

void ResumeTransformFeedback() {
  gles2::cmds::ResumeTransformFeedback* c =
      GetCmdSpace<gles2::cmds::ResumeTransformFeedback>();
  if (c) {
    c->Init();
  }
}

void SampleCoverage(GLclampf value, GLboolean invert) {
  gles2::cmds::SampleCoverage* c = GetCmdSpace<gles2::cmds::SampleCoverage>();
  if (c) {
    c->Init(value, invert);
  }
}

void SamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
  gles2::cmds::SamplerParameterf* c =
      GetCmdSpace<gles2::cmds::SamplerParameterf>();
  if (c) {
    c->Init(sampler, pname, param);
  }
}

void SamplerParameterfvImmediate(GLuint sampler,
                                 GLenum pname,
                                 const GLfloat* params) {
  const uint32_t size = gles2::cmds::SamplerParameterfvImmediate::ComputeSize();
  gles2::cmds::SamplerParameterfvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::SamplerParameterfvImmediate>(
          size);
  if (c) {
    c->Init(sampler, pname, params);
  }
}

void SamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
  gles2::cmds::SamplerParameteri* c =
      GetCmdSpace<gles2::cmds::SamplerParameteri>();
  if (c) {
    c->Init(sampler, pname, param);
  }
}

void SamplerParameterivImmediate(GLuint sampler,
                                 GLenum pname,
                                 const GLint* params) {
  const uint32_t size = gles2::cmds::SamplerParameterivImmediate::ComputeSize();
  gles2::cmds::SamplerParameterivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::SamplerParameterivImmediate>(
          size);
  if (c) {
    c->Init(sampler, pname, params);
  }
}

void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  gles2::cmds::Scissor* c = GetCmdSpace<gles2::cmds::Scissor>();
  if (c) {
    c->Init(x, y, width, height);
  }
}

void ShaderBinary(GLsizei n,
                  uint32_t shaders_shm_id,
                  uint32_t shaders_shm_offset,
                  GLenum binaryformat,
                  uint32_t binary_shm_id,
                  uint32_t binary_shm_offset,
                  GLsizei length) {
  gles2::cmds::ShaderBinary* c = GetCmdSpace<gles2::cmds::ShaderBinary>();
  if (c) {
    c->Init(n, shaders_shm_id, shaders_shm_offset, binaryformat, binary_shm_id,
            binary_shm_offset, length);
  }
}

void ShaderSourceBucket(GLuint shader, uint32_t str_bucket_id) {
  gles2::cmds::ShaderSourceBucket* c =
      GetCmdSpace<gles2::cmds::ShaderSourceBucket>();
  if (c) {
    c->Init(shader, str_bucket_id);
  }
}

void MultiDrawBeginCHROMIUM(GLsizei drawcount) {
  gles2::cmds::MultiDrawBeginCHROMIUM* c =
      GetCmdSpace<gles2::cmds::MultiDrawBeginCHROMIUM>();
  if (c) {
    c->Init(drawcount);
  }
}

void MultiDrawEndCHROMIUM() {
  gles2::cmds::MultiDrawEndCHROMIUM* c =
      GetCmdSpace<gles2::cmds::MultiDrawEndCHROMIUM>();
  if (c) {
    c->Init();
  }
}

void MultiDrawArraysCHROMIUM(GLenum mode,
                             uint32_t firsts_shm_id,
                             uint32_t firsts_shm_offset,
                             uint32_t counts_shm_id,
                             uint32_t counts_shm_offset,
                             GLsizei drawcount) {
  gles2::cmds::MultiDrawArraysCHROMIUM* c =
      GetCmdSpace<gles2::cmds::MultiDrawArraysCHROMIUM>();
  if (c) {
    c->Init(mode, firsts_shm_id, firsts_shm_offset, counts_shm_id,
            counts_shm_offset, drawcount);
  }
}

void MultiDrawArraysInstancedCHROMIUM(GLenum mode,
                                      uint32_t firsts_shm_id,
                                      uint32_t firsts_shm_offset,
                                      uint32_t counts_shm_id,
                                      uint32_t counts_shm_offset,
                                      uint32_t instance_counts_shm_id,
                                      uint32_t instance_counts_shm_offset,
                                      GLsizei drawcount) {
  gles2::cmds::MultiDrawArraysInstancedCHROMIUM* c =
      GetCmdSpace<gles2::cmds::MultiDrawArraysInstancedCHROMIUM>();
  if (c) {
    c->Init(mode, firsts_shm_id, firsts_shm_offset, counts_shm_id,
            counts_shm_offset, instance_counts_shm_id,
            instance_counts_shm_offset, drawcount);
  }
}

void MultiDrawArraysInstancedBaseInstanceCHROMIUM(
    GLenum mode,
    uint32_t firsts_shm_id,
    uint32_t firsts_shm_offset,
    uint32_t counts_shm_id,
    uint32_t counts_shm_offset,
    uint32_t instance_counts_shm_id,
    uint32_t instance_counts_shm_offset,
    uint32_t baseinstances_shm_id,
    uint32_t baseinstances_shm_offset,
    GLsizei drawcount) {
  gles2::cmds::MultiDrawArraysInstancedBaseInstanceCHROMIUM* c =
      GetCmdSpace<gles2::cmds::MultiDrawArraysInstancedBaseInstanceCHROMIUM>();
  if (c) {
    c->Init(mode, firsts_shm_id, firsts_shm_offset, counts_shm_id,
            counts_shm_offset, instance_counts_shm_id,
            instance_counts_shm_offset, baseinstances_shm_id,
            baseinstances_shm_offset, drawcount);
  }
}

void MultiDrawElementsCHROMIUM(GLenum mode,
                               uint32_t counts_shm_id,
                               uint32_t counts_shm_offset,
                               GLenum type,
                               uint32_t offsets_shm_id,
                               uint32_t offsets_shm_offset,
                               GLsizei drawcount) {
  gles2::cmds::MultiDrawElementsCHROMIUM* c =
      GetCmdSpace<gles2::cmds::MultiDrawElementsCHROMIUM>();
  if (c) {
    c->Init(mode, counts_shm_id, counts_shm_offset, type, offsets_shm_id,
            offsets_shm_offset, drawcount);
  }
}

void MultiDrawElementsInstancedCHROMIUM(GLenum mode,
                                        uint32_t counts_shm_id,
                                        uint32_t counts_shm_offset,
                                        GLenum type,
                                        uint32_t offsets_shm_id,
                                        uint32_t offsets_shm_offset,
                                        uint32_t instance_counts_shm_id,
                                        uint32_t instance_counts_shm_offset,
                                        GLsizei drawcount) {
  gles2::cmds::MultiDrawElementsInstancedCHROMIUM* c =
      GetCmdSpace<gles2::cmds::MultiDrawElementsInstancedCHROMIUM>();
  if (c) {
    c->Init(mode, counts_shm_id, counts_shm_offset, type, offsets_shm_id,
            offsets_shm_offset, instance_counts_shm_id,
            instance_counts_shm_offset, drawcount);
  }
}

void MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM(
    GLenum mode,
    uint32_t counts_shm_id,
    uint32_t counts_shm_offset,
    GLenum type,
    uint32_t offsets_shm_id,
    uint32_t offsets_shm_offset,
    uint32_t instance_counts_shm_id,
    uint32_t instance_counts_shm_offset,
    uint32_t basevertices_shm_id,
    uint32_t basevertices_shm_offset,
    uint32_t baseinstances_shm_id,
    uint32_t baseinstances_shm_offset,
    GLsizei drawcount) {
  gles2::cmds::MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM* c =
      GetCmdSpace<
          gles2::cmds::
              MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM>();
  if (c) {
    c->Init(mode, counts_shm_id, counts_shm_offset, type, offsets_shm_id,
            offsets_shm_offset, instance_counts_shm_id,
            instance_counts_shm_offset, basevertices_shm_id,
            basevertices_shm_offset, baseinstances_shm_id,
            baseinstances_shm_offset, drawcount);
  }
}

void StencilFunc(GLenum func, GLint ref, GLuint mask) {
  gles2::cmds::StencilFunc* c = GetCmdSpace<gles2::cmds::StencilFunc>();
  if (c) {
    c->Init(func, ref, mask);
  }
}

void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
  gles2::cmds::StencilFuncSeparate* c =
      GetCmdSpace<gles2::cmds::StencilFuncSeparate>();
  if (c) {
    c->Init(face, func, ref, mask);
  }
}

void StencilMask(GLuint mask) {
  gles2::cmds::StencilMask* c = GetCmdSpace<gles2::cmds::StencilMask>();
  if (c) {
    c->Init(mask);
  }
}

void StencilMaskSeparate(GLenum face, GLuint mask) {
  gles2::cmds::StencilMaskSeparate* c =
      GetCmdSpace<gles2::cmds::StencilMaskSeparate>();
  if (c) {
    c->Init(face, mask);
  }
}

void StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  gles2::cmds::StencilOp* c = GetCmdSpace<gles2::cmds::StencilOp>();
  if (c) {
    c->Init(fail, zfail, zpass);
  }
}

void StencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  gles2::cmds::StencilOpSeparate* c =
      GetCmdSpace<gles2::cmds::StencilOpSeparate>();
  if (c) {
    c->Init(face, fail, zfail, zpass);
  }
}

void TexImage2D(GLenum target,
                GLint level,
                GLint internalformat,
                GLsizei width,
                GLsizei height,
                GLenum format,
                GLenum type,
                uint32_t pixels_shm_id,
                uint32_t pixels_shm_offset) {
  gles2::cmds::TexImage2D* c = GetCmdSpace<gles2::cmds::TexImage2D>();
  if (c) {
    c->Init(target, level, internalformat, width, height, format, type,
            pixels_shm_id, pixels_shm_offset);
  }
}

void TexImage3D(GLenum target,
                GLint level,
                GLint internalformat,
                GLsizei width,
                GLsizei height,
                GLsizei depth,
                GLenum format,
                GLenum type,
                uint32_t pixels_shm_id,
                uint32_t pixels_shm_offset) {
  gles2::cmds::TexImage3D* c = GetCmdSpace<gles2::cmds::TexImage3D>();
  if (c) {
    c->Init(target, level, internalformat, width, height, depth, format, type,
            pixels_shm_id, pixels_shm_offset);
  }
}

void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
  gles2::cmds::TexParameterf* c = GetCmdSpace<gles2::cmds::TexParameterf>();
  if (c) {
    c->Init(target, pname, param);
  }
}

void TexParameterfvImmediate(GLenum target,
                             GLenum pname,
                             const GLfloat* params) {
  const uint32_t size = gles2::cmds::TexParameterfvImmediate::ComputeSize();
  gles2::cmds::TexParameterfvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::TexParameterfvImmediate>(size);
  if (c) {
    c->Init(target, pname, params);
  }
}

void TexParameteri(GLenum target, GLenum pname, GLint param) {
  gles2::cmds::TexParameteri* c = GetCmdSpace<gles2::cmds::TexParameteri>();
  if (c) {
    c->Init(target, pname, param);
  }
}

void TexParameterivImmediate(GLenum target, GLenum pname, const GLint* params) {
  const uint32_t size = gles2::cmds::TexParameterivImmediate::ComputeSize();
  gles2::cmds::TexParameterivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::TexParameterivImmediate>(size);
  if (c) {
    c->Init(target, pname, params);
  }
}

void TexStorage3D(GLenum target,
                  GLsizei levels,
                  GLenum internalFormat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth) {
  gles2::cmds::TexStorage3D* c = GetCmdSpace<gles2::cmds::TexStorage3D>();
  if (c) {
    c->Init(target, levels, internalFormat, width, height, depth);
  }
}

void TexSubImage2D(GLenum target,
                   GLint level,
                   GLint xoffset,
                   GLint yoffset,
                   GLsizei width,
                   GLsizei height,
                   GLenum format,
                   GLenum type,
                   uint32_t pixels_shm_id,
                   uint32_t pixels_shm_offset,
                   GLboolean internal) {
  gles2::cmds::TexSubImage2D* c = GetCmdSpace<gles2::cmds::TexSubImage2D>();
  if (c) {
    c->Init(target, level, xoffset, yoffset, width, height, format, type,
            pixels_shm_id, pixels_shm_offset, internal);
  }
}

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
                   uint32_t pixels_shm_id,
                   uint32_t pixels_shm_offset,
                   GLboolean internal) {
  gles2::cmds::TexSubImage3D* c = GetCmdSpace<gles2::cmds::TexSubImage3D>();
  if (c) {
    c->Init(target, level, xoffset, yoffset, zoffset, width, height, depth,
            format, type, pixels_shm_id, pixels_shm_offset, internal);
  }
}

void TransformFeedbackVaryingsBucket(GLuint program,
                                     uint32_t varyings_bucket_id,
                                     GLenum buffermode) {
  gles2::cmds::TransformFeedbackVaryingsBucket* c =
      GetCmdSpace<gles2::cmds::TransformFeedbackVaryingsBucket>();
  if (c) {
    c->Init(program, varyings_bucket_id, buffermode);
  }
}

void Uniform1f(GLint location, GLfloat x) {
  gles2::cmds::Uniform1f* c = GetCmdSpace<gles2::cmds::Uniform1f>();
  if (c) {
    c->Init(location, x);
  }
}

void Uniform1fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
  const uint32_t size = gles2::cmds::Uniform1fvImmediate::ComputeSize(count);
  gles2::cmds::Uniform1fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform1fvImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform1i(GLint location, GLint x) {
  gles2::cmds::Uniform1i* c = GetCmdSpace<gles2::cmds::Uniform1i>();
  if (c) {
    c->Init(location, x);
  }
}

void Uniform1ivImmediate(GLint location, GLsizei count, const GLint* v) {
  const uint32_t size = gles2::cmds::Uniform1ivImmediate::ComputeSize(count);
  gles2::cmds::Uniform1ivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform1ivImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform1ui(GLint location, GLuint x) {
  gles2::cmds::Uniform1ui* c = GetCmdSpace<gles2::cmds::Uniform1ui>();
  if (c) {
    c->Init(location, x);
  }
}

void Uniform1uivImmediate(GLint location, GLsizei count, const GLuint* v) {
  const uint32_t size = gles2::cmds::Uniform1uivImmediate::ComputeSize(count);
  gles2::cmds::Uniform1uivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform1uivImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform2f(GLint location, GLfloat x, GLfloat y) {
  gles2::cmds::Uniform2f* c = GetCmdSpace<gles2::cmds::Uniform2f>();
  if (c) {
    c->Init(location, x, y);
  }
}

void Uniform2fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
  const uint32_t size = gles2::cmds::Uniform2fvImmediate::ComputeSize(count);
  gles2::cmds::Uniform2fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform2fvImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform2i(GLint location, GLint x, GLint y) {
  gles2::cmds::Uniform2i* c = GetCmdSpace<gles2::cmds::Uniform2i>();
  if (c) {
    c->Init(location, x, y);
  }
}

void Uniform2ivImmediate(GLint location, GLsizei count, const GLint* v) {
  const uint32_t size = gles2::cmds::Uniform2ivImmediate::ComputeSize(count);
  gles2::cmds::Uniform2ivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform2ivImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform2ui(GLint location, GLuint x, GLuint y) {
  gles2::cmds::Uniform2ui* c = GetCmdSpace<gles2::cmds::Uniform2ui>();
  if (c) {
    c->Init(location, x, y);
  }
}

void Uniform2uivImmediate(GLint location, GLsizei count, const GLuint* v) {
  const uint32_t size = gles2::cmds::Uniform2uivImmediate::ComputeSize(count);
  gles2::cmds::Uniform2uivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform2uivImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  gles2::cmds::Uniform3f* c = GetCmdSpace<gles2::cmds::Uniform3f>();
  if (c) {
    c->Init(location, x, y, z);
  }
}

void Uniform3fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
  const uint32_t size = gles2::cmds::Uniform3fvImmediate::ComputeSize(count);
  gles2::cmds::Uniform3fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform3fvImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform3i(GLint location, GLint x, GLint y, GLint z) {
  gles2::cmds::Uniform3i* c = GetCmdSpace<gles2::cmds::Uniform3i>();
  if (c) {
    c->Init(location, x, y, z);
  }
}

void Uniform3ivImmediate(GLint location, GLsizei count, const GLint* v) {
  const uint32_t size = gles2::cmds::Uniform3ivImmediate::ComputeSize(count);
  gles2::cmds::Uniform3ivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform3ivImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform3ui(GLint location, GLuint x, GLuint y, GLuint z) {
  gles2::cmds::Uniform3ui* c = GetCmdSpace<gles2::cmds::Uniform3ui>();
  if (c) {
    c->Init(location, x, y, z);
  }
}

void Uniform3uivImmediate(GLint location, GLsizei count, const GLuint* v) {
  const uint32_t size = gles2::cmds::Uniform3uivImmediate::ComputeSize(count);
  gles2::cmds::Uniform3uivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform3uivImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  gles2::cmds::Uniform4f* c = GetCmdSpace<gles2::cmds::Uniform4f>();
  if (c) {
    c->Init(location, x, y, z, w);
  }
}

void Uniform4fvImmediate(GLint location, GLsizei count, const GLfloat* v) {
  const uint32_t size = gles2::cmds::Uniform4fvImmediate::ComputeSize(count);
  gles2::cmds::Uniform4fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform4fvImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  gles2::cmds::Uniform4i* c = GetCmdSpace<gles2::cmds::Uniform4i>();
  if (c) {
    c->Init(location, x, y, z, w);
  }
}

void Uniform4ivImmediate(GLint location, GLsizei count, const GLint* v) {
  const uint32_t size = gles2::cmds::Uniform4ivImmediate::ComputeSize(count);
  gles2::cmds::Uniform4ivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform4ivImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void Uniform4ui(GLint location, GLuint x, GLuint y, GLuint z, GLuint w) {
  gles2::cmds::Uniform4ui* c = GetCmdSpace<gles2::cmds::Uniform4ui>();
  if (c) {
    c->Init(location, x, y, z, w);
  }
}

void Uniform4uivImmediate(GLint location, GLsizei count, const GLuint* v) {
  const uint32_t size = gles2::cmds::Uniform4uivImmediate::ComputeSize(count);
  gles2::cmds::Uniform4uivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::Uniform4uivImmediate>(size);
  if (c) {
    c->Init(location, count, v);
  }
}

void UniformBlockBinding(GLuint program, GLuint index, GLuint binding) {
  gles2::cmds::UniformBlockBinding* c =
      GetCmdSpace<gles2::cmds::UniformBlockBinding>();
  if (c) {
    c->Init(program, index, binding);
  }
}

void UniformMatrix2fvImmediate(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat* value) {
  const uint32_t size =
      gles2::cmds::UniformMatrix2fvImmediate::ComputeSize(count);
  gles2::cmds::UniformMatrix2fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix2fvImmediate>(
          size);
  if (c) {
    c->Init(location, count, transpose, value);
  }
}

void UniformMatrix2x3fvImmediate(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value) {
  const uint32_t size =
      gles2::cmds::UniformMatrix2x3fvImmediate::ComputeSize(count);
  gles2::cmds::UniformMatrix2x3fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix2x3fvImmediate>(
          size);
  if (c) {
    c->Init(location, count, transpose, value);
  }
}

void UniformMatrix2x4fvImmediate(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value) {
  const uint32_t size =
      gles2::cmds::UniformMatrix2x4fvImmediate::ComputeSize(count);
  gles2::cmds::UniformMatrix2x4fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix2x4fvImmediate>(
          size);
  if (c) {
    c->Init(location, count, transpose, value);
  }
}

void UniformMatrix3fvImmediate(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat* value) {
  const uint32_t size =
      gles2::cmds::UniformMatrix3fvImmediate::ComputeSize(count);
  gles2::cmds::UniformMatrix3fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix3fvImmediate>(
          size);
  if (c) {
    c->Init(location, count, transpose, value);
  }
}

void UniformMatrix3x2fvImmediate(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value) {
  const uint32_t size =
      gles2::cmds::UniformMatrix3x2fvImmediate::ComputeSize(count);
  gles2::cmds::UniformMatrix3x2fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix3x2fvImmediate>(
          size);
  if (c) {
    c->Init(location, count, transpose, value);
  }
}

void UniformMatrix3x4fvImmediate(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value) {
  const uint32_t size =
      gles2::cmds::UniformMatrix3x4fvImmediate::ComputeSize(count);
  gles2::cmds::UniformMatrix3x4fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix3x4fvImmediate>(
          size);
  if (c) {
    c->Init(location, count, transpose, value);
  }
}

void UniformMatrix4fvImmediate(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat* value) {
  const uint32_t size =
      gles2::cmds::UniformMatrix4fvImmediate::ComputeSize(count);
  gles2::cmds::UniformMatrix4fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix4fvImmediate>(
          size);
  if (c) {
    c->Init(location, count, transpose, value);
  }
}

void UniformMatrix4x2fvImmediate(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value) {
  const uint32_t size =
      gles2::cmds::UniformMatrix4x2fvImmediate::ComputeSize(count);
  gles2::cmds::UniformMatrix4x2fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix4x2fvImmediate>(
          size);
  if (c) {
    c->Init(location, count, transpose, value);
  }
}

void UniformMatrix4x3fvImmediate(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat* value) {
  const uint32_t size =
      gles2::cmds::UniformMatrix4x3fvImmediate::ComputeSize(count);
  gles2::cmds::UniformMatrix4x3fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::UniformMatrix4x3fvImmediate>(
          size);
  if (c) {
    c->Init(location, count, transpose, value);
  }
}

void UseProgram(GLuint program) {
  gles2::cmds::UseProgram* c = GetCmdSpace<gles2::cmds::UseProgram>();
  if (c) {
    c->Init(program);
  }
}

void ValidateProgram(GLuint program) {
  gles2::cmds::ValidateProgram* c = GetCmdSpace<gles2::cmds::ValidateProgram>();
  if (c) {
    c->Init(program);
  }
}

void VertexAttrib1f(GLuint indx, GLfloat x) {
  gles2::cmds::VertexAttrib1f* c = GetCmdSpace<gles2::cmds::VertexAttrib1f>();
  if (c) {
    c->Init(indx, x);
  }
}

void VertexAttrib1fvImmediate(GLuint indx, const GLfloat* values) {
  const uint32_t size = gles2::cmds::VertexAttrib1fvImmediate::ComputeSize();
  gles2::cmds::VertexAttrib1fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttrib1fvImmediate>(
          size);
  if (c) {
    c->Init(indx, values);
  }
}

void VertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  gles2::cmds::VertexAttrib2f* c = GetCmdSpace<gles2::cmds::VertexAttrib2f>();
  if (c) {
    c->Init(indx, x, y);
  }
}

void VertexAttrib2fvImmediate(GLuint indx, const GLfloat* values) {
  const uint32_t size = gles2::cmds::VertexAttrib2fvImmediate::ComputeSize();
  gles2::cmds::VertexAttrib2fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttrib2fvImmediate>(
          size);
  if (c) {
    c->Init(indx, values);
  }
}

void VertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  gles2::cmds::VertexAttrib3f* c = GetCmdSpace<gles2::cmds::VertexAttrib3f>();
  if (c) {
    c->Init(indx, x, y, z);
  }
}

void VertexAttrib3fvImmediate(GLuint indx, const GLfloat* values) {
  const uint32_t size = gles2::cmds::VertexAttrib3fvImmediate::ComputeSize();
  gles2::cmds::VertexAttrib3fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttrib3fvImmediate>(
          size);
  if (c) {
    c->Init(indx, values);
  }
}

void VertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  gles2::cmds::VertexAttrib4f* c = GetCmdSpace<gles2::cmds::VertexAttrib4f>();
  if (c) {
    c->Init(indx, x, y, z, w);
  }
}

void VertexAttrib4fvImmediate(GLuint indx, const GLfloat* values) {
  const uint32_t size = gles2::cmds::VertexAttrib4fvImmediate::ComputeSize();
  gles2::cmds::VertexAttrib4fvImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttrib4fvImmediate>(
          size);
  if (c) {
    c->Init(indx, values);
  }
}

void VertexAttribI4i(GLuint indx, GLint x, GLint y, GLint z, GLint w) {
  gles2::cmds::VertexAttribI4i* c = GetCmdSpace<gles2::cmds::VertexAttribI4i>();
  if (c) {
    c->Init(indx, x, y, z, w);
  }
}

void VertexAttribI4ivImmediate(GLuint indx, const GLint* values) {
  const uint32_t size = gles2::cmds::VertexAttribI4ivImmediate::ComputeSize();
  gles2::cmds::VertexAttribI4ivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttribI4ivImmediate>(
          size);
  if (c) {
    c->Init(indx, values);
  }
}

void VertexAttribI4ui(GLuint indx, GLuint x, GLuint y, GLuint z, GLuint w) {
  gles2::cmds::VertexAttribI4ui* c =
      GetCmdSpace<gles2::cmds::VertexAttribI4ui>();
  if (c) {
    c->Init(indx, x, y, z, w);
  }
}

void VertexAttribI4uivImmediate(GLuint indx, const GLuint* values) {
  const uint32_t size = gles2::cmds::VertexAttribI4uivImmediate::ComputeSize();
  gles2::cmds::VertexAttribI4uivImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::VertexAttribI4uivImmediate>(
          size);
  if (c) {
    c->Init(indx, values);
  }
}

void VertexAttribIPointer(GLuint indx,
                          GLint size,
                          GLenum type,
                          GLsizei stride,
                          GLuint offset) {
  gles2::cmds::VertexAttribIPointer* c =
      GetCmdSpace<gles2::cmds::VertexAttribIPointer>();
  if (c) {
    c->Init(indx, size, type, stride, offset);
  }
}

void VertexAttribPointer(GLuint indx,
                         GLint size,
                         GLenum type,
                         GLboolean normalized,
                         GLsizei stride,
                         GLuint offset) {
  gles2::cmds::VertexAttribPointer* c =
      GetCmdSpace<gles2::cmds::VertexAttribPointer>();
  if (c) {
    c->Init(indx, size, type, normalized, stride, offset);
  }
}

void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  gles2::cmds::Viewport* c = GetCmdSpace<gles2::cmds::Viewport>();
  if (c) {
    c->Init(x, y, width, height);
  }
}

void WaitSync(GLuint sync, GLbitfield flags, GLuint64 timeout) {
  gles2::cmds::WaitSync* c = GetCmdSpace<gles2::cmds::WaitSync>();
  if (c) {
    c->Init(sync, flags, timeout);
  }
}

void BlitFramebufferCHROMIUM(GLint srcX0,
                             GLint srcY0,
                             GLint srcX1,
                             GLint srcY1,
                             GLint dstX0,
                             GLint dstY0,
                             GLint dstX1,
                             GLint dstY1,
                             GLbitfield mask,
                             GLenum filter) {
  gles2::cmds::BlitFramebufferCHROMIUM* c =
      GetCmdSpace<gles2::cmds::BlitFramebufferCHROMIUM>();
  if (c) {
    c->Init(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask,
            filter);
  }
}

void RenderbufferStorageMultisampleCHROMIUM(GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  gles2::cmds::RenderbufferStorageMultisampleCHROMIUM* c =
      GetCmdSpace<gles2::cmds::RenderbufferStorageMultisampleCHROMIUM>();
  if (c) {
    c->Init(target, samples, internalformat, width, height);
  }
}

void RenderbufferStorageMultisampleAdvancedAMD(GLenum target,
                                               GLsizei samples,
                                               GLsizei storageSamples,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height) {
  gles2::cmds::RenderbufferStorageMultisampleAdvancedAMD* c =
      GetCmdSpace<gles2::cmds::RenderbufferStorageMultisampleAdvancedAMD>();
  if (c) {
    c->Init(target, samples, storageSamples, internalformat, width, height);
  }
}

void RenderbufferStorageMultisampleEXT(GLenum target,
                                       GLsizei samples,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height) {
  gles2::cmds::RenderbufferStorageMultisampleEXT* c =
      GetCmdSpace<gles2::cmds::RenderbufferStorageMultisampleEXT>();
  if (c) {
    c->Init(target, samples, internalformat, width, height);
  }
}

void FramebufferTexture2DMultisampleEXT(GLenum target,
                                        GLenum attachment,
                                        GLenum textarget,
                                        GLuint texture,
                                        GLint level,
                                        GLsizei samples) {
  gles2::cmds::FramebufferTexture2DMultisampleEXT* c =
      GetCmdSpace<gles2::cmds::FramebufferTexture2DMultisampleEXT>();
  if (c) {
    c->Init(target, attachment, textarget, texture, level, samples);
  }
}

void TexStorage2DEXT(GLenum target,
                     GLsizei levels,
                     GLenum internalFormat,
                     GLsizei width,
                     GLsizei height) {
  gles2::cmds::TexStorage2DEXT* c = GetCmdSpace<gles2::cmds::TexStorage2DEXT>();
  if (c) {
    c->Init(target, levels, internalFormat, width, height);
  }
}

void GenQueriesEXTImmediate(GLsizei n, GLuint* queries) {
  const uint32_t size = gles2::cmds::GenQueriesEXTImmediate::ComputeSize(n);
  gles2::cmds::GenQueriesEXTImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::GenQueriesEXTImmediate>(size);
  if (c) {
    c->Init(n, queries);
  }
}

void DeleteQueriesEXTImmediate(GLsizei n, const GLuint* queries) {
  const uint32_t size = gles2::cmds::DeleteQueriesEXTImmediate::ComputeSize(n);
  gles2::cmds::DeleteQueriesEXTImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::DeleteQueriesEXTImmediate>(
          size);
  if (c) {
    c->Init(n, queries);
  }
}

void QueryCounterEXT(GLuint id,
                     GLenum target,
                     uint32_t sync_data_shm_id,
                     uint32_t sync_data_shm_offset,
                     GLuint submit_count) {
  gles2::cmds::QueryCounterEXT* c = GetCmdSpace<gles2::cmds::QueryCounterEXT>();
  if (c) {
    c->Init(id, target, sync_data_shm_id, sync_data_shm_offset, submit_count);
  }
}

void BeginQueryEXT(GLenum target,
                   GLuint id,
                   uint32_t sync_data_shm_id,
                   uint32_t sync_data_shm_offset) {
  gles2::cmds::BeginQueryEXT* c = GetCmdSpace<gles2::cmds::BeginQueryEXT>();
  if (c) {
    c->Init(target, id, sync_data_shm_id, sync_data_shm_offset);
  }
}

void BeginTransformFeedback(GLenum primitivemode) {
  gles2::cmds::BeginTransformFeedback* c =
      GetCmdSpace<gles2::cmds::BeginTransformFeedback>();
  if (c) {
    c->Init(primitivemode);
  }
}

void EndQueryEXT(GLenum target, GLuint submit_count) {
  gles2::cmds::EndQueryEXT* c = GetCmdSpace<gles2::cmds::EndQueryEXT>();
  if (c) {
    c->Init(target, submit_count);
  }
}

void EndTransformFeedback() {
  gles2::cmds::EndTransformFeedback* c =
      GetCmdSpace<gles2::cmds::EndTransformFeedback>();
  if (c) {
    c->Init();
  }
}

void SetDisjointValueSyncCHROMIUM(uint32_t sync_data_shm_id,
                                  uint32_t sync_data_shm_offset) {
  gles2::cmds::SetDisjointValueSyncCHROMIUM* c =
      GetCmdSpace<gles2::cmds::SetDisjointValueSyncCHROMIUM>();
  if (c) {
    c->Init(sync_data_shm_id, sync_data_shm_offset);
  }
}

void InsertEventMarkerEXT(GLuint bucket_id) {
  gles2::cmds::InsertEventMarkerEXT* c =
      GetCmdSpace<gles2::cmds::InsertEventMarkerEXT>();
  if (c) {
    c->Init(bucket_id);
  }
}

void PushGroupMarkerEXT(GLuint bucket_id) {
  gles2::cmds::PushGroupMarkerEXT* c =
      GetCmdSpace<gles2::cmds::PushGroupMarkerEXT>();
  if (c) {
    c->Init(bucket_id);
  }
}

void PopGroupMarkerEXT() {
  gles2::cmds::PopGroupMarkerEXT* c =
      GetCmdSpace<gles2::cmds::PopGroupMarkerEXT>();
  if (c) {
    c->Init();
  }
}

void GenVertexArraysOESImmediate(GLsizei n, GLuint* arrays) {
  const uint32_t size =
      gles2::cmds::GenVertexArraysOESImmediate::ComputeSize(n);
  gles2::cmds::GenVertexArraysOESImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::GenVertexArraysOESImmediate>(
          size);
  if (c) {
    c->Init(n, arrays);
  }
}

void DeleteVertexArraysOESImmediate(GLsizei n, const GLuint* arrays) {
  const uint32_t size =
      gles2::cmds::DeleteVertexArraysOESImmediate::ComputeSize(n);
  gles2::cmds::DeleteVertexArraysOESImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::DeleteVertexArraysOESImmediate>(size);
  if (c) {
    c->Init(n, arrays);
  }
}

void IsVertexArrayOES(GLuint array,
                      uint32_t result_shm_id,
                      uint32_t result_shm_offset) {
  gles2::cmds::IsVertexArrayOES* c =
      GetCmdSpace<gles2::cmds::IsVertexArrayOES>();
  if (c) {
    c->Init(array, result_shm_id, result_shm_offset);
  }
}

void BindVertexArrayOES(GLuint array) {
  gles2::cmds::BindVertexArrayOES* c =
      GetCmdSpace<gles2::cmds::BindVertexArrayOES>();
  if (c) {
    c->Init(array);
  }
}

void FramebufferParameteri(GLenum target, GLenum pname, GLint param) {
  gles2::cmds::FramebufferParameteri* c =
      GetCmdSpace<gles2::cmds::FramebufferParameteri>();
  if (c) {
    c->Init(target, pname, param);
  }
}

void BindImageTexture(GLuint unit,
                      GLuint texture,
                      GLint level,
                      GLboolean layered,
                      GLint layer,
                      GLenum access,
                      GLenum format) {
  gles2::cmds::BindImageTexture* c =
      GetCmdSpace<gles2::cmds::BindImageTexture>();
  if (c) {
    c->Init(unit, texture, level, layered, layer, access, format);
  }
}

void DispatchCompute(GLuint num_groups_x,
                     GLuint num_groups_y,
                     GLuint num_groups_z) {
  gles2::cmds::DispatchCompute* c = GetCmdSpace<gles2::cmds::DispatchCompute>();
  if (c) {
    c->Init(num_groups_x, num_groups_y, num_groups_z);
  }
}

void DispatchComputeIndirect(GLintptr offset) {
  gles2::cmds::DispatchComputeIndirect* c =
      GetCmdSpace<gles2::cmds::DispatchComputeIndirect>();
  if (c) {
    c->Init(offset);
  }
}

void DrawArraysIndirect(GLenum mode, GLuint offset) {
  gles2::cmds::DrawArraysIndirect* c =
      GetCmdSpace<gles2::cmds::DrawArraysIndirect>();
  if (c) {
    c->Init(mode, offset);
  }
}

void DrawElementsIndirect(GLenum mode, GLenum type, GLuint offset) {
  gles2::cmds::DrawElementsIndirect* c =
      GetCmdSpace<gles2::cmds::DrawElementsIndirect>();
  if (c) {
    c->Init(mode, type, offset);
  }
}

void GetProgramInterfaceiv(GLuint program,
                           GLenum program_interface,
                           GLenum pname,
                           uint32_t params_shm_id,
                           uint32_t params_shm_offset) {
  gles2::cmds::GetProgramInterfaceiv* c =
      GetCmdSpace<gles2::cmds::GetProgramInterfaceiv>();
  if (c) {
    c->Init(program, program_interface, pname, params_shm_id,
            params_shm_offset);
  }
}

void GetProgramResourceIndex(GLuint program,
                             GLenum program_interface,
                             uint32_t name_bucket_id,
                             uint32_t index_shm_id,
                             uint32_t index_shm_offset) {
  gles2::cmds::GetProgramResourceIndex* c =
      GetCmdSpace<gles2::cmds::GetProgramResourceIndex>();
  if (c) {
    c->Init(program, program_interface, name_bucket_id, index_shm_id,
            index_shm_offset);
  }
}

void GetProgramResourceName(GLuint program,
                            GLenum program_interface,
                            GLuint index,
                            uint32_t name_bucket_id,
                            uint32_t result_shm_id,
                            uint32_t result_shm_offset) {
  gles2::cmds::GetProgramResourceName* c =
      GetCmdSpace<gles2::cmds::GetProgramResourceName>();
  if (c) {
    c->Init(program, program_interface, index, name_bucket_id, result_shm_id,
            result_shm_offset);
  }
}

void GetProgramResourceiv(GLuint program,
                          GLenum program_interface,
                          GLuint index,
                          uint32_t props_bucket_id,
                          uint32_t params_shm_id,
                          uint32_t params_shm_offset) {
  gles2::cmds::GetProgramResourceiv* c =
      GetCmdSpace<gles2::cmds::GetProgramResourceiv>();
  if (c) {
    c->Init(program, program_interface, index, props_bucket_id, params_shm_id,
            params_shm_offset);
  }
}

void GetProgramResourceLocation(GLuint program,
                                GLenum program_interface,
                                uint32_t name_bucket_id,
                                uint32_t location_shm_id,
                                uint32_t location_shm_offset) {
  gles2::cmds::GetProgramResourceLocation* c =
      GetCmdSpace<gles2::cmds::GetProgramResourceLocation>();
  if (c) {
    c->Init(program, program_interface, name_bucket_id, location_shm_id,
            location_shm_offset);
  }
}

void MemoryBarrierEXT(GLbitfield barriers) {
  gles2::cmds::MemoryBarrierEXT* c =
      GetCmdSpace<gles2::cmds::MemoryBarrierEXT>();
  if (c) {
    c->Init(barriers);
  }
}

void MemoryBarrierByRegion(GLbitfield barriers) {
  gles2::cmds::MemoryBarrierByRegion* c =
      GetCmdSpace<gles2::cmds::MemoryBarrierByRegion>();
  if (c) {
    c->Init(barriers);
  }
}

void SwapBuffers(GLuint64 swap_id, GLbitfield flags) {
  gles2::cmds::SwapBuffers* c = GetCmdSpace<gles2::cmds::SwapBuffers>();
  if (c) {
    c->Init(swap_id, flags);
  }
}

void GetMaxValueInBufferCHROMIUM(GLuint buffer_id,
                                 GLsizei count,
                                 GLenum type,
                                 GLuint offset,
                                 uint32_t result_shm_id,
                                 uint32_t result_shm_offset) {
  gles2::cmds::GetMaxValueInBufferCHROMIUM* c =
      GetCmdSpace<gles2::cmds::GetMaxValueInBufferCHROMIUM>();
  if (c) {
    c->Init(buffer_id, count, type, offset, result_shm_id, result_shm_offset);
  }
}

void EnableFeatureCHROMIUM(GLuint bucket_id,
                           uint32_t result_shm_id,
                           uint32_t result_shm_offset) {
  gles2::cmds::EnableFeatureCHROMIUM* c =
      GetCmdSpace<gles2::cmds::EnableFeatureCHROMIUM>();
  if (c) {
    c->Init(bucket_id, result_shm_id, result_shm_offset);
  }
}

void MapBufferRange(GLenum target,
                    GLintptr offset,
                    GLsizeiptr size,
                    GLbitfield access,
                    uint32_t data_shm_id,
                    uint32_t data_shm_offset,
                    uint32_t result_shm_id,
                    uint32_t result_shm_offset) {
  gles2::cmds::MapBufferRange* c = GetCmdSpace<gles2::cmds::MapBufferRange>();
  if (c) {
    c->Init(target, offset, size, access, data_shm_id, data_shm_offset,
            result_shm_id, result_shm_offset);
  }
}

void UnmapBuffer(GLenum target) {
  gles2::cmds::UnmapBuffer* c = GetCmdSpace<gles2::cmds::UnmapBuffer>();
  if (c) {
    c->Init(target);
  }
}

void FlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr size) {
  gles2::cmds::FlushMappedBufferRange* c =
      GetCmdSpace<gles2::cmds::FlushMappedBufferRange>();
  if (c) {
    c->Init(target, offset, size);
  }
}

void ResizeCHROMIUM(GLint width,
                    GLint height,
                    GLfloat scale_factor,
                    GLboolean alpha,
                    GLuint shm_id,
                    GLuint shm_offset,
                    GLsizei color_space_size) {
  gles2::cmds::ResizeCHROMIUM* c = GetCmdSpace<gles2::cmds::ResizeCHROMIUM>();
  if (c) {
    c->Init(width, height, scale_factor, alpha, shm_id, shm_offset,
            color_space_size);
  }
}

void GetRequestableExtensionsCHROMIUM(uint32_t bucket_id) {
  gles2::cmds::GetRequestableExtensionsCHROMIUM* c =
      GetCmdSpace<gles2::cmds::GetRequestableExtensionsCHROMIUM>();
  if (c) {
    c->Init(bucket_id);
  }
}

void RequestExtensionCHROMIUM(uint32_t bucket_id) {
  gles2::cmds::RequestExtensionCHROMIUM* c =
      GetCmdSpace<gles2::cmds::RequestExtensionCHROMIUM>();
  if (c) {
    c->Init(bucket_id);
  }
}

void GetProgramInfoCHROMIUM(GLuint program, uint32_t bucket_id) {
  gles2::cmds::GetProgramInfoCHROMIUM* c =
      GetCmdSpace<gles2::cmds::GetProgramInfoCHROMIUM>();
  if (c) {
    c->Init(program, bucket_id);
  }
}

void GetUniformBlocksCHROMIUM(GLuint program, uint32_t bucket_id) {
  gles2::cmds::GetUniformBlocksCHROMIUM* c =
      GetCmdSpace<gles2::cmds::GetUniformBlocksCHROMIUM>();
  if (c) {
    c->Init(program, bucket_id);
  }
}

void GetTransformFeedbackVaryingsCHROMIUM(GLuint program, uint32_t bucket_id) {
  gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM* c =
      GetCmdSpace<gles2::cmds::GetTransformFeedbackVaryingsCHROMIUM>();
  if (c) {
    c->Init(program, bucket_id);
  }
}

void GetUniformsES3CHROMIUM(GLuint program, uint32_t bucket_id) {
  gles2::cmds::GetUniformsES3CHROMIUM* c =
      GetCmdSpace<gles2::cmds::GetUniformsES3CHROMIUM>();
  if (c) {
    c->Init(program, bucket_id);
  }
}

void DescheduleUntilFinishedCHROMIUM() {
  gles2::cmds::DescheduleUntilFinishedCHROMIUM* c =
      GetCmdSpace<gles2::cmds::DescheduleUntilFinishedCHROMIUM>();
  if (c) {
    c->Init();
  }
}

void GetTranslatedShaderSourceANGLE(GLuint shader, uint32_t bucket_id) {
  gles2::cmds::GetTranslatedShaderSourceANGLE* c =
      GetCmdSpace<gles2::cmds::GetTranslatedShaderSourceANGLE>();
  if (c) {
    c->Init(shader, bucket_id);
  }
}

void CopyTextureCHROMIUM(GLuint source_id,
                         GLint source_level,
                         GLenum dest_target,
                         GLuint dest_id,
                         GLint dest_level,
                         GLint internalformat,
                         GLenum dest_type,
                         GLboolean unpack_flip_y,
                         GLboolean unpack_premultiply_alpha,
                         GLboolean unpack_unmultiply_alpha) {
  gles2::cmds::CopyTextureCHROMIUM* c =
      GetCmdSpace<gles2::cmds::CopyTextureCHROMIUM>();
  if (c) {
    c->Init(source_id, source_level, dest_target, dest_id, dest_level,
            internalformat, dest_type, unpack_flip_y, unpack_premultiply_alpha,
            unpack_unmultiply_alpha);
  }
}

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
                            GLboolean unpack_unmultiply_alpha) {
  gles2::cmds::CopySubTextureCHROMIUM* c =
      GetCmdSpace<gles2::cmds::CopySubTextureCHROMIUM>();
  if (c) {
    c->Init(source_id, source_level, dest_target, dest_id, dest_level, xoffset,
            yoffset, x, y, width, height, unpack_flip_y,
            unpack_premultiply_alpha, unpack_unmultiply_alpha);
  }
}

void DrawArraysInstancedANGLE(GLenum mode,
                              GLint first,
                              GLsizei count,
                              GLsizei primcount) {
  gles2::cmds::DrawArraysInstancedANGLE* c =
      GetCmdSpace<gles2::cmds::DrawArraysInstancedANGLE>();
  if (c) {
    c->Init(mode, first, count, primcount);
  }
}

void DrawArraysInstancedBaseInstanceANGLE(GLenum mode,
                                          GLint first,
                                          GLsizei count,
                                          GLsizei primcount,
                                          GLuint baseinstance) {
  gles2::cmds::DrawArraysInstancedBaseInstanceANGLE* c =
      GetCmdSpace<gles2::cmds::DrawArraysInstancedBaseInstanceANGLE>();
  if (c) {
    c->Init(mode, first, count, primcount, baseinstance);
  }
}

void DrawElementsInstancedANGLE(GLenum mode,
                                GLsizei count,
                                GLenum type,
                                GLuint index_offset,
                                GLsizei primcount) {
  gles2::cmds::DrawElementsInstancedANGLE* c =
      GetCmdSpace<gles2::cmds::DrawElementsInstancedANGLE>();
  if (c) {
    c->Init(mode, count, type, index_offset, primcount);
  }
}

void DrawElementsInstancedBaseVertexBaseInstanceANGLE(GLenum mode,
                                                      GLsizei count,
                                                      GLenum type,
                                                      GLuint index_offset,
                                                      GLsizei primcount,
                                                      GLint basevertex,
                                                      GLuint baseinstance) {
  gles2::cmds::DrawElementsInstancedBaseVertexBaseInstanceANGLE* c =
      GetCmdSpace<
          gles2::cmds::DrawElementsInstancedBaseVertexBaseInstanceANGLE>();
  if (c) {
    c->Init(mode, count, type, index_offset, primcount, basevertex,
            baseinstance);
  }
}

void VertexAttribDivisorANGLE(GLuint index, GLuint divisor) {
  gles2::cmds::VertexAttribDivisorANGLE* c =
      GetCmdSpace<gles2::cmds::VertexAttribDivisorANGLE>();
  if (c) {
    c->Init(index, divisor);
  }
}

void BindUniformLocationCHROMIUMBucket(GLuint program,
                                       GLint location,
                                       uint32_t name_bucket_id) {
  gles2::cmds::BindUniformLocationCHROMIUMBucket* c =
      GetCmdSpace<gles2::cmds::BindUniformLocationCHROMIUMBucket>();
  if (c) {
    c->Init(program, location, name_bucket_id);
  }
}

void TraceBeginCHROMIUM(GLuint category_bucket_id, GLuint name_bucket_id) {
  gles2::cmds::TraceBeginCHROMIUM* c =
      GetCmdSpace<gles2::cmds::TraceBeginCHROMIUM>();
  if (c) {
    c->Init(category_bucket_id, name_bucket_id);
  }
}

void TraceEndCHROMIUM() {
  gles2::cmds::TraceEndCHROMIUM* c =
      GetCmdSpace<gles2::cmds::TraceEndCHROMIUM>();
  if (c) {
    c->Init();
  }
}

void DiscardFramebufferEXTImmediate(GLenum target,
                                    GLsizei count,
                                    const GLenum* attachments) {
  const uint32_t size =
      gles2::cmds::DiscardFramebufferEXTImmediate::ComputeSize(count);
  gles2::cmds::DiscardFramebufferEXTImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::DiscardFramebufferEXTImmediate>(size);
  if (c) {
    c->Init(target, count, attachments);
  }
}

void LoseContextCHROMIUM(GLenum current, GLenum other) {
  gles2::cmds::LoseContextCHROMIUM* c =
      GetCmdSpace<gles2::cmds::LoseContextCHROMIUM>();
  if (c) {
    c->Init(current, other);
  }
}

void DrawBuffersEXTImmediate(GLsizei count, const GLenum* bufs) {
  const uint32_t size =
      gles2::cmds::DrawBuffersEXTImmediate::ComputeSize(count);
  gles2::cmds::DrawBuffersEXTImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::DrawBuffersEXTImmediate>(size);
  if (c) {
    c->Init(count, bufs);
  }
}

void FlushDriverCachesCHROMIUM() {
  gles2::cmds::FlushDriverCachesCHROMIUM* c =
      GetCmdSpace<gles2::cmds::FlushDriverCachesCHROMIUM>();
  if (c) {
    c->Init();
  }
}

void SetActiveURLCHROMIUM(GLuint url_bucket_id) {
  gles2::cmds::SetActiveURLCHROMIUM* c =
      GetCmdSpace<gles2::cmds::SetActiveURLCHROMIUM>();
  if (c) {
    c->Init(url_bucket_id);
  }
}

void ContextVisibilityHintCHROMIUM(GLboolean visibility) {
  gles2::cmds::ContextVisibilityHintCHROMIUM* c =
      GetCmdSpace<gles2::cmds::ContextVisibilityHintCHROMIUM>();
  if (c) {
    c->Init(visibility);
  }
}

void BlendBarrierKHR() {
  gles2::cmds::BlendBarrierKHR* c = GetCmdSpace<gles2::cmds::BlendBarrierKHR>();
  if (c) {
    c->Init();
  }
}

void BindFragDataLocationIndexedEXTBucket(GLuint program,
                                          GLuint colorNumber,
                                          GLuint index,
                                          uint32_t name_bucket_id) {
  gles2::cmds::BindFragDataLocationIndexedEXTBucket* c =
      GetCmdSpace<gles2::cmds::BindFragDataLocationIndexedEXTBucket>();
  if (c) {
    c->Init(program, colorNumber, index, name_bucket_id);
  }
}

void BindFragDataLocationEXTBucket(GLuint program,
                                   GLuint colorNumber,
                                   uint32_t name_bucket_id) {
  gles2::cmds::BindFragDataLocationEXTBucket* c =
      GetCmdSpace<gles2::cmds::BindFragDataLocationEXTBucket>();
  if (c) {
    c->Init(program, colorNumber, name_bucket_id);
  }
}

void GetFragDataIndexEXT(GLuint program,
                         uint32_t name_bucket_id,
                         uint32_t index_shm_id,
                         uint32_t index_shm_offset) {
  gles2::cmds::GetFragDataIndexEXT* c =
      GetCmdSpace<gles2::cmds::GetFragDataIndexEXT>();
  if (c) {
    c->Init(program, name_bucket_id, index_shm_id, index_shm_offset);
  }
}

void InitializeDiscardableTextureCHROMIUM(GLuint texture_id,
                                          uint32_t shm_id,
                                          uint32_t shm_offset) {
  gles2::cmds::InitializeDiscardableTextureCHROMIUM* c =
      GetCmdSpace<gles2::cmds::InitializeDiscardableTextureCHROMIUM>();
  if (c) {
    c->Init(texture_id, shm_id, shm_offset);
  }
}

void UnlockDiscardableTextureCHROMIUM(GLuint texture_id) {
  gles2::cmds::UnlockDiscardableTextureCHROMIUM* c =
      GetCmdSpace<gles2::cmds::UnlockDiscardableTextureCHROMIUM>();
  if (c) {
    c->Init(texture_id);
  }
}

void LockDiscardableTextureCHROMIUM(GLuint texture_id) {
  gles2::cmds::LockDiscardableTextureCHROMIUM* c =
      GetCmdSpace<gles2::cmds::LockDiscardableTextureCHROMIUM>();
  if (c) {
    c->Init(texture_id);
  }
}

void WindowRectanglesEXTImmediate(GLenum mode,
                                  GLsizei count,
                                  const GLint* box) {
  const uint32_t size =
      gles2::cmds::WindowRectanglesEXTImmediate::ComputeSize(count);
  gles2::cmds::WindowRectanglesEXTImmediate* c =
      GetImmediateCmdSpaceTotalSize<gles2::cmds::WindowRectanglesEXTImmediate>(
          size);
  if (c) {
    c->Init(mode, count, box);
  }
}

void CreateGpuFenceINTERNAL(GLuint gpu_fence_id) {
  gles2::cmds::CreateGpuFenceINTERNAL* c =
      GetCmdSpace<gles2::cmds::CreateGpuFenceINTERNAL>();
  if (c) {
    c->Init(gpu_fence_id);
  }
}

void WaitGpuFenceCHROMIUM(GLuint gpu_fence_id) {
  gles2::cmds::WaitGpuFenceCHROMIUM* c =
      GetCmdSpace<gles2::cmds::WaitGpuFenceCHROMIUM>();
  if (c) {
    c->Init(gpu_fence_id);
  }
}

void DestroyGpuFenceCHROMIUM(GLuint gpu_fence_id) {
  gles2::cmds::DestroyGpuFenceCHROMIUM* c =
      GetCmdSpace<gles2::cmds::DestroyGpuFenceCHROMIUM>();
  if (c) {
    c->Init(gpu_fence_id);
  }
}

void SetReadbackBufferShadowAllocationINTERNAL(GLuint buffer_id,
                                               GLint shm_id,
                                               GLuint shm_offset,
                                               GLuint size) {
  gles2::cmds::SetReadbackBufferShadowAllocationINTERNAL* c =
      GetCmdSpace<gles2::cmds::SetReadbackBufferShadowAllocationINTERNAL>();
  if (c) {
    c->Init(buffer_id, shm_id, shm_offset, size);
  }
}

void FramebufferTextureMultiviewOVR(GLenum target,
                                    GLenum attachment,
                                    GLuint texture,
                                    GLint level,
                                    GLint baseViewIndex,
                                    GLsizei numViews) {
  gles2::cmds::FramebufferTextureMultiviewOVR* c =
      GetCmdSpace<gles2::cmds::FramebufferTextureMultiviewOVR>();
  if (c) {
    c->Init(target, attachment, texture, level, baseViewIndex, numViews);
  }
}

void MaxShaderCompilerThreadsKHR(GLuint count) {
  gles2::cmds::MaxShaderCompilerThreadsKHR* c =
      GetCmdSpace<gles2::cmds::MaxShaderCompilerThreadsKHR>();
  if (c) {
    c->Init(count);
  }
}

void CreateAndTexStorage2DSharedImageINTERNALImmediate(GLuint texture,
                                                       const GLbyte* mailbox) {
  const uint32_t size = gles2::cmds::
      CreateAndTexStorage2DSharedImageINTERNALImmediate::ComputeSize();
  gles2::cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>(size);
  if (c) {
    c->Init(texture, mailbox);
  }
}

void BeginSharedImageAccessDirectCHROMIUM(GLuint texture, GLenum mode) {
  gles2::cmds::BeginSharedImageAccessDirectCHROMIUM* c =
      GetCmdSpace<gles2::cmds::BeginSharedImageAccessDirectCHROMIUM>();
  if (c) {
    c->Init(texture, mode);
  }
}

void EndSharedImageAccessDirectCHROMIUM(GLuint texture) {
  gles2::cmds::EndSharedImageAccessDirectCHROMIUM* c =
      GetCmdSpace<gles2::cmds::EndSharedImageAccessDirectCHROMIUM>();
  if (c) {
    c->Init(texture);
  }
}

void CopySharedImageINTERNALImmediate(GLint xoffset,
                                      GLint yoffset,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height,
                                      GLboolean unpack_flip_y,
                                      const GLbyte* mailboxes) {
  const uint32_t size =
      gles2::cmds::CopySharedImageINTERNALImmediate::ComputeSize();
  gles2::cmds::CopySharedImageINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::CopySharedImageINTERNALImmediate>(size);
  if (c) {
    c->Init(xoffset, yoffset, x, y, width, height, unpack_flip_y, mailboxes);
  }
}

void CopySharedImageToTextureINTERNALImmediate(GLuint texture,
                                               GLenum target,
                                               GLuint internal_format,
                                               GLenum type,
                                               GLint src_x,
                                               GLint src_y,
                                               GLsizei width,
                                               GLsizei height,
                                               GLboolean flip_y,
                                               const GLbyte* src_mailbox) {
  const uint32_t size =
      gles2::cmds::CopySharedImageToTextureINTERNALImmediate::ComputeSize();
  gles2::cmds::CopySharedImageToTextureINTERNALImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::CopySharedImageToTextureINTERNALImmediate>(size);
  if (c) {
    c->Init(texture, target, internal_format, type, src_x, src_y, width, height,
            flip_y, src_mailbox);
  }
}

void ReadbackARGBImagePixelsINTERNAL(GLint src_x,
                                     GLint src_y,
                                     GLint plane_index,
                                     GLuint dst_width,
                                     GLuint dst_height,
                                     GLuint row_bytes,
                                     GLuint dst_sk_color_type,
                                     GLuint dst_sk_alpha_type,
                                     GLint shm_id,
                                     GLuint shm_offset,
                                     GLuint color_space_offset,
                                     GLuint pixels_offset,
                                     GLuint mailbox_offset) {
  gles2::cmds::ReadbackARGBImagePixelsINTERNAL* c =
      GetCmdSpace<gles2::cmds::ReadbackARGBImagePixelsINTERNAL>();
  if (c) {
    c->Init(src_x, src_y, plane_index, dst_width, dst_height, row_bytes,
            dst_sk_color_type, dst_sk_alpha_type, shm_id, shm_offset,
            color_space_offset, pixels_offset, mailbox_offset);
  }
}

void WritePixelsYUVINTERNAL(GLuint src_width,
                            GLuint src_height,
                            GLuint src_row_bytes_plane1,
                            GLuint src_row_bytes_plane2,
                            GLuint src_row_bytes_plane3,
                            GLuint src_row_bytes_plane4,
                            GLuint src_yuv_plane_config,
                            GLuint src_yuv_subsampling,
                            GLuint src_yuv_datatype,
                            GLint shm_id,
                            GLuint shm_offset,
                            GLuint pixels_offset_plane1,
                            GLuint pixels_offset_plane2,
                            GLuint pixels_offset_plane3,
                            GLuint pixels_offset_plane4) {
  gles2::cmds::WritePixelsYUVINTERNAL* c =
      GetCmdSpace<gles2::cmds::WritePixelsYUVINTERNAL>();
  if (c) {
    c->Init(src_width, src_height, src_row_bytes_plane1, src_row_bytes_plane2,
            src_row_bytes_plane3, src_row_bytes_plane4, src_yuv_plane_config,
            src_yuv_subsampling, src_yuv_datatype, shm_id, shm_offset,
            pixels_offset_plane1, pixels_offset_plane2, pixels_offset_plane3,
            pixels_offset_plane4);
  }
}

void EnableiOES(GLenum target, GLuint index) {
  gles2::cmds::EnableiOES* c = GetCmdSpace<gles2::cmds::EnableiOES>();
  if (c) {
    c->Init(target, index);
  }
}

void DisableiOES(GLenum target, GLuint index) {
  gles2::cmds::DisableiOES* c = GetCmdSpace<gles2::cmds::DisableiOES>();
  if (c) {
    c->Init(target, index);
  }
}

void BlendEquationiOES(GLuint buf, GLenum mode) {
  gles2::cmds::BlendEquationiOES* c =
      GetCmdSpace<gles2::cmds::BlendEquationiOES>();
  if (c) {
    c->Init(buf, mode);
  }
}

void BlendEquationSeparateiOES(GLuint buf, GLenum modeRGB, GLenum modeAlpha) {
  gles2::cmds::BlendEquationSeparateiOES* c =
      GetCmdSpace<gles2::cmds::BlendEquationSeparateiOES>();
  if (c) {
    c->Init(buf, modeRGB, modeAlpha);
  }
}

void BlendFunciOES(GLuint buf, GLenum src, GLenum dst) {
  gles2::cmds::BlendFunciOES* c = GetCmdSpace<gles2::cmds::BlendFunciOES>();
  if (c) {
    c->Init(buf, src, dst);
  }
}

void BlendFuncSeparateiOES(GLuint buf,
                           GLenum srcRGB,
                           GLenum dstRGB,
                           GLenum srcAlpha,
                           GLenum dstAlpha) {
  gles2::cmds::BlendFuncSeparateiOES* c =
      GetCmdSpace<gles2::cmds::BlendFuncSeparateiOES>();
  if (c) {
    c->Init(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
  }
}

void ColorMaskiOES(GLuint buf,
                   GLboolean r,
                   GLboolean g,
                   GLboolean b,
                   GLboolean a) {
  gles2::cmds::ColorMaskiOES* c = GetCmdSpace<gles2::cmds::ColorMaskiOES>();
  if (c) {
    c->Init(buf, r, g, b, a);
  }
}

void IsEnablediOES(GLenum target,
                   GLuint index,
                   uint32_t result_shm_id,
                   uint32_t result_shm_offset) {
  gles2::cmds::IsEnablediOES* c = GetCmdSpace<gles2::cmds::IsEnablediOES>();
  if (c) {
    c->Init(target, index, result_shm_id, result_shm_offset);
  }
}

void ProvokingVertexANGLE(GLenum provokeMode) {
  gles2::cmds::ProvokingVertexANGLE* c =
      GetCmdSpace<gles2::cmds::ProvokingVertexANGLE>();
  if (c) {
    c->Init(provokeMode);
  }
}

void FramebufferMemorylessPixelLocalStorageANGLE(GLint plane,
                                                 GLenum internalformat) {
  gles2::cmds::FramebufferMemorylessPixelLocalStorageANGLE* c =
      GetCmdSpace<gles2::cmds::FramebufferMemorylessPixelLocalStorageANGLE>();
  if (c) {
    c->Init(plane, internalformat);
  }
}

void FramebufferTexturePixelLocalStorageANGLE(GLint plane,
                                              GLuint backingtexture,
                                              GLint level,
                                              GLint layer) {
  gles2::cmds::FramebufferTexturePixelLocalStorageANGLE* c =
      GetCmdSpace<gles2::cmds::FramebufferTexturePixelLocalStorageANGLE>();
  if (c) {
    c->Init(plane, backingtexture, level, layer);
  }
}

void FramebufferPixelLocalClearValuefvANGLEImmediate(GLint plane,
                                                     const GLfloat* value) {
  const uint32_t size = gles2::cmds::
      FramebufferPixelLocalClearValuefvANGLEImmediate::ComputeSize();
  gles2::cmds::FramebufferPixelLocalClearValuefvANGLEImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::FramebufferPixelLocalClearValuefvANGLEImmediate>(size);
  if (c) {
    c->Init(plane, value);
  }
}

void FramebufferPixelLocalClearValueivANGLEImmediate(GLint plane,
                                                     const GLint* value) {
  const uint32_t size = gles2::cmds::
      FramebufferPixelLocalClearValueivANGLEImmediate::ComputeSize();
  gles2::cmds::FramebufferPixelLocalClearValueivANGLEImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::FramebufferPixelLocalClearValueivANGLEImmediate>(size);
  if (c) {
    c->Init(plane, value);
  }
}

void FramebufferPixelLocalClearValueuivANGLEImmediate(GLint plane,
                                                      const GLuint* value) {
  const uint32_t size = gles2::cmds::
      FramebufferPixelLocalClearValueuivANGLEImmediate::ComputeSize();
  gles2::cmds::FramebufferPixelLocalClearValueuivANGLEImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::FramebufferPixelLocalClearValueuivANGLEImmediate>(size);
  if (c) {
    c->Init(plane, value);
  }
}

void BeginPixelLocalStorageANGLEImmediate(GLsizei count,
                                          const GLenum* loadops) {
  const uint32_t size =
      gles2::cmds::BeginPixelLocalStorageANGLEImmediate::ComputeSize(count);
  gles2::cmds::BeginPixelLocalStorageANGLEImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::BeginPixelLocalStorageANGLEImmediate>(size);
  if (c) {
    c->Init(count, loadops);
  }
}

void EndPixelLocalStorageANGLEImmediate(GLsizei count, const GLenum* storeops) {
  const uint32_t size =
      gles2::cmds::EndPixelLocalStorageANGLEImmediate::ComputeSize(count);
  gles2::cmds::EndPixelLocalStorageANGLEImmediate* c =
      GetImmediateCmdSpaceTotalSize<
          gles2::cmds::EndPixelLocalStorageANGLEImmediate>(size);
  if (c) {
    c->Init(count, storeops);
  }
}

void PixelLocalStorageBarrierANGLE() {
  gles2::cmds::PixelLocalStorageBarrierANGLE* c =
      GetCmdSpace<gles2::cmds::PixelLocalStorageBarrierANGLE>();
  if (c) {
    c->Init();
  }
}

void FramebufferPixelLocalStorageInterruptANGLE() {
  gles2::cmds::FramebufferPixelLocalStorageInterruptANGLE* c =
      GetCmdSpace<gles2::cmds::FramebufferPixelLocalStorageInterruptANGLE>();
  if (c) {
    c->Init();
  }
}

void FramebufferPixelLocalStorageRestoreANGLE() {
  gles2::cmds::FramebufferPixelLocalStorageRestoreANGLE* c =
      GetCmdSpace<gles2::cmds::FramebufferPixelLocalStorageRestoreANGLE>();
  if (c) {
    c->Init();
  }
}

void GetFramebufferPixelLocalStorageParameterfvANGLE(
    GLint plane,
    GLenum pname,
    uint32_t params_shm_id,
    uint32_t params_shm_offset) {
  gles2::cmds::GetFramebufferPixelLocalStorageParameterfvANGLE* c = GetCmdSpace<
      gles2::cmds::GetFramebufferPixelLocalStorageParameterfvANGLE>();
  if (c) {
    c->Init(plane, pname, params_shm_id, params_shm_offset);
  }
}

void GetFramebufferPixelLocalStorageParameterivANGLE(
    GLint plane,
    GLenum pname,
    uint32_t params_shm_id,
    uint32_t params_shm_offset) {
  gles2::cmds::GetFramebufferPixelLocalStorageParameterivANGLE* c = GetCmdSpace<
      gles2::cmds::GetFramebufferPixelLocalStorageParameterivANGLE>();
  if (c) {
    c->Init(plane, pname, params_shm_id, params_shm_offset);
  }
}

void ClipControlEXT(GLenum origin, GLenum depth) {
  gles2::cmds::ClipControlEXT* c = GetCmdSpace<gles2::cmds::ClipControlEXT>();
  if (c) {
    c->Init(origin, depth);
  }
}

void PolygonModeANGLE(GLenum face, GLenum mode) {
  gles2::cmds::PolygonModeANGLE* c =
      GetCmdSpace<gles2::cmds::PolygonModeANGLE>();
  if (c) {
    c->Init(face, mode);
  }
}

void PolygonOffsetClampEXT(GLfloat factor, GLfloat units, GLfloat clamp) {
  gles2::cmds::PolygonOffsetClampEXT* c =
      GetCmdSpace<gles2::cmds::PolygonOffsetClampEXT>();
  if (c) {
    c->Init(factor, units, clamp);
  }
}

#endif  // GPU_COMMAND_BUFFER_CLIENT_GLES2_CMD_HELPER_AUTOGEN_H_
