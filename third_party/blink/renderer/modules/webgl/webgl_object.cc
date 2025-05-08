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

#include "third_party/blink/renderer/modules/webgl/webgl_object.h"

#include <limits>

#include "third_party/blink/renderer/modules/webgl/webgl_context_object_support.h"

namespace blink {

WebGLObject::WebGLObject(WebGLContextObjectSupport* context)
    : context_(context),
      cached_number_of_context_losses_(std::numeric_limits<uint32_t>::max()) {
  if (context_) {
    cached_number_of_context_losses_ = context->NumberOfContextLosses();
  }
}

WebGLObject::~WebGLObject() = default;

bool WebGLObject::Validate(const WebGLContextObjectSupport* context) const {
  // The contexts and context groups no longer maintain references to all
  // the objects they ever created, so there's no way to invalidate them
  // eagerly during context loss. The invalidation is discovered lazily.
  return (context == context_ && context_ != nullptr &&
          cached_number_of_context_losses_ == context->NumberOfContextLosses());
}

void WebGLObject::SetObject(GLuint object) {
  // SetObject may only be called when this container is in the
  // uninitialized state: object==0 && marked_for_deletion==false.
  DCHECK(!object_);
  DCHECK(!MarkedForDeletion());
  object_ = object;
}

void WebGLObject::ResetUnownedObject() {
  DCHECK(object_);
  object_ = 0;
}

void WebGLObject::DeleteObject(gpu::gles2::GLES2Interface* gl) {
  marked_for_deletion_ = true;
  if (!HasObject())
    return;

  if (!context_) {
    return;
  }

  if (context_->NumberOfContextLosses() != cached_number_of_context_losses_) {
    // This object has been invalidated.
    return;
  }

  if (!attachment_count_) {
    if (!gl)
      gl = context_->ContextGL();
    if (gl) {
      DeleteObjectImpl(gl);
      object_ = 0;
    }
  }
}

void WebGLObject::Detach() {
  attachment_count_ = 0;  // Make sure OpenGL resource is eventually deleted.
}

void WebGLObject::DetachAndDeleteObject() {
  // To ensure that all platform objects are deleted after being detached,
  // this method does them together.
  Detach();
  DeleteObject(nullptr);
}

void WebGLObject::Dispose() {
  DCHECK(!destruction_in_progress_);
  // This boilerplate pre-finalizer is sufficient for all subclasses, as long
  // as they implement DeleteObjectImpl properly, and don't try to touch
  // other objects on the Oilpan heap if the destructor's been entered.
  destruction_in_progress_ = true;
  DetachAndDeleteObject();
}

bool WebGLObject::DestructionInProgress() const {
  return destruction_in_progress_;
}

void WebGLObject::OnDetached(gpu::gles2::GLES2Interface* gl) {
  if (attachment_count_)
    --attachment_count_;
  if (marked_for_deletion_)
    DeleteObject(gl);
}

void WebGLObject::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
