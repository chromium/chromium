// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

// It is included by gles2_cmd_decoder.cc
#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_AUTOGEN_H_

error::Error GLES2DecoderImpl::HandleActiveTexture(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ActiveTexture& c =
      *static_cast<const volatile gles2::cmds::ActiveTexture*>(cmd_data);
  GLenum texture = static_cast<GLenum>(c.texture);
  DoActiveTexture(texture);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleAttachShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::AttachShader& c =
      *static_cast<const volatile gles2::cmds::AttachShader*>(cmd_data);
  GLuint program = c.program;
  GLuint shader = c.shader;
  DoAttachShader(program, shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindBuffer(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile gles2::cmds::BindBuffer& c =
      *static_cast<const volatile gles2::cmds::BindBuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint buffer = c.buffer;
  if (!validators_->buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBindBuffer", target, "target");
    return error::kNoError;
  }
  DoBindBuffer(target, buffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindBufferBase(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::BindBufferBase& c =
      *static_cast<const volatile gles2::cmds::BindBufferBase*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint index = static_cast<GLuint>(c.index);
  GLuint buffer = c.buffer;
  if (!validators_->indexed_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBindBufferBase", target, "target");
    return error::kNoError;
  }
  DoBindBufferBase(target, index, buffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindBufferRange(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::BindBufferRange& c =
      *static_cast<const volatile gles2::cmds::BindBufferRange*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint index = static_cast<GLuint>(c.index);
  GLuint buffer = c.buffer;
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  if (!validators_->indexed_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBindBufferRange", target, "target");
    return error::kNoError;
  }
  if (size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glBindBufferRange", "size < 0");
    return error::kNoError;
  }
  DoBindBufferRange(target, index, buffer, offset, size);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindFramebuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindFramebuffer& c =
      *static_cast<const volatile gles2::cmds::BindFramebuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint framebuffer = c.framebuffer;
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBindFramebuffer", target, "target");
    return error::kNoError;
  }
  DoBindFramebuffer(target, framebuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindRenderbuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindRenderbuffer& c =
      *static_cast<const volatile gles2::cmds::BindRenderbuffer*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint renderbuffer = c.renderbuffer;
  if (!validators_->render_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBindRenderbuffer", target, "target");
    return error::kNoError;
  }
  DoBindRenderbuffer(target, renderbuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindSampler(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::BindSampler& c =
      *static_cast<const volatile gles2::cmds::BindSampler*>(cmd_data);
  GLuint unit = static_cast<GLuint>(c.unit);
  GLuint sampler = c.sampler;
  DoBindSampler(unit, sampler);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindTexture(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindTexture& c =
      *static_cast<const volatile gles2::cmds::BindTexture*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint texture = c.texture;
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBindTexture", target, "target");
    return error::kNoError;
  }
  DoBindTexture(target, texture);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::BindTransformFeedback& c =
      *static_cast<const volatile gles2::cmds::BindTransformFeedback*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLuint transformfeedback = c.transformfeedback;
  if (!validators_->transform_feedback_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBindTransformFeedback", target,
                                    "target");
    return error::kNoError;
  }
  DoBindTransformFeedback(target, transformfeedback);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendColor(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendColor& c =
      *static_cast<const volatile gles2::cmds::BlendColor*>(cmd_data);
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  if (state_.blend_color_red != red || state_.blend_color_green != green ||
      state_.blend_color_blue != blue || state_.blend_color_alpha != alpha) {
    state_.blend_color_red = red;
    state_.blend_color_green = green;
    state_.blend_color_blue = blue;
    state_.blend_color_alpha = alpha;
    api()->glBlendColorFn(red, green, blue, alpha);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendEquation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendEquation& c =
      *static_cast<const volatile gles2::cmds::BlendEquation*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->equation.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlendEquation", mode, "mode");
    return error::kNoError;
  }
  if (state_.blend_equation_rgb != mode ||
      state_.blend_equation_alpha != mode) {
    state_.blend_equation_rgb = mode;
    state_.blend_equation_alpha = mode;
    api()->glBlendEquationFn(mode);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendEquationSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendEquationSeparate& c =
      *static_cast<const volatile gles2::cmds::BlendEquationSeparate*>(
          cmd_data);
  GLenum modeRGB = static_cast<GLenum>(c.modeRGB);
  GLenum modeAlpha = static_cast<GLenum>(c.modeAlpha);
  if (!validators_->equation.IsValid(modeRGB)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlendEquationSeparate", modeRGB,
                                    "modeRGB");
    return error::kNoError;
  }
  if (!validators_->equation.IsValid(modeAlpha)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlendEquationSeparate", modeAlpha,
                                    "modeAlpha");
    return error::kNoError;
  }
  if (state_.blend_equation_rgb != modeRGB ||
      state_.blend_equation_alpha != modeAlpha) {
    state_.blend_equation_rgb = modeRGB;
    state_.blend_equation_alpha = modeAlpha;
    api()->glBlendEquationSeparateFn(modeRGB, modeAlpha);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendFunc(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendFunc& c =
      *static_cast<const volatile gles2::cmds::BlendFunc*>(cmd_data);
  GLenum sfactor = static_cast<GLenum>(c.sfactor);
  GLenum dfactor = static_cast<GLenum>(c.dfactor);
  if (!validators_->src_blend_factor.IsValid(sfactor)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlendFunc", sfactor, "sfactor");
    return error::kNoError;
  }
  if (!validators_->dst_blend_factor.IsValid(dfactor)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlendFunc", dfactor, "dfactor");
    return error::kNoError;
  }
  if (state_.blend_source_rgb != sfactor || state_.blend_dest_rgb != dfactor ||
      state_.blend_source_alpha != sfactor ||
      state_.blend_dest_alpha != dfactor) {
    state_.blend_source_rgb = sfactor;
    state_.blend_dest_rgb = dfactor;
    state_.blend_source_alpha = sfactor;
    state_.blend_dest_alpha = dfactor;
    api()->glBlendFuncFn(sfactor, dfactor);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendFuncSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlendFuncSeparate& c =
      *static_cast<const volatile gles2::cmds::BlendFuncSeparate*>(cmd_data);
  GLenum srcRGB = static_cast<GLenum>(c.srcRGB);
  GLenum dstRGB = static_cast<GLenum>(c.dstRGB);
  GLenum srcAlpha = static_cast<GLenum>(c.srcAlpha);
  GLenum dstAlpha = static_cast<GLenum>(c.dstAlpha);
  if (!validators_->src_blend_factor.IsValid(srcRGB)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlendFuncSeparate", srcRGB, "srcRGB");
    return error::kNoError;
  }
  if (!validators_->dst_blend_factor.IsValid(dstRGB)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlendFuncSeparate", dstRGB, "dstRGB");
    return error::kNoError;
  }
  if (!validators_->src_blend_factor.IsValid(srcAlpha)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlendFuncSeparate", srcAlpha,
                                    "srcAlpha");
    return error::kNoError;
  }
  if (!validators_->dst_blend_factor.IsValid(dstAlpha)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlendFuncSeparate", dstAlpha,
                                    "dstAlpha");
    return error::kNoError;
  }
  if (state_.blend_source_rgb != srcRGB || state_.blend_dest_rgb != dstRGB ||
      state_.blend_source_alpha != srcAlpha ||
      state_.blend_dest_alpha != dstAlpha) {
    state_.blend_source_rgb = srcRGB;
    state_.blend_dest_rgb = dstRGB;
    state_.blend_source_alpha = srcAlpha;
    state_.blend_dest_alpha = dstAlpha;
    api()->glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBufferSubData(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BufferSubData& c =
      *static_cast<const volatile gles2::cmds::BufferSubData*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  uint32_t data_size = size;
  const void* data = GetSharedMemoryAs<const void*>(
      c.data_shm_id, c.data_shm_offset, data_size);
  if (!validators_->buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBufferSubData", target, "target");
    return error::kNoError;
  }
  if (size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glBufferSubData", "size < 0");
    return error::kNoError;
  }
  if (data == nullptr) {
    return error::kOutOfBounds;
  }
  DoBufferSubData(target, offset, size, data);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCheckFramebufferStatus(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CheckFramebufferStatus& c =
      *static_cast<const volatile gles2::cmds::CheckFramebufferStatus*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  typedef cmds::CheckFramebufferStatus::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCheckFramebufferStatus", target,
                                    "target");
    return error::kNoError;
  }
  *result_dst = DoCheckFramebufferStatus(target);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClear(uint32_t immediate_data_size,
                                           const volatile void* cmd_data) {
  const volatile gles2::cmds::Clear& c =
      *static_cast<const volatile gles2::cmds::Clear*>(cmd_data);
  error::Error error;
  error = WillAccessBoundFramebufferForDraw();
  if (error != error::kNoError)
    return error;
  GLbitfield mask = static_cast<GLbitfield>(c.mask);
  DoClear(mask);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearBufferfi(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::ClearBufferfi& c =
      *static_cast<const volatile gles2::cmds::ClearBufferfi*>(cmd_data);
  GLenum buffer = static_cast<GLenum>(c.buffer);
  GLint drawbuffers = static_cast<GLint>(c.drawbuffers);
  GLfloat depth = static_cast<GLfloat>(c.depth);
  GLint stencil = static_cast<GLint>(c.stencil);
  if (!validators_->bufferfi.IsValid(buffer)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glClearBufferfi", buffer, "buffer");
    return error::kNoError;
  }
  DoClearBufferfi(buffer, drawbuffers, depth, stencil);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearBufferfvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::ClearBufferfvImmediate& c =
      *static_cast<const volatile gles2::cmds::ClearBufferfvImmediate*>(
          cmd_data);
  GLenum buffer = static_cast<GLenum>(c.buffer);
  GLint drawbuffers = static_cast<GLint>(c.drawbuffers);
  uint32_t value_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 4>(1, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (!validators_->bufferfv.IsValid(buffer)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glClearBufferfv", buffer, "buffer");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoClearBufferfv(buffer, drawbuffers, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearBufferivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::ClearBufferivImmediate& c =
      *static_cast<const volatile gles2::cmds::ClearBufferivImmediate*>(
          cmd_data);
  GLenum buffer = static_cast<GLenum>(c.buffer);
  GLint drawbuffers = static_cast<GLint>(c.drawbuffers);
  uint32_t value_size;
  if (!GLES2Util::ComputeDataSize<GLint, 4>(1, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* value = GetImmediateDataAs<volatile const GLint*>(
      c, value_size, immediate_data_size);
  if (!validators_->bufferiv.IsValid(buffer)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glClearBufferiv", buffer, "buffer");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoClearBufferiv(buffer, drawbuffers, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearBufferuivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::ClearBufferuivImmediate& c =
      *static_cast<const volatile gles2::cmds::ClearBufferuivImmediate*>(
          cmd_data);
  GLenum buffer = static_cast<GLenum>(c.buffer);
  GLint drawbuffers = static_cast<GLint>(c.drawbuffers);
  uint32_t value_size;
  if (!GLES2Util::ComputeDataSize<GLuint, 4>(1, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* value = GetImmediateDataAs<volatile const GLuint*>(
      c, value_size, immediate_data_size);
  if (!validators_->bufferuiv.IsValid(buffer)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glClearBufferuiv", buffer, "buffer");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoClearBufferuiv(buffer, drawbuffers, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearColor(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearColor& c =
      *static_cast<const volatile gles2::cmds::ClearColor*>(cmd_data);
  GLclampf red = static_cast<GLclampf>(c.red);
  GLclampf green = static_cast<GLclampf>(c.green);
  GLclampf blue = static_cast<GLclampf>(c.blue);
  GLclampf alpha = static_cast<GLclampf>(c.alpha);
  if (state_.color_clear_red != red || state_.color_clear_green != green ||
      state_.color_clear_blue != blue || state_.color_clear_alpha != alpha) {
    state_.color_clear_red = red;
    state_.color_clear_green = green;
    state_.color_clear_blue = blue;
    state_.color_clear_alpha = alpha;
    api()->glClearColorFn(red, green, blue, alpha);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearDepthf(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearDepthf& c =
      *static_cast<const volatile gles2::cmds::ClearDepthf*>(cmd_data);
  GLclampf depth = static_cast<GLclampf>(c.depth);
  if (state_.depth_clear != depth) {
    state_.depth_clear = depth;
    glClearDepth(depth);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleClearStencil(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ClearStencil& c =
      *static_cast<const volatile gles2::cmds::ClearStencil*>(cmd_data);
  GLint s = static_cast<GLint>(c.s);
  if (state_.stencil_clear != s) {
    state_.stencil_clear = s;
    api()->glClearStencilFn(s);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleColorMask(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::ColorMask& c =
      *static_cast<const volatile gles2::cmds::ColorMask*>(cmd_data);
  GLboolean red = static_cast<GLboolean>(c.red);
  GLboolean green = static_cast<GLboolean>(c.green);
  GLboolean blue = static_cast<GLboolean>(c.blue);
  GLboolean alpha = static_cast<GLboolean>(c.alpha);
  if (state_.color_mask_red != red || state_.color_mask_green != green ||
      state_.color_mask_blue != blue || state_.color_mask_alpha != alpha) {
    state_.color_mask_red = red;
    state_.color_mask_green = green;
    state_.color_mask_blue = blue;
    state_.color_mask_alpha = alpha;
    framebuffer_state_.clear_state_dirty = true;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCompileShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CompileShader& c =
      *static_cast<const volatile gles2::cmds::CompileShader*>(cmd_data);
  GLuint shader = c.shader;
  DoCompileShader(shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyBufferSubData(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CopyBufferSubData& c =
      *static_cast<const volatile gles2::cmds::CopyBufferSubData*>(cmd_data);
  GLenum readtarget = static_cast<GLenum>(c.readtarget);
  GLenum writetarget = static_cast<GLenum>(c.writetarget);
  GLintptr readoffset = static_cast<GLintptr>(c.readoffset);
  GLintptr writeoffset = static_cast<GLintptr>(c.writeoffset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  if (!validators_->buffer_target.IsValid(readtarget)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCopyBufferSubData", readtarget,
                                    "readtarget");
    return error::kNoError;
  }
  if (!validators_->buffer_target.IsValid(writetarget)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCopyBufferSubData", writetarget,
                                    "writetarget");
    return error::kNoError;
  }
  if (size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopyBufferSubData", "size < 0");
    return error::kNoError;
  }
  DoCopyBufferSubData(readtarget, writetarget, readoffset, writeoffset, size);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyTexImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopyTexImage2D& c =
      *static_cast<const volatile gles2::cmds::CopyTexImage2D*>(cmd_data);
  error::Error error;
  error = WillAccessBoundFramebufferForRead();
  if (error != error::kNoError)
    return error;
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLint border = static_cast<GLint>(c.border);
  if (!validators_->texture_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCopyTexImage2D", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format.IsValid(internalformat)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCopyTexImage2D", internalformat,
                                    "internalformat");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopyTexImage2D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopyTexImage2D", "height < 0");
    return error::kNoError;
  }
  DoCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyTexSubImage2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopyTexSubImage2D& c =
      *static_cast<const volatile gles2::cmds::CopyTexSubImage2D*>(cmd_data);
  error::Error error;
  error = WillAccessBoundFramebufferForRead();
  if (error != error::kNoError)
    return error;
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->texture_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCopyTexSubImage2D", target, "target");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopyTexSubImage2D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopyTexSubImage2D", "height < 0");
    return error::kNoError;
  }
  DoCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyTexSubImage3D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::CopyTexSubImage3D& c =
      *static_cast<const volatile gles2::cmds::CopyTexSubImage3D*>(cmd_data);
  error::Error error;
  error = WillAccessBoundFramebufferForRead();
  if (error != error::kNoError)
    return error;
  GLenum target = static_cast<GLenum>(c.target);
  GLint level = static_cast<GLint>(c.level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint zoffset = static_cast<GLint>(c.zoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->texture_3_d_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCopyTexSubImage3D", target, "target");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopyTexSubImage3D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopyTexSubImage3D", "height < 0");
    return error::kNoError;
  }
  DoCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width,
                      height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCreateProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CreateProgram& c =
      *static_cast<const volatile gles2::cmds::CreateProgram*>(cmd_data);
  uint32_t client_id = c.client_id;
  if (GetProgram(client_id)) {
    return error::kInvalidArguments;
  }
  GLuint service_id = api()->glCreateProgramFn();
  if (service_id) {
    CreateProgram(client_id, service_id);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCreateShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CreateShader& c =
      *static_cast<const volatile gles2::cmds::CreateShader*>(cmd_data);
  GLenum type = static_cast<GLenum>(c.type);
  if (!validators_->shader_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCreateShader", type, "type");
    return error::kNoError;
  }
  uint32_t client_id = c.client_id;
  if (GetShader(client_id)) {
    return error::kInvalidArguments;
  }
  GLuint service_id = api()->glCreateShaderFn(type);
  if (service_id) {
    CreateShader(client_id, service_id, type);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCullFace(uint32_t immediate_data_size,
                                              const volatile void* cmd_data) {
  const volatile gles2::cmds::CullFace& c =
      *static_cast<const volatile gles2::cmds::CullFace*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->face_type.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCullFace", mode, "mode");
    return error::kNoError;
  }
  if (state_.cull_mode != mode) {
    state_.cull_mode = mode;
    api()->glCullFaceFn(mode);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteBuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteBuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteBuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t buffers_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&buffers_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* buffers = GetImmediateDataAs<volatile const GLuint*>(
      c, buffers_size, immediate_data_size);
  if (buffers == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteBuffersHelper(n, buffers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteFramebuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteFramebuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteFramebuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t framebuffers_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&framebuffers_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* framebuffers =
      GetImmediateDataAs<volatile const GLuint*>(c, framebuffers_size,
                                                 immediate_data_size);
  if (framebuffers == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteFramebuffersHelper(n, framebuffers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteRenderbuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteRenderbuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteRenderbuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t renderbuffers_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&renderbuffers_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* renderbuffers =
      GetImmediateDataAs<volatile const GLuint*>(c, renderbuffers_size,
                                                 immediate_data_size);
  if (renderbuffers == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteRenderbuffersHelper(n, renderbuffers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteSamplersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::DeleteSamplersImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteSamplersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t samplers_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&samplers_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* samplers = GetImmediateDataAs<volatile const GLuint*>(
      c, samplers_size, immediate_data_size);
  if (samplers == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteSamplersHelper(n, samplers);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteSync(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::DeleteSync& c =
      *static_cast<const volatile gles2::cmds::DeleteSync*>(cmd_data);
  GLuint sync = c.sync;
  DeleteSyncHelper(sync);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteTexturesImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteTexturesImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteTexturesImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t textures_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&textures_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* textures = GetImmediateDataAs<volatile const GLuint*>(
      c, textures_size, immediate_data_size);
  if (textures == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteTexturesHelper(n, textures);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteTransformFeedbacksImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::DeleteTransformFeedbacksImmediate& c =
      *static_cast<
          const volatile gles2::cmds::DeleteTransformFeedbacksImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t ids_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&ids_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* ids = GetImmediateDataAs<volatile const GLuint*>(
      c, ids_size, immediate_data_size);
  if (ids == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteTransformFeedbacksHelper(n, ids);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDepthFunc(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::DepthFunc& c =
      *static_cast<const volatile gles2::cmds::DepthFunc*>(cmd_data);
  GLenum func = static_cast<GLenum>(c.func);
  if (!validators_->cmp_function.IsValid(func)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glDepthFunc", func, "func");
    return error::kNoError;
  }
  if (state_.depth_func != func) {
    state_.depth_func = func;
    api()->glDepthFuncFn(func);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDepthMask(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::DepthMask& c =
      *static_cast<const volatile gles2::cmds::DepthMask*>(cmd_data);
  GLboolean flag = static_cast<GLboolean>(c.flag);
  if (state_.depth_mask != flag) {
    state_.depth_mask = flag;
    framebuffer_state_.clear_state_dirty = true;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDepthRangef(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DepthRangef& c =
      *static_cast<const volatile gles2::cmds::DepthRangef*>(cmd_data);
  GLclampf zNear = static_cast<GLclampf>(c.zNear);
  GLclampf zFar = static_cast<GLclampf>(c.zFar);
  DoDepthRangef(zNear, zFar);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDetachShader(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DetachShader& c =
      *static_cast<const volatile gles2::cmds::DetachShader*>(cmd_data);
  GLuint program = c.program;
  GLuint shader = c.shader;
  DoDetachShader(program, shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDisable(uint32_t immediate_data_size,
                                             const volatile void* cmd_data) {
  const volatile gles2::cmds::Disable& c =
      *static_cast<const volatile gles2::cmds::Disable*>(cmd_data);
  GLenum cap = static_cast<GLenum>(c.cap);
  if (!validators_->capability.IsValid(cap)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glDisable", cap, "cap");
    return error::kNoError;
  }
  DoDisable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDisableVertexAttribArray(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DisableVertexAttribArray& c =
      *static_cast<const volatile gles2::cmds::DisableVertexAttribArray*>(
          cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  DoDisableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEnable(uint32_t immediate_data_size,
                                            const volatile void* cmd_data) {
  const volatile gles2::cmds::Enable& c =
      *static_cast<const volatile gles2::cmds::Enable*>(cmd_data);
  GLenum cap = static_cast<GLenum>(c.cap);
  if (!validators_->capability.IsValid(cap)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glEnable", cap, "cap");
    return error::kNoError;
  }
  DoEnable(cap);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEnableVertexAttribArray(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::EnableVertexAttribArray& c =
      *static_cast<const volatile gles2::cmds::EnableVertexAttribArray*>(
          cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  DoEnableVertexAttribArray(index);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFenceSync(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::FenceSync& c =
      *static_cast<const volatile gles2::cmds::FenceSync*>(cmd_data);
  GLenum condition = static_cast<GLenum>(c.condition);
  GLbitfield flags = static_cast<GLbitfield>(c.flags);
  uint32_t client_id = c.client_id;
  GLsync service_id = 0;
  if (group_->GetSyncServiceId(client_id, &service_id)) {
    return error::kInvalidArguments;
  }
  service_id = DoFenceSync(condition, flags);
  if (service_id) {
    group_->AddSyncId(client_id, service_id);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFinish(uint32_t immediate_data_size,
                                            const volatile void* cmd_data) {
  error::Error error;
  error = WillAccessBoundFramebufferForRead();
  if (error != error::kNoError)
    return error;
  DoFinish();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFlush(uint32_t immediate_data_size,
                                           const volatile void* cmd_data) {
  DoFlush();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferRenderbuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FramebufferRenderbuffer& c =
      *static_cast<const volatile gles2::cmds::FramebufferRenderbuffer*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum renderbuffertarget = static_cast<GLenum>(c.renderbuffertarget);
  GLuint renderbuffer = c.renderbuffer;
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferRenderbuffer", target,
                                    "target");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferRenderbuffer", attachment,
                                    "attachment");
    return error::kNoError;
  }
  if (!validators_->render_buffer_target.IsValid(renderbuffertarget)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferRenderbuffer",
                                    renderbuffertarget, "renderbuffertarget");
    return error::kNoError;
  }
  DoFramebufferRenderbuffer(target, attachment, renderbuffertarget,
                            renderbuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferTexture2D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FramebufferTexture2D& c =
      *static_cast<const volatile gles2::cmds::FramebufferTexture2D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum textarget = static_cast<GLenum>(c.textarget);
  GLuint texture = c.texture;
  GLint level = static_cast<GLint>(c.level);
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferTexture2D", target, "target");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferTexture2D", attachment,
                                    "attachment");
    return error::kNoError;
  }
  if (!validators_->texture_target.IsValid(textarget)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferTexture2D", textarget,
                                    "textarget");
    return error::kNoError;
  }
  DoFramebufferTexture2D(target, attachment, textarget, texture, level);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferTextureLayer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::FramebufferTextureLayer& c =
      *static_cast<const volatile gles2::cmds::FramebufferTextureLayer*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLuint texture = c.texture;
  GLint level = static_cast<GLint>(c.level);
  GLint layer = static_cast<GLint>(c.layer);
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferTextureLayer", target,
                                    "target");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferTextureLayer", attachment,
                                    "attachment");
    return error::kNoError;
  }
  DoFramebufferTextureLayer(target, attachment, texture, level, layer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFrontFace(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::FrontFace& c =
      *static_cast<const volatile gles2::cmds::FrontFace*>(cmd_data);
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->face_mode.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFrontFace", mode, "mode");
    return error::kNoError;
  }
  if (state_.front_face != mode) {
    state_.front_face = mode;
    api()->glFrontFaceFn(mode);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenBuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenBuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::GenBuffersImmediate*>(cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t buffers_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&buffers_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* buffers = GetImmediateDataAs<volatile GLuint*>(
      c, buffers_size, immediate_data_size);
  if (buffers == nullptr) {
    return error::kOutOfBounds;
  }
  auto buffers_copy = std::make_unique<GLuint[]>(n);
  GLuint* buffers_safe = buffers_copy.get();
  std::copy(buffers, buffers + n, buffers_safe);
  if (!CheckUniqueAndNonNullIds(n, buffers_safe) ||
      !GenBuffersHelper(n, buffers_safe)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenerateMipmap(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenerateMipmap& c =
      *static_cast<const volatile gles2::cmds::GenerateMipmap*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGenerateMipmap", target, "target");
    return error::kNoError;
  }
  DoGenerateMipmap(target);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenFramebuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenFramebuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::GenFramebuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t framebuffers_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&framebuffers_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* framebuffers = GetImmediateDataAs<volatile GLuint*>(
      c, framebuffers_size, immediate_data_size);
  if (framebuffers == nullptr) {
    return error::kOutOfBounds;
  }
  auto framebuffers_copy = std::make_unique<GLuint[]>(n);
  GLuint* framebuffers_safe = framebuffers_copy.get();
  std::copy(framebuffers, framebuffers + n, framebuffers_safe);
  if (!CheckUniqueAndNonNullIds(n, framebuffers_safe) ||
      !GenFramebuffersHelper(n, framebuffers_safe)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenRenderbuffersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenRenderbuffersImmediate& c =
      *static_cast<const volatile gles2::cmds::GenRenderbuffersImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t renderbuffers_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&renderbuffers_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* renderbuffers = GetImmediateDataAs<volatile GLuint*>(
      c, renderbuffers_size, immediate_data_size);
  if (renderbuffers == nullptr) {
    return error::kOutOfBounds;
  }
  auto renderbuffers_copy = std::make_unique<GLuint[]>(n);
  GLuint* renderbuffers_safe = renderbuffers_copy.get();
  std::copy(renderbuffers, renderbuffers + n, renderbuffers_safe);
  if (!CheckUniqueAndNonNullIds(n, renderbuffers_safe) ||
      !GenRenderbuffersHelper(n, renderbuffers_safe)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenSamplersImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GenSamplersImmediate& c =
      *static_cast<const volatile gles2::cmds::GenSamplersImmediate*>(cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t samplers_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&samplers_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* samplers = GetImmediateDataAs<volatile GLuint*>(
      c, samplers_size, immediate_data_size);
  if (samplers == nullptr) {
    return error::kOutOfBounds;
  }
  auto samplers_copy = std::make_unique<GLuint[]>(n);
  GLuint* samplers_safe = samplers_copy.get();
  std::copy(samplers, samplers + n, samplers_safe);
  if (!CheckUniqueAndNonNullIds(n, samplers_safe) ||
      !GenSamplersHelper(n, samplers_safe)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenTexturesImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenTexturesImmediate& c =
      *static_cast<const volatile gles2::cmds::GenTexturesImmediate*>(cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t textures_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&textures_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* textures = GetImmediateDataAs<volatile GLuint*>(
      c, textures_size, immediate_data_size);
  if (textures == nullptr) {
    return error::kOutOfBounds;
  }
  auto textures_copy = std::make_unique<GLuint[]>(n);
  GLuint* textures_safe = textures_copy.get();
  std::copy(textures, textures + n, textures_safe);
  if (!CheckUniqueAndNonNullIds(n, textures_safe) ||
      !GenTexturesHelper(n, textures_safe)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenTransformFeedbacksImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GenTransformFeedbacksImmediate& c =
      *static_cast<const volatile gles2::cmds::GenTransformFeedbacksImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t ids_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&ids_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* ids =
      GetImmediateDataAs<volatile GLuint*>(c, ids_size, immediate_data_size);
  if (ids == nullptr) {
    return error::kOutOfBounds;
  }
  auto ids_copy = std::make_unique<GLuint[]>(n);
  GLuint* ids_safe = ids_copy.get();
  std::copy(ids, ids + n, ids_safe);
  if (!CheckUniqueAndNonNullIds(n, ids_safe) ||
      !GenTransformFeedbacksHelper(n, ids_safe)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetBooleanv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetBooleanv& c =
      *static_cast<const volatile gles2::cmds::GetBooleanv*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetBooleanv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetBooleanv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLboolean* params = result ? result->GetData() : nullptr;
  if (!validators_->g_l_state.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetBooleanv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetBooleanv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetBooleanv(pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetBooleanv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetBufferParameteri64v(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetBufferParameteri64v& c =
      *static_cast<const volatile gles2::cmds::GetBufferParameteri64v*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetBufferParameteri64v::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetBufferParameteri64v", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint64* params = result ? result->GetData() : nullptr;
  if (!validators_->buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetBufferParameteri64v", target,
                                    "target");
    return error::kNoError;
  }
  if (!validators_->buffer_parameter_64.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetBufferParameteri64v", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetBufferParameteri64v(target, pname, params, num_values);
  result->SetNumResults(num_values);
  return error::kNoError;
}
error::Error GLES2DecoderImpl::HandleGetBufferParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetBufferParameteriv& c =
      *static_cast<const volatile gles2::cmds::GetBufferParameteriv*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetBufferParameteriv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetBufferParameteriv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetBufferParameteriv", target, "target");
    return error::kNoError;
  }
  if (!validators_->buffer_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetBufferParameteriv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetBufferParameteriv(target, pname, params, num_values);
  result->SetNumResults(num_values);
  return error::kNoError;
}
error::Error GLES2DecoderImpl::HandleGetError(uint32_t immediate_data_size,
                                              const volatile void* cmd_data) {
  const volatile gles2::cmds::GetError& c =
      *static_cast<const volatile gles2::cmds::GetError*>(cmd_data);
  typedef cmds::GetError::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = GetErrorState()->GetGLError();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFloatv(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::GetFloatv& c =
      *static_cast<const volatile gles2::cmds::GetFloatv*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetFloatv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetFloatv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLfloat* params = result ? result->GetData() : nullptr;
  if (!validators_->g_l_state.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetFloatv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetFloatv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetFloatv(pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetFloatv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetFramebufferAttachmentParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetFramebufferAttachmentParameteriv& c =
      *static_cast<
          const volatile gles2::cmds::GetFramebufferAttachmentParameteriv*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetFramebufferAttachmentParameteriv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetFramebufferAttachmentParameteriv",
                                    pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetFramebufferAttachmentParameteriv",
                                    target, "target");
    return error::kNoError;
  }
  if (!validators_->attachment_query.IsValid(attachment)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetFramebufferAttachmentParameteriv",
                                    attachment, "attachment");
    return error::kNoError;
  }
  if (!validators_->framebuffer_attachment_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetFramebufferAttachmentParameteriv",
                                    pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetFramebufferAttachmentParameteriv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetFramebufferAttachmentParameteriv(target, attachment, pname, params,
                                        num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetFramebufferAttachmentParameteriv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetInteger64v(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetInteger64v& c =
      *static_cast<const volatile gles2::cmds::GetInteger64v*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetInteger64v::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetInteger64v", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint64* params = result ? result->GetData() : nullptr;
  if (!validators_->g_l_state.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetInteger64v", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetInteger64v");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetInteger64v(pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetInteger64v");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetIntegeri_v(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetIntegeri_v& c =
      *static_cast<const volatile gles2::cmds::GetIntegeri_v*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLuint index = static_cast<GLuint>(c.index);
  typedef cmds::GetIntegeri_v::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetIntegeri_v", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(c.data_shm_id, c.data_shm_offset,
                                              checked_size);
  GLint* data = result ? result->GetData() : nullptr;
  if (!validators_->indexed_g_l_state.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetIntegeri_v", pname, "pname");
    return error::kNoError;
  }
  if (data == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetIntegeri_v(pname, index, data, num_values);
  result->SetNumResults(num_values);
  return error::kNoError;
}
error::Error GLES2DecoderImpl::HandleGetInteger64i_v(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetInteger64i_v& c =
      *static_cast<const volatile gles2::cmds::GetInteger64i_v*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLuint index = static_cast<GLuint>(c.index);
  typedef cmds::GetInteger64i_v::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetInteger64i_v", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(c.data_shm_id, c.data_shm_offset,
                                              checked_size);
  GLint64* data = result ? result->GetData() : nullptr;
  if (!validators_->indexed_g_l_state.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetInteger64i_v", pname, "pname");
    return error::kNoError;
  }
  if (data == nullptr) {
    return error::kOutOfBounds;
  }
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetInteger64i_v(pname, index, data, num_values);
  result->SetNumResults(num_values);
  return error::kNoError;
}
error::Error GLES2DecoderImpl::HandleGetIntegerv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetIntegerv& c =
      *static_cast<const volatile gles2::cmds::GetIntegerv*>(cmd_data);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetIntegerv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetIntegerv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->g_l_state.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetIntegerv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetIntegerv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetIntegerv(pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetIntegerv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetProgramiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetProgramiv& c =
      *static_cast<const volatile gles2::cmds::GetProgramiv*>(cmd_data);
  GLuint program = c.program;
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetProgramiv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetProgramiv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->program_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetProgramiv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetProgramiv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetProgramiv(program, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetProgramiv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetRenderbufferParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetRenderbufferParameteriv& c =
      *static_cast<const volatile gles2::cmds::GetRenderbufferParameteriv*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetRenderbufferParameteriv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetRenderbufferParameteriv", pname,
                                    "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->render_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetRenderbufferParameteriv", target,
                                    "target");
    return error::kNoError;
  }
  if (!validators_->render_buffer_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetRenderbufferParameteriv", pname,
                                    "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetRenderbufferParameteriv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetRenderbufferParameteriv(target, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetRenderbufferParameteriv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetSamplerParameterfv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetSamplerParameterfv& c =
      *static_cast<const volatile gles2::cmds::GetSamplerParameterfv*>(
          cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetSamplerParameterfv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetSamplerParameterfv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLfloat* params = result ? result->GetData() : nullptr;
  if (!validators_->sampler_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetSamplerParameterfv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetSamplerParameterfv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetSamplerParameterfv(sampler, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetSamplerParameterfv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetSamplerParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetSamplerParameteriv& c =
      *static_cast<const volatile gles2::cmds::GetSamplerParameteriv*>(
          cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetSamplerParameteriv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetSamplerParameteriv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->sampler_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetSamplerParameteriv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetSamplerParameteriv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetSamplerParameteriv(sampler, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetSamplerParameteriv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetShaderiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetShaderiv& c =
      *static_cast<const volatile gles2::cmds::GetShaderiv*>(cmd_data);
  GLuint shader = c.shader;
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetShaderiv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetShaderiv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->shader_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetShaderiv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetShaderiv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetShaderiv(shader, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetShaderiv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetSynciv(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetSynciv& c =
      *static_cast<const volatile gles2::cmds::GetSynciv*>(cmd_data);
  GLuint sync = static_cast<GLuint>(c.sync);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetSynciv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetSynciv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.values_shm_id, c.values_shm_offset, checked_size);
  GLint* values = result ? result->GetData() : nullptr;
  if (!validators_->sync_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetSynciv", pname, "pname");
    return error::kNoError;
  }
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetSynciv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetSynciv(sync, pname, num_values, nullptr, values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetSynciv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTexParameterfv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetTexParameterfv& c =
      *static_cast<const volatile gles2::cmds::GetTexParameterfv*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetTexParameterfv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetTexParameterfv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLfloat* params = result ? result->GetData() : nullptr;
  if (!validators_->get_tex_param_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetTexParameterfv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetTexParameterfv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetTexParameterfv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetTexParameterfv(target, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetTexParameterfv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetTexParameteriv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetTexParameteriv& c =
      *static_cast<const volatile gles2::cmds::GetTexParameteriv*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetTexParameteriv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetTexParameteriv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->get_tex_param_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetTexParameteriv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetTexParameteriv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetTexParameteriv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetTexParameteriv(target, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetTexParameteriv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribfv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetVertexAttribfv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribfv*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetVertexAttribfv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetVertexAttribfv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLfloat* params = result ? result->GetData() : nullptr;
  if (!validators_->vertex_attribute.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetVertexAttribfv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetVertexAttribfv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetVertexAttribfv(index, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetVertexAttribfv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetVertexAttribiv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribiv*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetVertexAttribiv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetVertexAttribiv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->vertex_attribute.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetVertexAttribiv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetVertexAttribiv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetVertexAttribiv(index, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetVertexAttribiv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribIiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetVertexAttribIiv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribIiv*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetVertexAttribIiv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetVertexAttribIiv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLint* params = result ? result->GetData() : nullptr;
  if (!validators_->vertex_attribute.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetVertexAttribIiv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetVertexAttribIiv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetVertexAttribIiv(index, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetVertexAttribIiv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetVertexAttribIuiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::GetVertexAttribIuiv& c =
      *static_cast<const volatile gles2::cmds::GetVertexAttribIuiv*>(cmd_data);
  GLuint index = static_cast<GLuint>(c.index);
  GLenum pname = static_cast<GLenum>(c.pname);
  typedef cmds::GetVertexAttribIuiv::Result Result;
  GLsizei num_values = 0;
  if (!GetNumValuesReturnedForGLGet(pname, &num_values)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(":GetVertexAttribIuiv", pname, "pname");
    return error::kNoError;
  }
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(num_values).AssignIfValid(&checked_size)) {
    return error::kOutOfBounds;
  }
  Result* result = GetSharedMemoryAs<Result*>(
      c.params_shm_id, c.params_shm_offset, checked_size);
  GLuint* params = result ? result->GetData() : nullptr;
  if (!validators_->vertex_attribute.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetVertexAttribIuiv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  LOCAL_COPY_REAL_GL_ERRORS_TO_WRAPPER("GetVertexAttribIuiv");
  // Check that the client initialized the result.
  if (result->size != 0) {
    return error::kInvalidArguments;
  }
  DoGetVertexAttribIuiv(index, pname, params, num_values);
  GLenum error = LOCAL_PEEK_GL_ERROR("GetVertexAttribIuiv");
  if (error == GL_NO_ERROR) {
    result->SetNumResults(num_values);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleHint(uint32_t immediate_data_size,
                                          const volatile void* cmd_data) {
  const volatile gles2::cmds::Hint& c =
      *static_cast<const volatile gles2::cmds::Hint*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->hint_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glHint", target, "target");
    return error::kNoError;
  }
  if (!validators_->hint_mode.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glHint", mode, "mode");
    return error::kNoError;
  }
  switch (target) {
    case GL_GENERATE_MIPMAP_HINT:
      if (state_.hint_generate_mipmap != mode) {
        state_.hint_generate_mipmap = mode;
        if (!feature_info_->gl_version_info().is_desktop_core_profile) {
          api()->glHintFn(target, mode);
        }
      }
      break;
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
      if (state_.hint_fragment_shader_derivative != mode) {
        state_.hint_fragment_shader_derivative = mode;
        if (feature_info_->feature_flags().oes_standard_derivatives) {
          api()->glHintFn(target, mode);
        }
      }
      break;
    case GL_TEXTURE_FILTERING_HINT_CHROMIUM:
      if (state_.hint_texture_filtering != mode) {
        state_.hint_texture_filtering = mode;
        if (feature_info_->feature_flags().chromium_texture_filtering_hint) {
          api()->glHintFn(target, mode);
        }
      }
      break;
    default:
      NOTREACHED();
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleInvalidateFramebufferImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::InvalidateFramebufferImmediate& c =
      *static_cast<const volatile gles2::cmds::InvalidateFramebufferImmediate*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t attachments_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLenum, 1>(count, &attachments_size)) {
    return error::kOutOfBounds;
  }
  if (attachments_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLenum* attachments =
      GetImmediateDataAs<volatile const GLenum*>(c, attachments_size,
                                                 immediate_data_size);
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glInvalidateFramebuffer", target,
                                    "target");
    return error::kNoError;
  }
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glInvalidateFramebuffer",
                       "count < 0");
    return error::kNoError;
  }
  if (attachments == nullptr) {
    return error::kOutOfBounds;
  }
  DoInvalidateFramebuffer(target, count, attachments);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleInvalidateSubFramebufferImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::InvalidateSubFramebufferImmediate& c =
      *static_cast<
          const volatile gles2::cmds::InvalidateSubFramebufferImmediate*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t attachments_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLenum, 1>(count, &attachments_size)) {
    return error::kOutOfBounds;
  }
  if (attachments_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLenum* attachments =
      GetImmediateDataAs<volatile const GLenum*>(c, attachments_size,
                                                 immediate_data_size);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glInvalidateSubFramebuffer", target,
                                    "target");
    return error::kNoError;
  }
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glInvalidateSubFramebuffer",
                       "count < 0");
    return error::kNoError;
  }
  if (attachments == nullptr) {
    return error::kOutOfBounds;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glInvalidateSubFramebuffer",
                       "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glInvalidateSubFramebuffer",
                       "height < 0");
    return error::kNoError;
  }
  DoInvalidateSubFramebuffer(target, count, attachments, x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsBuffer(uint32_t immediate_data_size,
                                              const volatile void* cmd_data) {
  const volatile gles2::cmds::IsBuffer& c =
      *static_cast<const volatile gles2::cmds::IsBuffer*>(cmd_data);
  GLuint buffer = c.buffer;
  typedef cmds::IsBuffer::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsBuffer(buffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsEnabled(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::IsEnabled& c =
      *static_cast<const volatile gles2::cmds::IsEnabled*>(cmd_data);
  GLenum cap = static_cast<GLenum>(c.cap);
  typedef cmds::IsEnabled::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (!validators_->capability.IsValid(cap)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glIsEnabled", cap, "cap");
    return error::kNoError;
  }
  *result_dst = DoIsEnabled(cap);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsFramebuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsFramebuffer& c =
      *static_cast<const volatile gles2::cmds::IsFramebuffer*>(cmd_data);
  GLuint framebuffer = c.framebuffer;
  typedef cmds::IsFramebuffer::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsFramebuffer(framebuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsProgram(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::IsProgram& c =
      *static_cast<const volatile gles2::cmds::IsProgram*>(cmd_data);
  GLuint program = c.program;
  typedef cmds::IsProgram::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsRenderbuffer(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsRenderbuffer& c =
      *static_cast<const volatile gles2::cmds::IsRenderbuffer*>(cmd_data);
  GLuint renderbuffer = c.renderbuffer;
  typedef cmds::IsRenderbuffer::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsRenderbuffer(renderbuffer);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsSampler(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::IsSampler& c =
      *static_cast<const volatile gles2::cmds::IsSampler*>(cmd_data);
  GLuint sampler = c.sampler;
  typedef cmds::IsSampler::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsSampler(sampler);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsShader(uint32_t immediate_data_size,
                                              const volatile void* cmd_data) {
  const volatile gles2::cmds::IsShader& c =
      *static_cast<const volatile gles2::cmds::IsShader*>(cmd_data);
  GLuint shader = c.shader;
  typedef cmds::IsShader::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsShader(shader);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsSync(uint32_t immediate_data_size,
                                            const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::IsSync& c =
      *static_cast<const volatile gles2::cmds::IsSync*>(cmd_data);
  GLuint sync = c.sync;
  typedef cmds::IsSync::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsSync(sync);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsTexture(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::IsTexture& c =
      *static_cast<const volatile gles2::cmds::IsTexture*>(cmd_data);
  GLuint texture = c.texture;
  typedef cmds::IsTexture::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsTexture(texture);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::IsTransformFeedback& c =
      *static_cast<const volatile gles2::cmds::IsTransformFeedback*>(cmd_data);
  GLuint transformfeedback = c.transformfeedback;
  typedef cmds::IsTransformFeedback::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsTransformFeedback(transformfeedback);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleLineWidth(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::LineWidth& c =
      *static_cast<const volatile gles2::cmds::LineWidth*>(cmd_data);
  GLfloat width = static_cast<GLfloat>(c.width);
  if (width <= 0.0f || std::isnan(width)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "LineWidth", "width out of range");
    return error::kNoError;
  }
  if (state_.line_width != width) {
    state_.line_width = width;
    DoLineWidth(width);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleLinkProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::LinkProgram& c =
      *static_cast<const volatile gles2::cmds::LinkProgram*>(cmd_data);
  GLuint program = c.program;
  DoLinkProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePauseTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  DoPauseTransformFeedback();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePolygonOffset(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PolygonOffset& c =
      *static_cast<const volatile gles2::cmds::PolygonOffset*>(cmd_data);
  GLfloat factor = static_cast<GLfloat>(c.factor);
  GLfloat units = static_cast<GLfloat>(c.units);
  if (state_.polygon_offset_factor != factor ||
      state_.polygon_offset_units != units) {
    state_.polygon_offset_factor = factor;
    state_.polygon_offset_units = units;
    api()->glPolygonOffsetFn(factor, units);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleReadBuffer(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::ReadBuffer& c =
      *static_cast<const volatile gles2::cmds::ReadBuffer*>(cmd_data);
  GLenum src = static_cast<GLenum>(c.src);
  if (!validators_->read_buffer.IsValid(src)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glReadBuffer", src, "src");
    return error::kNoError;
  }
  DoReadBuffer(src);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleReleaseShaderCompiler(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  DoReleaseShaderCompiler();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRenderbufferStorage(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::RenderbufferStorage& c =
      *static_cast<const volatile gles2::cmds::RenderbufferStorage*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->render_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glRenderbufferStorage", target, "target");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(internalformat)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glRenderbufferStorage", internalformat,
                                    "internalformat");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRenderbufferStorage", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRenderbufferStorage", "height < 0");
    return error::kNoError;
  }
  DoRenderbufferStorage(target, internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleResumeTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  DoResumeTransformFeedback();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSampleCoverage(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SampleCoverage& c =
      *static_cast<const volatile gles2::cmds::SampleCoverage*>(cmd_data);
  GLclampf value = static_cast<GLclampf>(c.value);
  GLboolean invert = static_cast<GLboolean>(c.invert);
  DoSampleCoverage(value, invert);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSamplerParameterf(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::SamplerParameterf& c =
      *static_cast<const volatile gles2::cmds::SamplerParameterf*>(cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat param = static_cast<GLfloat>(c.param);
  if (!validators_->sampler_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glSamplerParameterf", pname, "pname");
    return error::kNoError;
  }
  DoSamplerParameterf(sampler, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSamplerParameterfvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::SamplerParameterfvImmediate& c =
      *static_cast<const volatile gles2::cmds::SamplerParameterfvImmediate*>(
          cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t params_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 1>(1, &params_size)) {
    return error::kOutOfBounds;
  }
  if (params_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* params = GetImmediateDataAs<volatile const GLfloat*>(
      c, params_size, immediate_data_size);
  if (!validators_->sampler_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glSamplerParameterfv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  DoSamplerParameterfv(sampler, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSamplerParameteri(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::SamplerParameteri& c =
      *static_cast<const volatile gles2::cmds::SamplerParameteri*>(cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  if (!validators_->sampler_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glSamplerParameteri", pname, "pname");
    return error::kNoError;
  }
  DoSamplerParameteri(sampler, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSamplerParameterivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::SamplerParameterivImmediate& c =
      *static_cast<const volatile gles2::cmds::SamplerParameterivImmediate*>(
          cmd_data);
  GLuint sampler = c.sampler;
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t params_size;
  if (!GLES2Util::ComputeDataSize<GLint, 1>(1, &params_size)) {
    return error::kOutOfBounds;
  }
  if (params_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* params = GetImmediateDataAs<volatile const GLint*>(
      c, params_size, immediate_data_size);
  if (!validators_->sampler_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glSamplerParameteriv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  DoSamplerParameteriv(sampler, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleScissor(uint32_t immediate_data_size,
                                             const volatile void* cmd_data) {
  const volatile gles2::cmds::Scissor& c =
      *static_cast<const volatile gles2::cmds::Scissor*>(cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glScissor", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glScissor", "height < 0");
    return error::kNoError;
  }
  if (state_.scissor_x != x || state_.scissor_y != y ||
      state_.scissor_width != width || state_.scissor_height != height) {
    state_.scissor_x = x;
    state_.scissor_y = y;
    state_.scissor_width = width;
    state_.scissor_height = height;
    DoScissor(x, y, width, height);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleShaderSourceBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ShaderSourceBucket& c =
      *static_cast<const volatile gles2::cmds::ShaderSourceBucket*>(cmd_data);
  GLuint shader = static_cast<GLuint>(c.shader);

  Bucket* bucket = GetBucket(c.str_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  std::vector<char*> strs;
  std::vector<GLint> len;
  if (!bucket->GetAsStrings(&count, &strs, &len)) {
    return error::kInvalidArguments;
  }
  const char** str =
      strs.size() > 0 ? const_cast<const char**>(&strs[0]) : nullptr;
  const GLint* length =
      len.size() > 0 ? const_cast<const GLint*>(&len[0]) : nullptr;
  (void)length;
  DoShaderSource(shader, count, str, length);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMultiDrawBeginCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MultiDrawBeginCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::MultiDrawBeginCHROMIUM*>(
          cmd_data);
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  GLsizei drawcount = static_cast<GLsizei>(c.drawcount);
  if (drawcount < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glMultiDrawBeginCHROMIUM",
                       "drawcount < 0");
    return error::kNoError;
  }
  DoMultiDrawBeginCHROMIUM(drawcount);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMultiDrawEndCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().webgl_multi_draw) {
    return error::kUnknownCommand;
  }

  DoMultiDrawEndCHROMIUM();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilFunc(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilFunc& c =
      *static_cast<const volatile gles2::cmds::StencilFunc*>(cmd_data);
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->cmp_function.IsValid(func)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilFunc", func, "func");
    return error::kNoError;
  }
  if (state_.stencil_front_func != func || state_.stencil_front_ref != ref ||
      state_.stencil_front_mask != mask || state_.stencil_back_func != func ||
      state_.stencil_back_ref != ref || state_.stencil_back_mask != mask) {
    state_.stencil_front_func = func;
    state_.stencil_front_ref = ref;
    state_.stencil_front_mask = mask;
    state_.stencil_back_func = func;
    state_.stencil_back_ref = ref;
    state_.stencil_back_mask = mask;
    state_.stencil_state_changed_since_validation = true;
    api()->glStencilFuncFn(func, ref, mask);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilFuncSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilFuncSeparate& c =
      *static_cast<const volatile gles2::cmds::StencilFuncSeparate*>(cmd_data);
  GLenum face = static_cast<GLenum>(c.face);
  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->face_type.IsValid(face)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilFuncSeparate", face, "face");
    return error::kNoError;
  }
  if (!validators_->cmp_function.IsValid(func)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilFuncSeparate", func, "func");
    return error::kNoError;
  }
  bool changed = false;
  if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_front_func != func ||
               state_.stencil_front_ref != ref ||
               state_.stencil_front_mask != mask;
  }
  if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_back_func != func ||
               state_.stencil_back_ref != ref ||
               state_.stencil_back_mask != mask;
  }
  if (changed) {
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
      state_.stencil_front_func = func;
      state_.stencil_front_ref = ref;
      state_.stencil_front_mask = mask;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
      state_.stencil_back_func = func;
      state_.stencil_back_ref = ref;
      state_.stencil_back_mask = mask;
    }
    state_.stencil_state_changed_since_validation = true;
    api()->glStencilFuncSeparateFn(face, func, ref, mask);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilMask(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilMask& c =
      *static_cast<const volatile gles2::cmds::StencilMask*>(cmd_data);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (state_.stencil_front_writemask != mask ||
      state_.stencil_back_writemask != mask) {
    state_.stencil_front_writemask = mask;
    state_.stencil_back_writemask = mask;
    framebuffer_state_.clear_state_dirty = true;
    state_.stencil_state_changed_since_validation = true;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilMaskSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilMaskSeparate& c =
      *static_cast<const volatile gles2::cmds::StencilMaskSeparate*>(cmd_data);
  GLenum face = static_cast<GLenum>(c.face);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->face_type.IsValid(face)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilMaskSeparate", face, "face");
    return error::kNoError;
  }
  bool changed = false;
  if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_front_writemask != mask;
  }
  if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_back_writemask != mask;
  }
  if (changed) {
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
      state_.stencil_front_writemask = mask;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
      state_.stencil_back_writemask = mask;
    }
    framebuffer_state_.clear_state_dirty = true;
    state_.stencil_state_changed_since_validation = true;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilOp(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilOp& c =
      *static_cast<const volatile gles2::cmds::StencilOp*>(cmd_data);
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  if (!validators_->stencil_op.IsValid(fail)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilOp", fail, "fail");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zfail)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilOp", zfail, "zfail");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zpass)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilOp", zpass, "zpass");
    return error::kNoError;
  }
  if (state_.stencil_front_fail_op != fail ||
      state_.stencil_front_z_fail_op != zfail ||
      state_.stencil_front_z_pass_op != zpass ||
      state_.stencil_back_fail_op != fail ||
      state_.stencil_back_z_fail_op != zfail ||
      state_.stencil_back_z_pass_op != zpass) {
    state_.stencil_front_fail_op = fail;
    state_.stencil_front_z_fail_op = zfail;
    state_.stencil_front_z_pass_op = zpass;
    state_.stencil_back_fail_op = fail;
    state_.stencil_back_z_fail_op = zfail;
    state_.stencil_back_z_pass_op = zpass;
    api()->glStencilOpFn(fail, zfail, zpass);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleStencilOpSeparate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::StencilOpSeparate& c =
      *static_cast<const volatile gles2::cmds::StencilOpSeparate*>(cmd_data);
  GLenum face = static_cast<GLenum>(c.face);
  GLenum fail = static_cast<GLenum>(c.fail);
  GLenum zfail = static_cast<GLenum>(c.zfail);
  GLenum zpass = static_cast<GLenum>(c.zpass);
  if (!validators_->face_type.IsValid(face)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilOpSeparate", face, "face");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(fail)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilOpSeparate", fail, "fail");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zfail)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilOpSeparate", zfail, "zfail");
    return error::kNoError;
  }
  if (!validators_->stencil_op.IsValid(zpass)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glStencilOpSeparate", zpass, "zpass");
    return error::kNoError;
  }
  bool changed = false;
  if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_front_fail_op != fail ||
               state_.stencil_front_z_fail_op != zfail ||
               state_.stencil_front_z_pass_op != zpass;
  }
  if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
    changed |= state_.stencil_back_fail_op != fail ||
               state_.stencil_back_z_fail_op != zfail ||
               state_.stencil_back_z_pass_op != zpass;
  }
  if (changed) {
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
      state_.stencil_front_fail_op = fail;
      state_.stencil_front_z_fail_op = zfail;
      state_.stencil_front_z_pass_op = zpass;
    }
    if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
      state_.stencil_back_fail_op = fail;
      state_.stencil_back_z_fail_op = zfail;
      state_.stencil_back_z_pass_op = zpass;
    }
    api()->glStencilOpSeparateFn(face, fail, zfail, zpass);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterf(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexParameterf& c =
      *static_cast<const volatile gles2::cmds::TexParameterf*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLfloat param = static_cast<GLfloat>(c.param);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameterf", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameterf", pname, "pname");
    return error::kNoError;
  }
  DoTexParameterf(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterfvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexParameterfvImmediate& c =
      *static_cast<const volatile gles2::cmds::TexParameterfvImmediate*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t params_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 1>(1, &params_size)) {
    return error::kOutOfBounds;
  }
  if (params_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* params = GetImmediateDataAs<volatile const GLfloat*>(
      c, params_size, immediate_data_size);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameterfv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameterfv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  DoTexParameterfv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameteri(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexParameteri& c =
      *static_cast<const volatile gles2::cmds::TexParameteri*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameteri", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameteri", pname, "pname");
    return error::kNoError;
  }
  DoTexParameteri(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexParameterivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexParameterivImmediate& c =
      *static_cast<const volatile gles2::cmds::TexParameterivImmediate*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  uint32_t params_size;
  if (!GLES2Util::ComputeDataSize<GLint, 1>(1, &params_size)) {
    return error::kOutOfBounds;
  }
  if (params_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* params = GetImmediateDataAs<volatile const GLint*>(
      c, params_size, immediate_data_size);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameteriv", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexParameteriv", pname, "pname");
    return error::kNoError;
  }
  if (params == nullptr) {
    return error::kOutOfBounds;
  }
  DoTexParameteriv(target, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexStorage3D(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::TexStorage3D& c =
      *static_cast<const volatile gles2::cmds::TexStorage3D*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLsizei levels = static_cast<GLsizei>(c.levels);
  GLenum internalFormat = static_cast<GLenum>(c.internalFormat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLsizei depth = static_cast<GLsizei>(c.depth);
  if (!validators_->texture_3_d_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexStorage3D", target, "target");
    return error::kNoError;
  }
  if (levels < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage3D", "levels < 0");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format_storage.IsValid(internalFormat)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexStorage3D", internalFormat,
                                    "internalFormat");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage3D", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage3D", "height < 0");
    return error::kNoError;
  }
  if (depth < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage3D", "depth < 0");
    return error::kNoError;
  }
  DoTexStorage3D(target, levels, internalFormat, width, height, depth);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTransformFeedbackVaryingsBucket(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::TransformFeedbackVaryingsBucket& c = *static_cast<
      const volatile gles2::cmds::TransformFeedbackVaryingsBucket*>(cmd_data);
  GLuint program = static_cast<GLuint>(c.program);

  Bucket* bucket = GetBucket(c.varyings_bucket_id);
  if (!bucket) {
    return error::kInvalidArguments;
  }
  GLsizei count = 0;
  std::vector<char*> strs;
  std::vector<GLint> len;
  if (!bucket->GetAsStrings(&count, &strs, &len)) {
    return error::kInvalidArguments;
  }
  const char** varyings =
      strs.size() > 0 ? const_cast<const char**>(&strs[0]) : nullptr;
  const GLint* length =
      len.size() > 0 ? const_cast<const GLint*>(&len[0]) : nullptr;
  (void)length;
  GLenum buffermode = static_cast<GLenum>(c.buffermode);
  if (!validators_->buffer_mode.IsValid(buffermode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTransformFeedbackVaryings", buffermode,
                                    "buffermode");
    return error::kNoError;
  }
  DoTransformFeedbackVaryings(program, count, varyings, buffermode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1f(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1f& c =
      *static_cast<const volatile gles2::cmds::Uniform1f*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat temp[1] = {
      x,
  };
  DoUniform1fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1fvImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform1fvImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLfloat, 1>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* v = GetImmediateDataAs<volatile const GLfloat*>(
      c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform1fv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform1fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1i(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1i& c =
      *static_cast<const volatile gles2::cmds::Uniform1i*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  DoUniform1i(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform1ivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform1ivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 1>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* v =
      GetImmediateDataAs<volatile const GLint*>(c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform1iv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform1iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1ui(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::Uniform1ui& c =
      *static_cast<const volatile gles2::cmds::Uniform1ui*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLuint x = static_cast<GLuint>(c.x);
  GLuint temp[1] = {
      x,
  };
  DoUniform1uiv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform1uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::Uniform1uivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform1uivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLuint, 1>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* v = GetImmediateDataAs<volatile const GLuint*>(
      c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform1uiv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform1uiv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2f(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2f& c =
      *static_cast<const volatile gles2::cmds::Uniform2f*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat temp[2] = {
      x,
      y,
  };
  DoUniform2fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform2fvImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLfloat, 2>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* v = GetImmediateDataAs<volatile const GLfloat*>(
      c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform2fv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform2fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2i(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2i& c =
      *static_cast<const volatile gles2::cmds::Uniform2i*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint temp[2] = {
      x,
      y,
  };
  DoUniform2iv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform2ivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform2ivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 2>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* v =
      GetImmediateDataAs<volatile const GLint*>(c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform2iv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform2iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2ui(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::Uniform2ui& c =
      *static_cast<const volatile gles2::cmds::Uniform2ui*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLuint x = static_cast<GLuint>(c.x);
  GLuint y = static_cast<GLuint>(c.y);
  GLuint temp[2] = {
      x,
      y,
  };
  DoUniform2uiv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform2uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::Uniform2uivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform2uivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLuint, 2>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* v = GetImmediateDataAs<volatile const GLuint*>(
      c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform2uiv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform2uiv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3f(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3f& c =
      *static_cast<const volatile gles2::cmds::Uniform3f*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat temp[3] = {
      x,
      y,
      z,
  };
  DoUniform3fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform3fvImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLfloat, 3>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* v = GetImmediateDataAs<volatile const GLfloat*>(
      c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform3fv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform3fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3i(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3i& c =
      *static_cast<const volatile gles2::cmds::Uniform3i*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint temp[3] = {
      x,
      y,
      z,
  };
  DoUniform3iv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform3ivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform3ivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 3>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* v =
      GetImmediateDataAs<volatile const GLint*>(c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform3iv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform3iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3ui(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::Uniform3ui& c =
      *static_cast<const volatile gles2::cmds::Uniform3ui*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLuint x = static_cast<GLuint>(c.x);
  GLuint y = static_cast<GLuint>(c.y);
  GLuint z = static_cast<GLuint>(c.z);
  GLuint temp[3] = {
      x,
      y,
      z,
  };
  DoUniform3uiv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform3uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::Uniform3uivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform3uivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLuint, 3>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* v = GetImmediateDataAs<volatile const GLuint*>(
      c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform3uiv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform3uiv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4f(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4f& c =
      *static_cast<const volatile gles2::cmds::Uniform4f*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  GLfloat temp[4] = {
      x,
      y,
      z,
      w,
  };
  DoUniform4fv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform4fvImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLfloat, 4>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* v = GetImmediateDataAs<volatile const GLfloat*>(
      c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform4fv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform4fv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4i(uint32_t immediate_data_size,
                                               const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4i& c =
      *static_cast<const volatile gles2::cmds::Uniform4i*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint w = static_cast<GLint>(c.w);
  GLint temp[4] = {
      x,
      y,
      z,
      w,
  };
  DoUniform4iv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::Uniform4ivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform4ivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 4>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* v =
      GetImmediateDataAs<volatile const GLint*>(c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform4iv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform4iv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4ui(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::Uniform4ui& c =
      *static_cast<const volatile gles2::cmds::Uniform4ui*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLuint x = static_cast<GLuint>(c.x);
  GLuint y = static_cast<GLuint>(c.y);
  GLuint z = static_cast<GLuint>(c.z);
  GLuint w = static_cast<GLuint>(c.w);
  GLuint temp[4] = {
      x,
      y,
      z,
      w,
  };
  DoUniform4uiv(location, 1, &temp[0]);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniform4uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::Uniform4uivImmediate& c =
      *static_cast<const volatile gles2::cmds::Uniform4uivImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t v_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLuint, 4>(count, &v_size)) {
    return error::kOutOfBounds;
  }
  if (v_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* v = GetImmediateDataAs<volatile const GLuint*>(
      c, v_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniform4uiv", "count < 0");
    return error::kNoError;
  }
  if (v == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniform4uiv(location, count, v);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix2fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t value_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 4>(count, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniformMatrix2fv", "count < 0");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix2fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix2x3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::UniformMatrix2x3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix2x3fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t value_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 6>(count, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniformMatrix2x3fv", "count < 0");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix2x3fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix2x4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::UniformMatrix2x4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix2x4fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t value_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 8>(count, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniformMatrix2x4fv", "count < 0");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix2x4fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix3fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t value_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 9>(count, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniformMatrix3fv", "count < 0");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix3fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix3x2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::UniformMatrix3x2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix3x2fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t value_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 6>(count, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniformMatrix3x2fv", "count < 0");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix3x2fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix3x4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::UniformMatrix3x4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix3x4fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t value_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 12>(count, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniformMatrix3x4fv", "count < 0");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix3x4fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UniformMatrix4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix4fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t value_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 16>(count, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniformMatrix4fv", "count < 0");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix4fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix4x2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::UniformMatrix4x2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix4x2fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t value_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 8>(count, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniformMatrix4x2fv", "count < 0");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix4x2fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUniformMatrix4x3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::UniformMatrix4x3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::UniformMatrix4x3fvImmediate*>(
          cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLsizei count = static_cast<GLsizei>(c.count);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t value_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLfloat, 12>(count, &value_size)) {
    return error::kOutOfBounds;
  }
  if (value_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* value = GetImmediateDataAs<volatile const GLfloat*>(
      c, value_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUniformMatrix4x3fv", "count < 0");
    return error::kNoError;
  }
  if (value == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix4x3fv(location, count, transpose, value);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUseProgram(uint32_t immediate_data_size,
                                                const volatile void* cmd_data) {
  const volatile gles2::cmds::UseProgram& c =
      *static_cast<const volatile gles2::cmds::UseProgram*>(cmd_data);
  GLuint program = c.program;
  DoUseProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleValidateProgram(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ValidateProgram& c =
      *static_cast<const volatile gles2::cmds::ValidateProgram*>(cmd_data);
  GLuint program = c.program;
  DoValidateProgram(program);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib1f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib1f& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib1f*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  DoVertexAttrib1f(indx, x);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib1fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib1fvImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib1fvImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t values_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 1>(1, &values_size)) {
    return error::kOutOfBounds;
  }
  if (values_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* values = GetImmediateDataAs<volatile const GLfloat*>(
      c, values_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib1fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib2f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib2f& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib2f*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  DoVertexAttrib2f(indx, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib2fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib2fvImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib2fvImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t values_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 2>(1, &values_size)) {
    return error::kOutOfBounds;
  }
  if (values_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* values = GetImmediateDataAs<volatile const GLfloat*>(
      c, values_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib2fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib3f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib3f& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib3f*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  DoVertexAttrib3f(indx, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib3fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib3fvImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib3fvImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t values_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 3>(1, &values_size)) {
    return error::kOutOfBounds;
  }
  if (values_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* values = GetImmediateDataAs<volatile const GLfloat*>(
      c, values_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib3fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib4f(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib4f& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib4f*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLfloat x = static_cast<GLfloat>(c.x);
  GLfloat y = static_cast<GLfloat>(c.y);
  GLfloat z = static_cast<GLfloat>(c.z);
  GLfloat w = static_cast<GLfloat>(c.w);
  DoVertexAttrib4f(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttrib4fvImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::VertexAttrib4fvImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttrib4fvImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t values_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 4>(1, &values_size)) {
    return error::kOutOfBounds;
  }
  if (values_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* values = GetImmediateDataAs<volatile const GLfloat*>(
      c, values_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  DoVertexAttrib4fv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttribI4i(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::VertexAttribI4i& c =
      *static_cast<const volatile gles2::cmds::VertexAttribI4i*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint z = static_cast<GLint>(c.z);
  GLint w = static_cast<GLint>(c.w);
  DoVertexAttribI4i(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttribI4ivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::VertexAttribI4ivImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttribI4ivImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t values_size;
  if (!GLES2Util::ComputeDataSize<GLint, 4>(1, &values_size)) {
    return error::kOutOfBounds;
  }
  if (values_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* values = GetImmediateDataAs<volatile const GLint*>(
      c, values_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  DoVertexAttribI4iv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttribI4ui(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::VertexAttribI4ui& c =
      *static_cast<const volatile gles2::cmds::VertexAttribI4ui*>(cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  GLuint x = static_cast<GLuint>(c.x);
  GLuint y = static_cast<GLuint>(c.y);
  GLuint z = static_cast<GLuint>(c.z);
  GLuint w = static_cast<GLuint>(c.w);
  DoVertexAttribI4ui(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleVertexAttribI4uivImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::VertexAttribI4uivImmediate& c =
      *static_cast<const volatile gles2::cmds::VertexAttribI4uivImmediate*>(
          cmd_data);
  GLuint indx = static_cast<GLuint>(c.indx);
  uint32_t values_size;
  if (!GLES2Util::ComputeDataSize<GLuint, 4>(1, &values_size)) {
    return error::kOutOfBounds;
  }
  if (values_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* values = GetImmediateDataAs<volatile const GLuint*>(
      c, values_size, immediate_data_size);
  if (values == nullptr) {
    return error::kOutOfBounds;
  }
  DoVertexAttribI4uiv(indx, values);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleViewport(uint32_t immediate_data_size,
                                              const volatile void* cmd_data) {
  const volatile gles2::cmds::Viewport& c =
      *static_cast<const volatile gles2::cmds::Viewport*>(cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glViewport", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glViewport", "height < 0");
    return error::kNoError;
  }
  DoViewport(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlitFramebufferCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BlitFramebufferCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::BlitFramebufferCHROMIUM*>(
          cmd_data);
  if (!features().chromium_framebuffer_multisample) {
    return error::kUnknownCommand;
  }

  error::Error error;
  error = WillAccessBoundFramebufferForDraw();
  if (error != error::kNoError)
    return error;
  error = WillAccessBoundFramebufferForRead();
  if (error != error::kNoError)
    return error;
  GLint srcX0 = static_cast<GLint>(c.srcX0);
  GLint srcY0 = static_cast<GLint>(c.srcY0);
  GLint srcX1 = static_cast<GLint>(c.srcX1);
  GLint srcY1 = static_cast<GLint>(c.srcY1);
  GLint dstX0 = static_cast<GLint>(c.dstX0);
  GLint dstY0 = static_cast<GLint>(c.dstY0);
  GLint dstX1 = static_cast<GLint>(c.dstX1);
  GLint dstY1 = static_cast<GLint>(c.dstY1);
  GLbitfield mask = static_cast<GLbitfield>(c.mask);
  GLenum filter = static_cast<GLenum>(c.filter);
  if (!validators_->blit_filter.IsValid(filter)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBlitFramebufferCHROMIUM", filter,
                                    "filter");
    return error::kNoError;
  }
  DoBlitFramebufferCHROMIUM(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                            dstY1, mask, filter);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRenderbufferStorageMultisampleCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::RenderbufferStorageMultisampleCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::RenderbufferStorageMultisampleCHROMIUM*>(
          cmd_data);
  if (!features().chromium_framebuffer_multisample) {
    return error::kUnknownCommand;
  }

  GLenum target = static_cast<GLenum>(c.target);
  GLsizei samples = static_cast<GLsizei>(c.samples);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->render_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glRenderbufferStorageMultisampleCHROMIUM",
                                    target, "target");
    return error::kNoError;
  }
  if (samples < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glRenderbufferStorageMultisampleCHROMIUM",
                       "samples < 0");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(internalformat)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glRenderbufferStorageMultisampleCHROMIUM",
                                    internalformat, "internalformat");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glRenderbufferStorageMultisampleCHROMIUM", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glRenderbufferStorageMultisampleCHROMIUM",
                       "height < 0");
    return error::kNoError;
  }
  DoRenderbufferStorageMultisampleCHROMIUM(target, samples, internalformat,
                                           width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRenderbufferStorageMultisampleAdvancedAMD(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::RenderbufferStorageMultisampleAdvancedAMD& c =
      *static_cast<const volatile gles2::cmds::
                       RenderbufferStorageMultisampleAdvancedAMD*>(cmd_data);
  if (!features().amd_framebuffer_multisample_advanced) {
    return error::kUnknownCommand;
  }

  GLenum target = static_cast<GLenum>(c.target);
  GLsizei samples = static_cast<GLsizei>(c.samples);
  GLsizei storageSamples = static_cast<GLsizei>(c.storageSamples);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->render_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glRenderbufferStorageMultisampleAdvancedAMD", target, "target");
    return error::kNoError;
  }
  if (samples < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glRenderbufferStorageMultisampleAdvancedAMD",
                       "samples < 0");
    return error::kNoError;
  }
  if (storageSamples < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glRenderbufferStorageMultisampleAdvancedAMD",
                       "storageSamples < 0");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(internalformat)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glRenderbufferStorageMultisampleAdvancedAMD", internalformat,
        "internalformat");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glRenderbufferStorageMultisampleAdvancedAMD",
                       "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE,
                       "glRenderbufferStorageMultisampleAdvancedAMD",
                       "height < 0");
    return error::kNoError;
  }
  DoRenderbufferStorageMultisampleAdvancedAMD(target, samples, storageSamples,
                                              internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleRenderbufferStorageMultisampleEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::RenderbufferStorageMultisampleEXT& c =
      *static_cast<
          const volatile gles2::cmds::RenderbufferStorageMultisampleEXT*>(
          cmd_data);
  if (!features().multisampled_render_to_texture) {
    return error::kUnknownCommand;
  }

  GLenum target = static_cast<GLenum>(c.target);
  GLsizei samples = static_cast<GLsizei>(c.samples);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->render_buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glRenderbufferStorageMultisampleEXT",
                                    target, "target");
    return error::kNoError;
  }
  if (samples < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT",
                       "samples < 0");
    return error::kNoError;
  }
  if (!validators_->render_buffer_format.IsValid(internalformat)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glRenderbufferStorageMultisampleEXT",
                                    internalformat, "internalformat");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT",
                       "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glRenderbufferStorageMultisampleEXT",
                       "height < 0");
    return error::kNoError;
  }
  DoRenderbufferStorageMultisampleEXT(target, samples, internalformat, width,
                                      height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferTexture2DMultisampleEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FramebufferTexture2DMultisampleEXT& c =
      *static_cast<
          const volatile gles2::cmds::FramebufferTexture2DMultisampleEXT*>(
          cmd_data);
  if (!features().multisampled_render_to_texture) {
    return error::kUnknownCommand;
  }

  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLenum textarget = static_cast<GLenum>(c.textarget);
  GLuint texture = c.texture;
  GLint level = static_cast<GLint>(c.level);
  GLsizei samples = static_cast<GLsizei>(c.samples);
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferTexture2DMultisampleEXT",
                                    target, "target");
    return error::kNoError;
  }
  if (!validators_->attachment.IsValid(attachment)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferTexture2DMultisampleEXT",
                                    attachment, "attachment");
    return error::kNoError;
  }
  if (!validators_->texture_target.IsValid(textarget)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferTexture2DMultisampleEXT",
                                    textarget, "textarget");
    return error::kNoError;
  }
  if (samples < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glFramebufferTexture2DMultisampleEXT",
                       "samples < 0");
    return error::kNoError;
  }
  DoFramebufferTexture2DMultisample(target, attachment, textarget, texture,
                                    level, samples);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexStorage2DEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexStorage2DEXT& c =
      *static_cast<const volatile gles2::cmds::TexStorage2DEXT*>(cmd_data);
  if (!features().ext_texture_storage) {
    return error::kUnknownCommand;
  }

  GLenum target = static_cast<GLenum>(c.target);
  GLsizei levels = static_cast<GLsizei>(c.levels);
  GLenum internalFormat = static_cast<GLenum>(c.internalFormat);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexStorage2DEXT", target, "target");
    return error::kNoError;
  }
  if (levels < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2DEXT", "levels < 0");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format_storage.IsValid(internalFormat)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexStorage2DEXT", internalFormat,
                                    "internalFormat");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2DEXT", "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2DEXT", "height < 0");
    return error::kNoError;
  }
  DoTexStorage2DEXT(target, levels, internalFormat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenQueriesEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenQueriesEXTImmediate& c =
      *static_cast<const volatile gles2::cmds::GenQueriesEXTImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t queries_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&queries_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* queries = GetImmediateDataAs<volatile GLuint*>(
      c, queries_size, immediate_data_size);
  if (queries == nullptr) {
    return error::kOutOfBounds;
  }
  auto queries_copy = std::make_unique<GLuint[]>(n);
  GLuint* queries_safe = queries_copy.get();
  std::copy(queries, queries + n, queries_safe);
  if (!CheckUniqueAndNonNullIds(n, queries_safe) ||
      !GenQueriesEXTHelper(n, queries_safe)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteQueriesEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteQueriesEXTImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteQueriesEXTImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t queries_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&queries_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* queries = GetImmediateDataAs<volatile const GLuint*>(
      c, queries_size, immediate_data_size);
  if (queries == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteQueriesEXTHelper(n, queries);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBeginTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::BeginTransformFeedback& c =
      *static_cast<const volatile gles2::cmds::BeginTransformFeedback*>(
          cmd_data);
  GLenum primitivemode = static_cast<GLenum>(c.primitivemode);
  if (!validators_->transform_feedback_primitive_mode.IsValid(primitivemode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBeginTransformFeedback", primitivemode,
                                    "primitivemode");
    return error::kNoError;
  }
  DoBeginTransformFeedback(primitivemode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEndTransformFeedback(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  DoEndTransformFeedback();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleInsertEventMarkerEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::InsertEventMarkerEXT& c =
      *static_cast<const volatile gles2::cmds::InsertEventMarkerEXT*>(cmd_data);

  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string str;
  if (!bucket->GetAsString(&str)) {
    return error::kInvalidArguments;
  }
  DoInsertEventMarkerEXT(0, str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePushGroupMarkerEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PushGroupMarkerEXT& c =
      *static_cast<const volatile gles2::cmds::PushGroupMarkerEXT*>(cmd_data);

  GLuint bucket_id = static_cast<GLuint>(c.bucket_id);
  Bucket* bucket = GetBucket(bucket_id);
  if (!bucket || bucket->size() == 0) {
    return error::kInvalidArguments;
  }
  std::string str;
  if (!bucket->GetAsString(&str)) {
    return error::kInvalidArguments;
  }
  DoPushGroupMarkerEXT(0, str.c_str());
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePopGroupMarkerEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  DoPopGroupMarkerEXT();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGenVertexArraysOESImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GenVertexArraysOESImmediate& c =
      *static_cast<const volatile gles2::cmds::GenVertexArraysOESImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t arrays_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&arrays_size)) {
    return error::kOutOfBounds;
  }
  volatile GLuint* arrays =
      GetImmediateDataAs<volatile GLuint*>(c, arrays_size, immediate_data_size);
  if (arrays == nullptr) {
    return error::kOutOfBounds;
  }
  auto arrays_copy = std::make_unique<GLuint[]>(n);
  GLuint* arrays_safe = arrays_copy.get();
  std::copy(arrays, arrays + n, arrays_safe);
  if (!CheckUniqueAndNonNullIds(n, arrays_safe) ||
      !GenVertexArraysOESHelper(n, arrays_safe)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDeleteVertexArraysOESImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DeleteVertexArraysOESImmediate& c =
      *static_cast<const volatile gles2::cmds::DeleteVertexArraysOESImmediate*>(
          cmd_data);
  GLsizei n = static_cast<GLsizei>(c.n);
  uint32_t arrays_size;
  if (!base::CheckMul(n, sizeof(GLuint)).AssignIfValid(&arrays_size)) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* arrays = GetImmediateDataAs<volatile const GLuint*>(
      c, arrays_size, immediate_data_size);
  if (arrays == nullptr) {
    return error::kOutOfBounds;
  }
  DeleteVertexArraysOESHelper(n, arrays);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsVertexArrayOES(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsVertexArrayOES& c =
      *static_cast<const volatile gles2::cmds::IsVertexArrayOES*>(cmd_data);
  GLuint array = c.array;
  typedef cmds::IsVertexArrayOES::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsVertexArrayOES(array);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindVertexArrayOES(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindVertexArrayOES& c =
      *static_cast<const volatile gles2::cmds::BindVertexArrayOES*>(cmd_data);
  GLuint array = c.array;
  DoBindVertexArrayOES(array);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferParameteri(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::FramebufferParameteri& c =
      *static_cast<const volatile gles2::cmds::FramebufferParameteri*>(
          cmd_data);
  if (!features().mesa_framebuffer_flip_y) {
    return error::kUnknownCommand;
  }

  GLenum target = static_cast<GLenum>(c.target);
  GLenum pname = static_cast<GLenum>(c.pname);
  GLint param = static_cast<GLint>(c.param);
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferParameteri", target,
                                    "target");
    return error::kNoError;
  }
  if (!validators_->framebuffer_parameter.IsValid(pname)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFramebufferParameteri", pname, "pname");
    return error::kNoError;
  }
  DoFramebufferParameteri(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindImageTexture(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleDispatchCompute(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleDispatchComputeIndirect(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleDrawArraysIndirect(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleDrawElementsIndirect(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleGetProgramInterfaceiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleGetProgramResourceIndex(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleGetProgramResourceName(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleGetProgramResourceiv(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleGetProgramResourceLocation(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleMemoryBarrierEXT(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleMemoryBarrierByRegion(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  return error::kUnknownCommand;
}

error::Error GLES2DecoderImpl::HandleSwapBuffers(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SwapBuffers& c =
      *static_cast<const volatile gles2::cmds::SwapBuffers*>(cmd_data);
  GLuint64 swap_id = c.swap_id();
  GLbitfield flags = static_cast<GLbitfield>(c.flags);
  if (!validators_->swap_buffers_flags.IsValid(flags)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glSwapBuffers",
                       "flags GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (c.trace_id) {
    TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("gpu_cmd_queue"),
                           "CommandBufferQueue", c.trace_id,
                           TRACE_EVENT_FLAG_FLOW_IN);
  }
  DoSwapBuffers(swap_id, flags);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleGetMaxValueInBufferCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::GetMaxValueInBufferCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::GetMaxValueInBufferCHROMIUM*>(
          cmd_data);
  GLuint buffer_id = c.buffer_id;
  GLsizei count = static_cast<GLsizei>(c.count);
  GLenum type = static_cast<GLenum>(c.type);
  GLuint offset = static_cast<GLuint>(c.offset);
  typedef cmds::GetMaxValueInBufferCHROMIUM::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glGetMaxValueInBufferCHROMIUM",
                       "count < 0");
    return error::kNoError;
  }
  if (!validators_->get_max_index_type.IsValid(type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glGetMaxValueInBufferCHROMIUM", type,
                                    "type");
    return error::kNoError;
  }
  *result_dst = DoGetMaxValueInBufferCHROMIUM(buffer_id, count, type, offset);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFlushMappedBufferRange(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::FlushMappedBufferRange& c =
      *static_cast<const volatile gles2::cmds::FlushMappedBufferRange*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLintptr offset = static_cast<GLintptr>(c.offset);
  GLsizeiptr size = static_cast<GLsizeiptr>(c.size);
  if (!validators_->buffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glFlushMappedBufferRange", target,
                                    "target");
    return error::kNoError;
  }
  if (size < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glFlushMappedBufferRange",
                       "size < 0");
    return error::kNoError;
  }
  DoFlushMappedBufferRange(target, offset, size);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopyTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopyTextureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CopyTextureCHROMIUM*>(cmd_data);
  GLuint source_id = static_cast<GLuint>(c.source_id);
  GLint source_level = static_cast<GLint>(c.source_level);
  GLenum dest_target = static_cast<GLenum>(c.dest_target);
  GLuint dest_id = static_cast<GLuint>(c.dest_id);
  GLint dest_level = static_cast<GLint>(c.dest_level);
  GLint internalformat = static_cast<GLint>(c.internalformat);
  GLenum dest_type = static_cast<GLenum>(c.dest_type);
  GLboolean unpack_flip_y = static_cast<GLboolean>(c.unpack_flip_y);
  GLboolean unpack_premultiply_alpha =
      static_cast<GLboolean>(c.unpack_premultiply_alpha);
  GLboolean unpack_unmultiply_alpha =
      static_cast<GLboolean>(c.unpack_unmultiply_alpha);
  if (!validators_->texture_target.IsValid(dest_target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCopyTextureCHROMIUM", dest_target,
                                    "dest_target");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format.IsValid(internalformat)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopyTextureCHROMIUM",
                       "internalformat GL_INVALID_VALUE");
    return error::kNoError;
  }
  if (!validators_->pixel_type.IsValid(dest_type)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCopyTextureCHROMIUM", dest_type,
                                    "dest_type");
    return error::kNoError;
  }
  DoCopyTextureCHROMIUM(source_id, source_level, dest_target, dest_id,
                        dest_level, internalformat, dest_type, unpack_flip_y,
                        unpack_premultiply_alpha, unpack_unmultiply_alpha);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCopySubTextureCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CopySubTextureCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CopySubTextureCHROMIUM*>(
          cmd_data);
  GLuint source_id = static_cast<GLuint>(c.source_id);
  GLint source_level = static_cast<GLint>(c.source_level);
  GLenum dest_target = static_cast<GLenum>(c.dest_target);
  GLuint dest_id = static_cast<GLuint>(c.dest_id);
  GLint dest_level = static_cast<GLint>(c.dest_level);
  GLint xoffset = static_cast<GLint>(c.xoffset);
  GLint yoffset = static_cast<GLint>(c.yoffset);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  GLboolean unpack_flip_y = static_cast<GLboolean>(c.unpack_flip_y);
  GLboolean unpack_premultiply_alpha =
      static_cast<GLboolean>(c.unpack_premultiply_alpha);
  GLboolean unpack_unmultiply_alpha =
      static_cast<GLboolean>(c.unpack_unmultiply_alpha);
  if (!validators_->texture_target.IsValid(dest_target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCopySubTextureCHROMIUM", dest_target,
                                    "dest_target");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTextureCHROMIUM",
                       "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCopySubTextureCHROMIUM",
                       "height < 0");
    return error::kNoError;
  }
  DoCopySubTextureCHROMIUM(source_id, source_level, dest_target, dest_id,
                           dest_level, xoffset, yoffset, x, y, width, height,
                           unpack_flip_y, unpack_premultiply_alpha,
                           unpack_unmultiply_alpha);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleProduceTextureDirectCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ProduceTextureDirectCHROMIUMImmediate& c =
      *static_cast<
          const volatile gles2::cmds::ProduceTextureDirectCHROMIUMImmediate*>(
          cmd_data);
  GLuint texture = c.texture;
  uint32_t mailbox_size;
  if (!GLES2Util::ComputeDataSize<GLbyte, 16>(1, &mailbox_size)) {
    return error::kOutOfBounds;
  }
  if (mailbox_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile GLbyte* mailbox = GetImmediateDataAs<volatile GLbyte*>(
      c, mailbox_size, immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  DoProduceTextureDirectCHROMIUM(texture, mailbox);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCreateAndConsumeTextureINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CreateAndConsumeTextureINTERNALImmediate& c =
      *static_cast<const volatile gles2::cmds::
                       CreateAndConsumeTextureINTERNALImmediate*>(cmd_data);
  GLuint texture = static_cast<GLuint>(c.texture);
  uint32_t mailbox_size;
  if (!GLES2Util::ComputeDataSize<GLbyte, 16>(1, &mailbox_size)) {
    return error::kOutOfBounds;
  }
  if (mailbox_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox = GetImmediateDataAs<volatile const GLbyte*>(
      c, mailbox_size, immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  DoCreateAndConsumeTextureINTERNAL(texture, mailbox);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindTexImage2DCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindTexImage2DCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::BindTexImage2DCHROMIUM*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint imageId = static_cast<GLint>(c.imageId);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBindTexImage2DCHROMIUM", target,
                                    "target");
    return error::kNoError;
  }
  DoBindTexImage2DCHROMIUM(target, imageId);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBindTexImage2DWithInternalformatCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BindTexImage2DWithInternalformatCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::
                       BindTexImage2DWithInternalformatCHROMIUM*>(cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  GLint imageId = static_cast<GLint>(c.imageId);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glBindTexImage2DWithInternalformatCHROMIUM", target, "target");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format.IsValid(internalformat)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM(
        "glBindTexImage2DWithInternalformatCHROMIUM", internalformat,
        "internalformat");
    return error::kNoError;
  }
  DoBindTexImage2DWithInternalformatCHROMIUM(target, internalformat, imageId);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleReleaseTexImage2DCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ReleaseTexImage2DCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ReleaseTexImage2DCHROMIUM*>(
          cmd_data);
  GLenum target = static_cast<GLenum>(c.target);
  GLint imageId = static_cast<GLint>(c.imageId);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glReleaseTexImage2DCHROMIUM", target,
                                    "target");
    return error::kNoError;
  }
  DoReleaseTexImage2DCHROMIUM(target, imageId);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTraceEndCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  DoTraceEndCHROMIUM();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDiscardFramebufferEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DiscardFramebufferEXTImmediate& c =
      *static_cast<const volatile gles2::cmds::DiscardFramebufferEXTImmediate*>(
          cmd_data);
  if (!features().ext_discard_framebuffer) {
    return error::kUnknownCommand;
  }

  GLenum target = static_cast<GLenum>(c.target);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t attachments_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLenum, 1>(count, &attachments_size)) {
    return error::kOutOfBounds;
  }
  if (attachments_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLenum* attachments =
      GetImmediateDataAs<volatile const GLenum*>(c, attachments_size,
                                                 immediate_data_size);
  if (!validators_->framebuffer_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glDiscardFramebufferEXT", target,
                                    "target");
    return error::kNoError;
  }
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDiscardFramebufferEXT",
                       "count < 0");
    return error::kNoError;
  }
  if (attachments == nullptr) {
    return error::kOutOfBounds;
  }
  DoDiscardFramebufferEXT(target, count, attachments);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleLoseContextCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::LoseContextCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::LoseContextCHROMIUM*>(cmd_data);
  GLenum current = static_cast<GLenum>(c.current);
  GLenum other = static_cast<GLenum>(c.other);
  if (!validators_->reset_status.IsValid(current)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glLoseContextCHROMIUM", current,
                                    "current");
    return error::kNoError;
  }
  if (!validators_->reset_status.IsValid(other)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glLoseContextCHROMIUM", other, "other");
    return error::kNoError;
  }
  DoLoseContextCHROMIUM(current, other);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleUnpremultiplyAndDitherCopyCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::UnpremultiplyAndDitherCopyCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::UnpremultiplyAndDitherCopyCHROMIUM*>(
          cmd_data);
  if (!features().unpremultiply_and_dither_copy) {
    return error::kUnknownCommand;
  }

  GLuint source_id = static_cast<GLuint>(c.source_id);
  GLuint dest_id = static_cast<GLuint>(c.dest_id);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUnpremultiplyAndDitherCopyCHROMIUM",
                       "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glUnpremultiplyAndDitherCopyCHROMIUM",
                       "height < 0");
    return error::kNoError;
  }
  DoUnpremultiplyAndDitherCopyCHROMIUM(source_id, dest_id, x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleDrawBuffersEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::DrawBuffersEXTImmediate& c =
      *static_cast<const volatile gles2::cmds::DrawBuffersEXTImmediate*>(
          cmd_data);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t bufs_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLenum, 1>(count, &bufs_size)) {
    return error::kOutOfBounds;
  }
  if (bufs_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLenum* bufs = GetImmediateDataAs<volatile const GLenum*>(
      c, bufs_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glDrawBuffersEXT", "count < 0");
    return error::kNoError;
  }
  if (bufs == nullptr) {
    return error::kOutOfBounds;
  }
  DoDrawBuffersEXT(count, bufs);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleScheduleCALayerInUseQueryCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ScheduleCALayerInUseQueryCHROMIUMImmediate& c =
      *static_cast<const volatile gles2::cmds::
                       ScheduleCALayerInUseQueryCHROMIUMImmediate*>(cmd_data);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t textures_size = 0;
  if (count >= 0 &&
      !GLES2Util::ComputeDataSize<GLuint, 1>(count, &textures_size)) {
    return error::kOutOfBounds;
  }
  if (textures_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLuint* textures = GetImmediateDataAs<volatile const GLuint*>(
      c, textures_size, immediate_data_size);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glScheduleCALayerInUseQueryCHROMIUM",
                       "count < 0");
    return error::kNoError;
  }
  if (textures == nullptr) {
    return error::kOutOfBounds;
  }
  DoScheduleCALayerInUseQueryCHROMIUM(count, textures);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCommitOverlayPlanesCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CommitOverlayPlanesCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CommitOverlayPlanesCHROMIUM*>(
          cmd_data);
  GLuint64 swap_id = c.swap_id();
  GLbitfield flags = static_cast<GLbitfield>(c.flags);
  if (!validators_->swap_buffers_flags.IsValid(flags)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glCommitOverlayPlanesCHROMIUM",
                       "flags GL_INVALID_VALUE");
    return error::kNoError;
  }
  DoCommitOverlayPlanes(swap_id, flags);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFlushDriverCachesCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  DoFlushDriverCachesCHROMIUM();
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleScheduleDCLayerCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ScheduleDCLayerCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ScheduleDCLayerCHROMIUM*>(
          cmd_data);
  GLuint texture_0 = static_cast<GLuint>(c.texture_0);
  GLuint texture_1 = static_cast<GLuint>(c.texture_1);
  GLint z_order = static_cast<GLint>(c.z_order);
  GLint content_x = static_cast<GLint>(c.content_x);
  GLint content_y = static_cast<GLint>(c.content_y);
  GLint content_width = static_cast<GLint>(c.content_width);
  GLint content_height = static_cast<GLint>(c.content_height);
  GLint quad_x = static_cast<GLint>(c.quad_x);
  GLint quad_y = static_cast<GLint>(c.quad_y);
  GLint quad_width = static_cast<GLint>(c.quad_width);
  GLint quad_height = static_cast<GLint>(c.quad_height);
  GLfloat transform_c1r1 = static_cast<GLfloat>(c.transform_c1r1);
  GLfloat transform_c2r1 = static_cast<GLfloat>(c.transform_c2r1);
  GLfloat transform_c1r2 = static_cast<GLfloat>(c.transform_c1r2);
  GLfloat transform_c2r2 = static_cast<GLfloat>(c.transform_c2r2);
  GLfloat transform_tx = static_cast<GLfloat>(c.transform_tx);
  GLfloat transform_ty = static_cast<GLfloat>(c.transform_ty);
  GLboolean is_clipped = static_cast<GLboolean>(c.is_clipped);
  GLint clip_x = static_cast<GLint>(c.clip_x);
  GLint clip_y = static_cast<GLint>(c.clip_y);
  GLint clip_width = static_cast<GLint>(c.clip_width);
  GLint clip_height = static_cast<GLint>(c.clip_height);
  GLuint protected_video_type = static_cast<GLuint>(c.protected_video_type);
  DoScheduleDCLayerCHROMIUM(
      texture_0, texture_1, z_order, content_x, content_y, content_width,
      content_height, quad_x, quad_y, quad_width, quad_height, transform_c1r1,
      transform_c2r1, transform_c1r2, transform_c2r2, transform_tx,
      transform_ty, is_clipped, clip_x, clip_y, clip_width, clip_height,
      protected_video_type);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMatrixLoadfCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MatrixLoadfCHROMIUMImmediate& c =
      *static_cast<const volatile gles2::cmds::MatrixLoadfCHROMIUMImmediate*>(
          cmd_data);
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }

  GLenum matrixMode = static_cast<GLenum>(c.matrixMode);
  uint32_t m_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 16>(1, &m_size)) {
    return error::kOutOfBounds;
  }
  if (m_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* m = GetImmediateDataAs<volatile const GLfloat*>(
      c, m_size, immediate_data_size);
  if (!validators_->matrix_mode.IsValid(matrixMode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glMatrixLoadfCHROMIUM", matrixMode,
                                    "matrixMode");
    return error::kNoError;
  }
  if (m == nullptr) {
    return error::kOutOfBounds;
  }
  DoMatrixLoadfCHROMIUM(matrixMode, m);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMatrixLoadIdentityCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MatrixLoadIdentityCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::MatrixLoadIdentityCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }

  GLenum matrixMode = static_cast<GLenum>(c.matrixMode);
  if (!validators_->matrix_mode.IsValid(matrixMode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glMatrixLoadIdentityCHROMIUM", matrixMode,
                                    "matrixMode");
    return error::kNoError;
  }
  DoMatrixLoadIdentityCHROMIUM(matrixMode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleIsPathCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::IsPathCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::IsPathCHROMIUM*>(cmd_data);
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }

  GLuint path = c.path;
  typedef cmds::IsPathCHROMIUM::Result Result;
  Result* result_dst = GetSharedMemoryAs<Result*>(
      c.result_shm_id, c.result_shm_offset, sizeof(*result_dst));
  if (!result_dst) {
    return error::kOutOfBounds;
  }
  *result_dst = DoIsPathCHROMIUM(path);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandlePathStencilFuncCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::PathStencilFuncCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::PathStencilFuncCHROMIUM*>(
          cmd_data);
  if (!features().chromium_path_rendering) {
    return error::kUnknownCommand;
  }

  GLenum func = static_cast<GLenum>(c.func);
  GLint ref = static_cast<GLint>(c.ref);
  GLuint mask = static_cast<GLuint>(c.mask);
  if (!validators_->cmp_function.IsValid(func)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glPathStencilFuncCHROMIUM", func, "func");
    return error::kNoError;
  }
  if (state_.stencil_path_func != func || state_.stencil_path_ref != ref ||
      state_.stencil_path_mask != mask) {
    state_.stencil_path_func = func;
    state_.stencil_path_ref = ref;
    state_.stencil_path_mask = mask;
    glPathStencilFuncNV(func, ref, mask);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleContextVisibilityHintCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::ContextVisibilityHintCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::ContextVisibilityHintCHROMIUM*>(
          cmd_data);
  GLboolean visibility = static_cast<GLboolean>(c.visibility);
  DoContextVisibilityHintCHROMIUM(visibility);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleCoverageModulationCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CoverageModulationCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::CoverageModulationCHROMIUM*>(
          cmd_data);
  if (!features().chromium_framebuffer_mixed_samples) {
    return error::kUnknownCommand;
  }

  GLenum components = static_cast<GLenum>(c.components);
  if (!validators_->coverage_modulation_components.IsValid(components)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glCoverageModulationCHROMIUM", components,
                                    "components");
    return error::kNoError;
  }
  if (state_.coverage_modulation != components) {
    state_.coverage_modulation = components;
    glCoverageModulationNV(components);
  }
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBlendBarrierKHR(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!features().blend_equation_advanced) {
    return error::kUnknownCommand;
  }

  api()->glBlendBarrierKHRFn();
  return error::kNoError;
}

error::Error
GLES2DecoderImpl::HandleUniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::
      UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate& c = *static_cast<
          const volatile gles2::cmds::
              UniformMatrix4fvStreamTextureMatrixCHROMIUMImmediate*>(cmd_data);
  GLint location = static_cast<GLint>(c.location);
  GLboolean transpose = static_cast<GLboolean>(c.transpose);
  uint32_t transform_size;
  if (!GLES2Util::ComputeDataSize<GLfloat, 16>(1, &transform_size)) {
    return error::kOutOfBounds;
  }
  if (transform_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLfloat* transform =
      GetImmediateDataAs<volatile const GLfloat*>(c, transform_size,
                                                  immediate_data_size);
  if (transform == nullptr) {
    return error::kOutOfBounds;
  }
  DoUniformMatrix4fvStreamTextureMatrixCHROMIUM(location, transpose, transform);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleOverlayPromotionHintCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::OverlayPromotionHintCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::OverlayPromotionHintCHROMIUM*>(
          cmd_data);
  GLuint texture = c.texture;
  GLboolean promotion_hint = static_cast<GLboolean>(c.promotion_hint);
  GLint display_x = static_cast<GLint>(c.display_x);
  GLint display_y = static_cast<GLint>(c.display_y);
  GLint display_width = static_cast<GLint>(c.display_width);
  GLint display_height = static_cast<GLint>(c.display_height);
  DoOverlayPromotionHintCHROMIUM(texture, promotion_hint, display_x, display_y,
                                 display_width, display_height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSwapBuffersWithBoundsCHROMIUMImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SwapBuffersWithBoundsCHROMIUMImmediate& c =
      *static_cast<
          const volatile gles2::cmds::SwapBuffersWithBoundsCHROMIUMImmediate*>(
          cmd_data);
  GLuint64 swap_id = c.swap_id();
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t rects_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 4>(count, &rects_size)) {
    return error::kOutOfBounds;
  }
  if (rects_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* rects = GetImmediateDataAs<volatile const GLint*>(
      c, rects_size, immediate_data_size);
  GLbitfield flags = static_cast<GLbitfield>(c.flags);
  if (count < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glSwapBuffersWithBoundsCHROMIUM",
                       "count < 0");
    return error::kNoError;
  }
  if (rects == nullptr) {
    return error::kOutOfBounds;
  }
  if (!validators_->swap_buffers_flags.IsValid(flags)) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glSwapBuffersWithBoundsCHROMIUM",
                       "flags GL_INVALID_VALUE");
    return error::kNoError;
  }
  DoSwapBuffersWithBoundsCHROMIUM(swap_id, count, rects, flags);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSetDrawRectangleCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SetDrawRectangleCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::SetDrawRectangleCHROMIUM*>(
          cmd_data);
  GLint x = static_cast<GLint>(c.x);
  GLint y = static_cast<GLint>(c.y);
  GLint width = static_cast<GLint>(c.width);
  GLint height = static_cast<GLint>(c.height);
  DoSetDrawRectangleCHROMIUM(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSetEnableDCLayersCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::SetEnableDCLayersCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::SetEnableDCLayersCHROMIUM*>(
          cmd_data);
  GLboolean enabled = static_cast<GLboolean>(c.enabled);
  DoSetEnableDCLayersCHROMIUM(enabled);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleTexStorage2DImageCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::TexStorage2DImageCHROMIUM& c =
      *static_cast<const volatile gles2::cmds::TexStorage2DImageCHROMIUM*>(
          cmd_data);
  if (!features().chromium_texture_storage_image) {
    return error::kUnknownCommand;
  }

  GLenum target = static_cast<GLenum>(c.target);
  GLenum internalFormat = static_cast<GLenum>(c.internalFormat);
  GLenum bufferUsage = static_cast<GLenum>(c.bufferUsage);
  GLsizei width = static_cast<GLsizei>(c.width);
  GLsizei height = static_cast<GLsizei>(c.height);
  if (!validators_->texture_bind_target.IsValid(target)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexStorage2DImageCHROMIUM", target,
                                    "target");
    return error::kNoError;
  }
  if (!validators_->texture_internal_format_storage.IsValid(internalFormat)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glTexStorage2DImageCHROMIUM",
                                    internalFormat, "internalFormat");
    return error::kNoError;
  }
  if (width < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2DImageCHROMIUM",
                       "width < 0");
    return error::kNoError;
  }
  if (height < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glTexStorage2DImageCHROMIUM",
                       "height < 0");
    return error::kNoError;
  }
  DoTexStorage2DImageCHROMIUM(target, internalFormat, bufferUsage, width,
                              height);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleWindowRectanglesEXTImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::WindowRectanglesEXTImmediate& c =
      *static_cast<const volatile gles2::cmds::WindowRectanglesEXTImmediate*>(
          cmd_data);
  if (!features().ext_window_rectangles) {
    return error::kUnknownCommand;
  }

  GLenum mode = static_cast<GLenum>(c.mode);
  GLsizei count = static_cast<GLsizei>(c.count);
  uint32_t box_size = 0;
  if (count >= 0 && !GLES2Util::ComputeDataSize<GLint, 4>(count, &box_size)) {
    return error::kOutOfBounds;
  }
  if (box_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLint* box = GetImmediateDataAs<volatile const GLint*>(
      c, box_size, immediate_data_size);
  if (!validators_->window_rectangles_mode.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glWindowRectanglesEXT", mode, "mode");
    return error::kNoError;
  }
  if (box == nullptr) {
    return error::kOutOfBounds;
  }
  DoWindowRectanglesEXT(mode, count, box);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleSetReadbackBufferShadowAllocationINTERNAL(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::SetReadbackBufferShadowAllocationINTERNAL& c =
      *static_cast<const volatile gles2::cmds::
                       SetReadbackBufferShadowAllocationINTERNAL*>(cmd_data);
  GLuint buffer_id = c.buffer_id;
  GLint shm_id = static_cast<GLint>(c.shm_id);
  GLuint shm_offset = static_cast<GLuint>(c.shm_offset);
  GLuint size = static_cast<GLuint>(c.size);
  DoSetReadbackBufferShadowAllocationINTERNAL(buffer_id, shm_id, shm_offset,
                                              size);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleFramebufferTextureMultiviewOVR(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  if (!feature_info_->IsWebGL2OrES3OrHigherContext())
    return error::kUnknownCommand;
  const volatile gles2::cmds::FramebufferTextureMultiviewOVR& c =
      *static_cast<const volatile gles2::cmds::FramebufferTextureMultiviewOVR*>(
          cmd_data);
  if (!features().ovr_multiview2) {
    return error::kUnknownCommand;
  }

  GLenum target = static_cast<GLenum>(c.target);
  GLenum attachment = static_cast<GLenum>(c.attachment);
  GLuint texture = static_cast<GLuint>(c.texture);
  GLint level = static_cast<GLint>(c.level);
  GLint baseViewIndex = static_cast<GLint>(c.baseViewIndex);
  GLsizei numViews = static_cast<GLsizei>(c.numViews);
  if (numViews < 0) {
    LOCAL_SET_GL_ERROR(GL_INVALID_VALUE, "glFramebufferTextureMultiviewOVR",
                       "numViews < 0");
    return error::kNoError;
  }
  DoFramebufferTextureMultiviewOVR(target, attachment, texture, level,
                                   baseViewIndex, numViews);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleMaxShaderCompilerThreadsKHR(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::MaxShaderCompilerThreadsKHR& c =
      *static_cast<const volatile gles2::cmds::MaxShaderCompilerThreadsKHR*>(
          cmd_data);
  if (!features().khr_parallel_shader_compile) {
    return error::kUnknownCommand;
  }

  GLuint count = static_cast<GLuint>(c.count);
  api()->glMaxShaderCompilerThreadsKHRFn(count);
  return error::kNoError;
}

error::Error
GLES2DecoderImpl::HandleCreateAndTexStorage2DSharedImageINTERNALImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate&
      c = *static_cast<const volatile gles2::cmds::
                           CreateAndTexStorage2DSharedImageINTERNALImmediate*>(
          cmd_data);
  GLuint texture = static_cast<GLuint>(c.texture);
  GLenum internalformat = static_cast<GLenum>(c.internalformat);
  uint32_t mailbox_size;
  if (!GLES2Util::ComputeDataSize<GLbyte, 16>(1, &mailbox_size)) {
    return error::kOutOfBounds;
  }
  if (mailbox_size > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox = GetImmediateDataAs<volatile const GLbyte*>(
      c, mailbox_size, immediate_data_size);
  if (mailbox == nullptr) {
    return error::kOutOfBounds;
  }
  DoCreateAndTexStorage2DSharedImageINTERNAL(texture, internalformat, mailbox);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleBeginSharedImageAccessDirectCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::BeginSharedImageAccessDirectCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::BeginSharedImageAccessDirectCHROMIUM*>(
          cmd_data);
  GLuint texture = static_cast<GLuint>(c.texture);
  GLenum mode = static_cast<GLenum>(c.mode);
  if (!validators_->shared_image_access_mode.IsValid(mode)) {
    LOCAL_SET_GL_ERROR_INVALID_ENUM("glBeginSharedImageAccessDirectCHROMIUM",
                                    mode, "mode");
    return error::kNoError;
  }
  DoBeginSharedImageAccessDirectCHROMIUM(texture, mode);
  return error::kNoError;
}

error::Error GLES2DecoderImpl::HandleEndSharedImageAccessDirectCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile gles2::cmds::EndSharedImageAccessDirectCHROMIUM& c =
      *static_cast<
          const volatile gles2::cmds::EndSharedImageAccessDirectCHROMIUM*>(
          cmd_data);
  GLuint texture = static_cast<GLuint>(c.texture);
  DoEndSharedImageAccessDirectCHROMIUM(texture);
  return error::kNoError;
}

bool GLES2DecoderImpl::SetCapabilityState(GLenum cap, bool enabled) {
  switch (cap) {
    case GL_BLEND:
      state_.enable_flags.blend = enabled;
      if (state_.enable_flags.cached_blend != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_blend = enabled;
        return true;
      }
      return false;
    case GL_CULL_FACE:
      state_.enable_flags.cull_face = enabled;
      if (state_.enable_flags.cached_cull_face != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_cull_face = enabled;
        return true;
      }
      return false;
    case GL_DEPTH_TEST:
      state_.enable_flags.depth_test = enabled;
      if (state_.enable_flags.cached_depth_test != enabled ||
          state_.ignore_cached_state) {
        framebuffer_state_.clear_state_dirty = true;
      }
      return false;
    case GL_DITHER:
      state_.enable_flags.dither = enabled;
      if (state_.enable_flags.cached_dither != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_dither = enabled;
        return true;
      }
      return false;
    case GL_FRAMEBUFFER_SRGB_EXT:
      state_.enable_flags.framebuffer_srgb_ext = enabled;
      if (state_.enable_flags.cached_framebuffer_srgb_ext != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_framebuffer_srgb_ext = enabled;
        return true;
      }
      return false;
    case GL_POLYGON_OFFSET_FILL:
      state_.enable_flags.polygon_offset_fill = enabled;
      if (state_.enable_flags.cached_polygon_offset_fill != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_polygon_offset_fill = enabled;
        return true;
      }
      return false;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      state_.enable_flags.sample_alpha_to_coverage = enabled;
      if (state_.enable_flags.cached_sample_alpha_to_coverage != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_sample_alpha_to_coverage = enabled;
        return true;
      }
      return false;
    case GL_SAMPLE_COVERAGE:
      state_.enable_flags.sample_coverage = enabled;
      if (state_.enable_flags.cached_sample_coverage != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_sample_coverage = enabled;
        return true;
      }
      return false;
    case GL_SCISSOR_TEST:
      state_.enable_flags.scissor_test = enabled;
      if (state_.enable_flags.cached_scissor_test != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_scissor_test = enabled;
        return true;
      }
      return false;
    case GL_STENCIL_TEST:
      state_.enable_flags.stencil_test = enabled;
      if (state_.enable_flags.cached_stencil_test != enabled ||
          state_.ignore_cached_state) {
        state_.stencil_state_changed_since_validation = true;
        framebuffer_state_.clear_state_dirty = true;
      }
      return false;
    case GL_RASTERIZER_DISCARD:
      state_.enable_flags.rasterizer_discard = enabled;
      if (state_.enable_flags.cached_rasterizer_discard != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_rasterizer_discard = enabled;
        return true;
      }
      return false;
    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
      state_.enable_flags.primitive_restart_fixed_index = enabled;
      if (state_.enable_flags.cached_primitive_restart_fixed_index != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_primitive_restart_fixed_index = enabled;
        return true;
      }
      return false;
    case GL_MULTISAMPLE_EXT:
      state_.enable_flags.multisample_ext = enabled;
      if (state_.enable_flags.cached_multisample_ext != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_multisample_ext = enabled;
        return true;
      }
      return false;
    case GL_SAMPLE_ALPHA_TO_ONE_EXT:
      state_.enable_flags.sample_alpha_to_one_ext = enabled;
      if (state_.enable_flags.cached_sample_alpha_to_one_ext != enabled ||
          state_.ignore_cached_state) {
        state_.enable_flags.cached_sample_alpha_to_one_ext = enabled;
        return true;
      }
      return false;
    default:
      NOTREACHED();
      return false;
  }
}
#endif  // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_DECODER_AUTOGEN_H_
