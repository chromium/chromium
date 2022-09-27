// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_texture_norm_16.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTTextureNorm16::EXTTextureNorm16(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_EXT_texture_norm16");
}

WebGLExtensionName EXTTextureNorm16::GetName() const {
  return kEXTTextureNorm16Name;
}

EXTTextureNorm16* EXTTextureNorm16::Create(WebGLRenderingContextBase* context) {
  return MakeGarbageCollected<EXTTextureNorm16>(context);
}

bool EXTTextureNorm16::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_EXT_texture_norm16");
}

const char* EXTTextureNorm16::ExtensionName() {
  return "EXT_texture_norm16";
}

}  // namespace blink
