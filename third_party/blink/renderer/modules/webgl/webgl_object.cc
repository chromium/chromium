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

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLObject::WebGLObject(WebGLRenderingContextBase* context)
    : cached_number_of_context_losses_(context->NumberOfContextLosses()),
      attachment_count_(0),
      marked_for_deletion_(false),
      destruction_in_progress_(false) {}

WebGLObject::~WebGLObject() = default;

uint32_t WebGLObject::CachedNumberOfContextLosses() const {
  return cached_number_of_context_losses_;
}

void WebGLObject::DeleteObject(gpu::gles2::GLES2Interface* gl) {
  marked_for_deletion_ = true;
  if (!HasObject())
    return;

  if (!HasGroupOrContext())
    return;

  if (CurrentNumberOfContextLosses() != cached_number_of_context_losses_) {
    // This object has been invalidated.
    return;
  }

  if (!attachment_count_) {
    if (!gl)
      gl = GetAGLInterface();
    if (gl) {
      DeleteObjectImpl(gl);
      // Ensure the inherited class no longer claims to have a valid object
      DCHECK(!HasObject());
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

}  // namespace blink
