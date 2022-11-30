// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_float_blend.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTFloatBlend::EXTFloatBlend(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_EXT_float_blend");
}

WebGLExtensionName EXTFloatBlend::GetName() const {
  return kEXTFloatBlendName;
}

bool EXTFloatBlend::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_EXT_float_blend");
}

const char* EXTFloatBlend::ExtensionName() {
  return "EXT_float_blend";
}

}  // namespace blink
