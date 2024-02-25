// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/oes_shader_multisample_interpolation.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

OESShaderMultisampleInterpolation::OESShaderMultisampleInterpolation(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_OES_shader_multisample_interpolation");
}

WebGLExtensionName OESShaderMultisampleInterpolation::GetName() const {
  return kOESShaderMultisampleInterpolationName;
}

bool OESShaderMultisampleInterpolation::Supported(
    WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_OES_shader_multisample_interpolation");
}

const char* OESShaderMultisampleInterpolation::ExtensionName() {
  return "OES_shader_multisample_interpolation";
}

}  // namespace blink
