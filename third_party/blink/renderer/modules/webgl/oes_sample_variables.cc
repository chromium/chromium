// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/oes_sample_variables.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

OESSampleVariables::OESSampleVariables(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_OES_sample_variables");
}

WebGLExtensionName OESSampleVariables::GetName() const {
  return kOESSampleVariablesName;
}

bool OESSampleVariables::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_OES_sample_variables");
}

const char* OESSampleVariables::ExtensionName() {
  return "OES_sample_variables";
}

}  // namespace blink
