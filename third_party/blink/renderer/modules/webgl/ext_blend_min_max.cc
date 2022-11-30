// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_blend_min_max.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTBlendMinMax::EXTBlendMinMax(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_EXT_blend_minmax");
}

WebGLExtensionName EXTBlendMinMax::GetName() const {
  return kEXTBlendMinMaxName;
}

bool EXTBlendMinMax::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_EXT_blend_minmax");
}

const char* EXTBlendMinMax::ExtensionName() {
  return "EXT_blend_minmax";
}

}  // namespace blink
