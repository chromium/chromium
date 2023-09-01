// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_depth_clamp.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTDepthClamp::EXTDepthClamp(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_EXT_depth_clamp");
}

WebGLExtensionName EXTDepthClamp::GetName() const {
  return kEXTDepthClampName;
}

bool EXTDepthClamp::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_EXT_depth_clamp");
}

const char* EXTDepthClamp::ExtensionName() {
  return "EXT_depth_clamp";
}

}  // namespace blink
