/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/webgl_draw_buffers.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLDrawBuffers::WebGLDrawBuffers(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_EXT_draw_buffers");
}

WebGLExtensionName WebGLDrawBuffers::GetName() const {
  return kWebGLDrawBuffersName;
}

// static
bool WebGLDrawBuffers::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_EXT_draw_buffers");
}

// static
const char* WebGLDrawBuffers::ExtensionName() {
  return "WEBGL_draw_buffers";
}

void WebGLDrawBuffers::drawBuffersWEBGL(const Vector<GLenum>& buffers) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  const GLsizei buffer_count = buffers.size();
  if (!scoped.Context()->framebuffer_binding_) {
    if (buffer_count != 1) {
      scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION,
                                          "drawBuffersWEBGL",
                                          "must provide exactly one buffer");
      return;
    }

    GLenum buffer = buffers.front();
    if (buffer != GL_BACK && buffer != GL_NONE) {
      scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION,
                                          "drawBuffersWEBGL", "BACK or NONE");
      return;
    }
    // Because the backbuffer is simulated on all current WebKit ports, we need
    // to change BACK to COLOR_ATTACHMENT0.
    GLenum value = buffer == GL_BACK ? GL_COLOR_ATTACHMENT0 : GL_NONE;
    scoped.Context()->ContextGL()->DrawBuffersEXT(1, &value);
    scoped.Context()->SetBackDrawBuffer(buffer);
  } else {
    if (buffer_count > scoped.Context()->MaxDrawBuffers()) {
      scoped.Context()->SynthesizeGLError(GL_INVALID_VALUE, "drawBuffersWEBGL",
                                          "more than max draw buffers");
      return;
    }
    GLsizei index = 0;
    for (const GLenum& buffer : buffers) {
      if (buffer != GL_NONE &&
          buffer != static_cast<GLenum>(GL_COLOR_ATTACHMENT0_EXT + index)) {
        scoped.Context()->SynthesizeGLError(GL_INVALID_OPERATION,
                                            "drawBuffersWEBGL",
                                            "COLOR_ATTACHMENTi_EXT or NONE");
        return;
      }
      ++index;
    }
    scoped.Context()->framebuffer_binding_->DrawBuffers(buffers);
  }
}

}  // namespace blink
