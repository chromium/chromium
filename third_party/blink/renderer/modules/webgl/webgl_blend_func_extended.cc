// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_blend_func_extended.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLBlendFuncExtended::WebGLBlendFuncExtended(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_blend_func_extended");
}

WebGLExtensionName WebGLBlendFuncExtended::GetName() const {
  return kWebGLBlendFuncExtendedName;
}

bool WebGLBlendFuncExtended::Supported(WebGLRenderingContextBase* context) {
  // Ensure that the WebGL extension is supported only on passthrough
  // as the validating decoder may expose the extension string.
  DCHECK(context->GetDrawingBuffer());
  if (!context->GetDrawingBuffer()
           ->GetGraphicsInfo()
           .using_passthrough_command_decoder) {
    return false;
  }
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_EXT_blend_func_extended");
}

const char* WebGLBlendFuncExtended::ExtensionName() {
  return "WEBGL_blend_func_extended";
}

}  // namespace blink
