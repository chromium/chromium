// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_polygon_offset_clamp.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTPolygonOffsetClamp::EXTPolygonOffsetClamp(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_polygon_offset_clamp");
}

WebGLExtensionName EXTPolygonOffsetClamp::GetName() const {
  return kEXTPolygonOffsetClampName;
}

bool EXTPolygonOffsetClamp::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_EXT_polygon_offset_clamp");
}

const char* EXTPolygonOffsetClamp::ExtensionName() {
  return "EXT_polygon_offset_clamp";
}

void EXTPolygonOffsetClamp::polygonOffsetClampEXT(GLfloat factor,
                                                  GLfloat units,
                                                  GLfloat clamp) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  scoped.Context()->ContextGL()->PolygonOffsetClampEXT(factor, units, clamp);
}

}  // namespace blink
