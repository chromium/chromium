// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_conservative_depth.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTConservativeDepth::EXTConservativeDepth(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_conservative_depth");
}

WebGLExtensionName EXTConservativeDepth::GetName() const {
  return kEXTConservativeDepthName;
}

bool EXTConservativeDepth::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_EXT_conservative_depth");
}

const char* EXTConservativeDepth::ExtensionName() {
  return "EXT_conservative_depth";
}

}  // namespace blink
