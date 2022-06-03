// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_texture_compression_rgtc.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTTextureCompressionRGTC::EXTTextureCompressionRGTC(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_texture_compression_rgtc");
  context->AddCompressedTextureFormat(GL_COMPRESSED_RED_RGTC1_EXT);
  context->AddCompressedTextureFormat(GL_COMPRESSED_SIGNED_RED_RGTC1_EXT);
  context->AddCompressedTextureFormat(GL_COMPRESSED_RED_GREEN_RGTC2_EXT);
  context->AddCompressedTextureFormat(GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT);
}

WebGLExtensionName EXTTextureCompressionRGTC::GetName() const {
  return kEXTTextureCompressionRGTCName;
}

bool EXTTextureCompressionRGTC::Supported(WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensions_util = context->ExtensionsUtil();
  return extensions_util->SupportsExtension("GL_EXT_texture_compression_rgtc");
}

const char* EXTTextureCompressionRGTC::ExtensionName() {
  return "EXT_texture_compression_rgtc";
}

}  // namespace blink
