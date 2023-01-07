// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_etc1.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLCompressedTextureETC1::WebGLCompressedTextureETC1(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_OES_compressed_ETC1_RGB8_texture");

  context->AddCompressedTextureFormat(GL_ETC1_RGB8_OES);
}

WebGLExtensionName WebGLCompressedTextureETC1::GetName() const {
  return kWebGLCompressedTextureETC1Name;
}

bool WebGLCompressedTextureETC1::Supported(WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensions_util = context->ExtensionsUtil();
  return extensions_util->SupportsExtension(
      "GL_OES_compressed_ETC1_RGB8_texture");
}

const char* WebGLCompressedTextureETC1::ExtensionName() {
  return "WEBGL_compressed_texture_etc1";
}

}  // namespace blink
