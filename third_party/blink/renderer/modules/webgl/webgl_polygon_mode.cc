// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_polygon_mode.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLPolygonMode::WebGLPolygonMode(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_ANGLE_polygon_mode");
}

WebGLExtensionName WebGLPolygonMode::GetName() const {
  return kWebGLPolygonModeName;
}

bool WebGLPolygonMode::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_ANGLE_polygon_mode");
}

const char* WebGLPolygonMode::ExtensionName() {
  return "WEBGL_polygon_mode";
}

void WebGLPolygonMode::polygonModeWEBGL(GLenum face, GLenum mode) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  EmitDeferredPortabilityWarning(scoped.Context());
  scoped.Context()->ContextGL()->PolygonModeANGLE(face, mode);
}

void WebGLPolygonMode::EmitDeferredPortabilityWarning(
    WebGLRenderingContextBase* context) {
  if (!emitted_warning_) {
    context->EmitGLWarning(
        "this extension has very low support on mobile devices; do not rely on "
        "it for rendering effects without implementing a fallback path",
        "WEBGL_polygon_mode");
    emitted_warning_ = true;
  }
}

}  // namespace blink
