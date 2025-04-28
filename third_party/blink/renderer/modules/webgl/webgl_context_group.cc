/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webgl/webgl_context_group.h"

namespace blink {

WebGLContextGroup::WebGLContextGroup() : number_of_context_losses_(0) {}

gpu::gles2::GLES2Interface* WebGLContextGroup::GetAGLInterface() {
  DCHECK(!contexts_.empty());
  return (*contexts_.begin())->ContextGL();
}

void WebGLContextGroup::AddContext(WebGLRenderingContextBase* context) {
  contexts_.insert(context);
}

void WebGLContextGroup::LoseContextGroup(
    WebGLRenderingContextBase::LostContextMode mode,
    WebGLRenderingContextBase::AutoRecoveryMethod auto_recovery_method) {
  ++number_of_context_losses_;
  for (WebGLRenderingContextBase* const context : contexts_)
    context->LoseContextImpl(mode, auto_recovery_method);
}

uint32_t WebGLContextGroup::NumberOfContextLosses() const {
  return number_of_context_losses_;
}

}  // namespace blink
