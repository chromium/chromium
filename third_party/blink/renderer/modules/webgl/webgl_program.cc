/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/webgl_program.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_group.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLProgram::WebGLProgram(WebGLRenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx),
      link_status_(false),
      link_count_(0),
      active_transform_feedback_count_(0),
      info_valid_(true),
      required_transform_feedback_buffer_count_(0),
      required_transform_feedback_buffer_count_after_next_link_(0) {
  SetObject(ctx->ContextGL()->CreateProgram());
}

WebGLProgram::~WebGLProgram() = default;

void WebGLProgram::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteProgram(object_);
  object_ = 0;
  if (!DestructionInProgress()) {
    if (vertex_shader_) {
      vertex_shader_->OnDetached(gl);
      vertex_shader_ = nullptr;
    }
    if (fragment_shader_) {
      fragment_shader_->OnDetached(gl);
      fragment_shader_ = nullptr;
    }
  }
}

bool WebGLProgram::LinkStatus(WebGLRenderingContextBase* context) {
  CacheInfoIfNeeded(context);
  return link_status_;
}

bool WebGLProgram::CompletionStatus(WebGLRenderingContextBase* context) {
  GLint completed = 0;
  gpu::gles2::GLES2Interface* gl = context->ContextGL();
  gl->GetProgramiv(object_, GL_COMPLETION_STATUS_KHR, &completed);

  return completed;
}

void WebGLProgram::IncreaseLinkCount() {
  ++link_count_;
  info_valid_ = false;
}

void WebGLProgram::IncreaseActiveTransformFeedbackCount() {
  ++active_transform_feedback_count_;
}

void WebGLProgram::DecreaseActiveTransformFeedbackCount() {
  --active_transform_feedback_count_;
}

WebGLShader* WebGLProgram::GetAttachedShader(GLenum type) {
  switch (type) {
    case GL_VERTEX_SHADER:
      return vertex_shader_.Get();
    case GL_FRAGMENT_SHADER:
      return fragment_shader_.Get();
    default:
      return nullptr;
  }
}

bool WebGLProgram::AttachShader(WebGLShader* shader) {
  if (!shader || !shader->Object())
    return false;
  switch (shader->GetType()) {
    case GL_VERTEX_SHADER:
      if (vertex_shader_)
        return false;
      vertex_shader_ = shader;
      return true;
    case GL_FRAGMENT_SHADER:
      if (fragment_shader_)
        return false;
      fragment_shader_ = shader;
      return true;
    default:
      return false;
  }
}

bool WebGLProgram::DetachShader(WebGLShader* shader) {
  if (!shader || !shader->Object())
    return false;
  switch (shader->GetType()) {
    case GL_VERTEX_SHADER:
      if (vertex_shader_ != shader)
        return false;
      vertex_shader_ = nullptr;
      return true;
    case GL_FRAGMENT_SHADER:
      if (fragment_shader_ != shader)
        return false;
      fragment_shader_ = nullptr;
      return true;
    default:
      return false;
  }
}

void WebGLProgram::CacheInfoIfNeeded(WebGLRenderingContextBase* context) {
  if (info_valid_)
    return;
  if (!object_)
    return;
  gpu::gles2::GLES2Interface* gl = context->ContextGL();
  GLint link_status = 0;
  gl->GetProgramiv(object_, GL_LINK_STATUS, &link_status);
  setLinkStatus(link_status);
}

void WebGLProgram::setLinkStatus(bool link_status) {
  if (info_valid_)
    return;

  link_status_ = link_status;
  if (link_status_ == GL_TRUE) {
    required_transform_feedback_buffer_count_ =
        required_transform_feedback_buffer_count_after_next_link_;
  }
  info_valid_ = true;
}

void WebGLProgram::Trace(Visitor* visitor) const {
  visitor->Trace(vertex_shader_);
  visitor->Trace(fragment_shader_);
  WebGLSharedPlatform3DObject::Trace(visitor);
}

}  // namespace blink
