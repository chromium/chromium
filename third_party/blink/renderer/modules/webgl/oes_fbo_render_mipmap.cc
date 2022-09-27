// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/oes_fbo_render_mipmap.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

OESFboRenderMipmap::OESFboRenderMipmap(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_OES_fbo_render_mipmap");
}

WebGLExtensionName OESFboRenderMipmap::GetName() const {
  return kOESFboRenderMipmapName;
}

bool OESFboRenderMipmap::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_OES_fbo_render_mipmap");
}

const char* OESFboRenderMipmap::ExtensionName() {
  return "OES_fbo_render_mipmap";
}

}  // namespace blink
