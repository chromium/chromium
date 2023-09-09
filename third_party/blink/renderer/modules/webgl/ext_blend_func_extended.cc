// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_blend_func_extended.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTBlendFuncExtended::EXTBlendFuncExtended(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_blend_func_extended");
}

WebGLExtensionName EXTBlendFuncExtended::GetName() const {
  return kEXTBlendFuncExtendedName;
}

bool EXTBlendFuncExtended::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_EXT_blend_func_extended");
}

const char* EXTBlendFuncExtended::ExtensionName() {
  return "EXT_blend_func_extended";
}

}  // namespace blink
