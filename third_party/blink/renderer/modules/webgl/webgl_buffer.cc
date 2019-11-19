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

#include "third_party/blink/renderer/modules/webgl/webgl_buffer.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLBuffer* WebGLBuffer::Create(WebGLRenderingContextBase* ctx) {
  return MakeGarbageCollected<WebGLBuffer>(ctx);
}

WebGLBuffer::WebGLBuffer(WebGLRenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx), initial_target_(0), size_(0) {
  GLuint buffer;
  ctx->ContextGL()->GenBuffers(1, &buffer);
  SetObject(buffer);
}

WebGLBuffer::~WebGLBuffer() = default;

void WebGLBuffer::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  gl->DeleteBuffers(1, &object_);
  object_ = 0;
}

void WebGLBuffer::SetInitialTarget(GLenum target) {
  // WebGL restricts the ability to bind buffers to multiple targets based on
  // it's initial bind point.
  DCHECK(!initial_target_);
  initial_target_ = target;
}

}  // namespace blink
