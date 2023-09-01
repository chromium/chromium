// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_texture_mirror_clamp_to_edge.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTTextureMirrorClampToEdge::EXTTextureMirrorClampToEdge(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_texture_mirror_clamp_to_edge");
}

WebGLExtensionName EXTTextureMirrorClampToEdge::GetName() const {
  return kEXTTextureMirrorClampToEdgeName;
}

bool EXTTextureMirrorClampToEdge::Supported(
    WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_EXT_texture_mirror_clamp_to_edge");
}

const char* EXTTextureMirrorClampToEdge::ExtensionName() {
  return "EXT_texture_mirror_clamp_to_edge";
}

}  // namespace blink
