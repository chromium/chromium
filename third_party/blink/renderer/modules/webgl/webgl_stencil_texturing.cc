// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_stencil_texturing.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLStencilTexturing::WebGLStencilTexturing(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_stencil_texturing");
}

WebGLExtensionName WebGLStencilTexturing::GetName() const {
  return kWebGLStencilTexturingName;
}

bool WebGLStencilTexturing::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_ANGLE_stencil_texturing");
}

const char* WebGLStencilTexturing::ExtensionName() {
  return "WEBGL_stencil_texturing";
}

}  // namespace blink
