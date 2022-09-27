// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_transform_feedback.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"

namespace blink {

WebGLTransformFeedback::WebGLTransformFeedback(WebGL2RenderingContextBase* ctx,
                                               TFType type)
    : WebGLContextObject(ctx),
      object_(0),
      type_(type),
      target_(0),
      program_(nullptr),
      active_(false),
      paused_(false) {
  GLint max_attribs = ctx->GetMaxTransformFeedbackSeparateAttribs();
  DCHECK_GE(max_attribs, 0);
  bound_indexed_transform_feedback_buffers_.resize(max_attribs);

  switch (type_) {
    case TFType::kDefault:
      break;
    case TFType::kUser: {
      GLuint tf;
      ctx->ContextGL()->GenTransformFeedbacks(1, &tf);
      object_ = tf;
      break;
    }
  }
}

WebGLTransformFeedback::~WebGLTransformFeedback() = default;

void WebGLTransformFeedback::DispatchDetached(gpu::gles2::GLES2Interface* gl) {
  for (WebGLBuffer* buffer : bound_indexed_transform_feedback_buffers_) {
    if (buffer)
      buffer->OnDetached(gl);
  }
}

void WebGLTransformFeedback::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  switch (type_) {
    case TFType::kDefault:
      break;
    case TFType::kUser:
      gl->DeleteTransformFeedbacks(1, &object_);
      object_ = 0;
      break;
  }

  // Member<> objects must not be accessed during the destruction,
  // since they could have been already finalized.
  // The finalizers of these objects will handle their detachment
  // by themselves.
  if (!DestructionInProgress())
    DispatchDetached(gl);
}

void WebGLTransformFeedback::SetTarget(GLenum target) {
  if (target_)
    return;
  if (target == GL_TRANSFORM_FEEDBACK)
    target_ = target;
}

void WebGLTransformFeedback::SetProgram(WebGLProgram* program) {
  program_ = program;
  program_link_count_ = program->LinkCount();
}

bool WebGLTransformFeedback::ValidateProgramForResume(
    WebGLProgram* program) const {
  return program && program_ == program &&
         program->LinkCount() == program_link_count_;
}

bool WebGLTransformFeedback::SetBoundIndexedTransformFeedbackBuffer(
    GLuint index,
    WebGLBuffer* buffer) {
  if (index >= bound_indexed_transform_feedback_buffers_.size())
    return false;
  if (buffer)
    buffer->OnAttached();
  if (bound_indexed_transform_feedback_buffers_[index]) {
    bound_indexed_transform_feedback_buffers_[index]->OnDetached(
        Context()->ContextGL());
  }
  bound_indexed_transform_feedback_buffers_[index] = buffer;
  return true;
}

bool WebGLTransformFeedback::GetBoundIndexedTransformFeedbackBuffer(
    GLuint index,
    WebGLBuffer** outBuffer) const {
  if (index >= bound_indexed_transform_feedback_buffers_.size())
    return false;
  *outBuffer = bound_indexed_transform_feedback_buffers_[index].Get();
  return true;
}

bool WebGLTransformFeedback::HasEnoughBuffers(GLuint num_required) const {
  if (num_required > bound_indexed_transform_feedback_buffers_.size())
    return false;
  for (GLuint i = 0; i < num_required; i++) {
    if (!bound_indexed_transform_feedback_buffers_[i])
      return false;
  }
  return true;
}

bool WebGLTransformFeedback::UsesBuffer(WebGLBuffer* buffer) {
  for (WebGLBuffer* feedback_buffer :
       bound_indexed_transform_feedback_buffers_) {
    if (feedback_buffer == buffer)
      return true;
  }
  return false;
}

void WebGLTransformFeedback::UnbindBuffer(WebGLBuffer* buffer) {
  for (wtf_size_t i = 0; i < bound_indexed_transform_feedback_buffers_.size();
       ++i) {
    if (bound_indexed_transform_feedback_buffers_[i] == buffer) {
      bound_indexed_transform_feedback_buffers_[i]->OnDetached(
          Context()->ContextGL());
      bound_indexed_transform_feedback_buffers_[i] = nullptr;
    }
  }
}

void WebGLTransformFeedback::Trace(Visitor* visitor) const {
  visitor->Trace(bound_indexed_transform_feedback_buffers_);
  visitor->Trace(program_);
  WebGLContextObject::Trace(visitor);
}

}  // namespace blink
