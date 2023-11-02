// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLUnownedTexture::WebGLUnownedTexture(WebGLRenderingContextBase* ctx,
                                         GLuint texture,
                                         GLenum target)
    : WebGLTexture(ctx, texture, target) {}

void WebGLUnownedTexture::OnGLDeleteTextures() {
  // The owner of the texture name notified us that it is no longer valid.
  // Just zero it out so we're not going to use it somewhere.
  // Note that this will suppress the rest of the logic found in
  // WebGLObject::DeleteObject(), since one of the first things that the method
  // does is a check to see if |object_| is valid.
  object_ = 0;
}

void WebGLUnownedTexture::DeleteObjectImpl(gpu::gles2::GLES2Interface* gl) {
  // Normally, we would invoke gl->DeleteTextures() here, but
  // WebGLUnownedTexture does not own its texture name. Just zero it out.
  object_ = 0;
}

WebGLUnownedTexture::~WebGLUnownedTexture() = default;

}  // namespace blink
