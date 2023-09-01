// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/nv_shader_noperspective_interpolation.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

NVShaderNoperspectiveInterpolation::NVShaderNoperspectiveInterpolation(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_NV_shader_noperspective_interpolation");
}

WebGLExtensionName NVShaderNoperspectiveInterpolation::GetName() const {
  return kNVShaderNoperspectiveInterpolationName;
}

bool NVShaderNoperspectiveInterpolation::Supported(
    WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_NV_shader_noperspective_interpolation");
}

const char* NVShaderNoperspectiveInterpolation::ExtensionName() {
  return "NV_shader_noperspective_interpolation";
}

}  // namespace blink
