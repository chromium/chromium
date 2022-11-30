// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_texture_compression_bptc.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTTextureCompressionBPTC::EXTTextureCompressionBPTC(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_texture_compression_bptc");
  context->AddCompressedTextureFormat(GL_COMPRESSED_RGBA_BPTC_UNORM_EXT);
  context->AddCompressedTextureFormat(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT);
  context->AddCompressedTextureFormat(GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT);
  context->AddCompressedTextureFormat(
      GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT);
}

WebGLExtensionName EXTTextureCompressionBPTC::GetName() const {
  return kEXTTextureCompressionBPTCName;
}

bool EXTTextureCompressionBPTC::Supported(WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensions_util = context->ExtensionsUtil();
  return extensions_util->SupportsExtension("GL_EXT_texture_compression_bptc");
}

const char* EXTTextureCompressionBPTC::ExtensionName() {
  return "EXT_texture_compression_bptc";
}

}  // namespace blink
