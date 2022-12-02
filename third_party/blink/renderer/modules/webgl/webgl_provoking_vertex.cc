// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_provoking_vertex.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLProvokingVertex::WebGLProvokingVertex(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_provoking_vertex");
}

WebGLExtensionName WebGLProvokingVertex::GetName() const {
  return kWebGLProvokingVertexName;
}

bool WebGLProvokingVertex::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_ANGLE_provoking_vertex");
}

const char* WebGLProvokingVertex::ExtensionName() {
  return "WEBGL_provoking_vertex";
}

void WebGLProvokingVertex::provokingVertexWEBGL(GLenum provokeMode) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost())
    return;
  scoped.Context()->ContextGL()->ProvokingVertexANGLE(provokeMode);
}

}  // namespace blink
