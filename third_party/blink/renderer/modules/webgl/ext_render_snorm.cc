// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_render_snorm.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTRenderSnorm::EXTRenderSnorm(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_EXT_render_snorm");
}

WebGLExtensionName EXTRenderSnorm::GetName() const {
  return kEXTRenderSnormName;
}

bool EXTRenderSnorm::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_EXT_render_snorm");
}

const char* EXTRenderSnorm::ExtensionName() {
  return "EXT_render_snorm";
}

}  // namespace blink
