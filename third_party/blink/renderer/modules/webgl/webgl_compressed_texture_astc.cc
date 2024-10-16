// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_astc.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

const WebGLCompressedTextureASTC::BlockSizeCompressASTC
    WebGLCompressedTextureASTC::kBlockSizeCompressASTC[] = {
        {GL_COMPRESSED_RGBA_ASTC_4x4_KHR, 4, 4},
        {GL_COMPRESSED_RGBA_ASTC_5x4_KHR, 5, 4},
        {GL_COMPRESSED_RGBA_ASTC_5x5_KHR, 5, 5},
        {GL_COMPRESSED_RGBA_ASTC_6x5_KHR, 6, 5},
        {GL_COMPRESSED_RGBA_ASTC_6x6_KHR, 6, 6},
        {GL_COMPRESSED_RGBA_ASTC_8x5_KHR, 8, 5},
        {GL_COMPRESSED_RGBA_ASTC_8x6_KHR, 8, 6},
        {GL_COMPRESSED_RGBA_ASTC_8x8_KHR, 8, 8},
        {GL_COMPRESSED_RGBA_ASTC_10x5_KHR, 10, 5},
        {GL_COMPRESSED_RGBA_ASTC_10x6_KHR, 10, 6},
        {GL_COMPRESSED_RGBA_ASTC_10x8_KHR, 10, 8},
        {GL_COMPRESSED_RGBA_ASTC_10x10_KHR, 10, 10},
        {GL_COMPRESSED_RGBA_ASTC_12x10_KHR, 12, 10},
        {GL_COMPRESSED_RGBA_ASTC_12x12_KHR, 12, 12}};

WebGLCompressedTextureASTC::WebGLCompressedTextureASTC(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_KHR_texture_compression_astc_ldr");

  supports_hdr = context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_KHR_texture_compression_astc_hdr");

  const int kAlphaFormatGap =
      GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR - GL_COMPRESSED_RGBA_ASTC_4x4_KHR;

  for (size_t i = 0;
       i < std::size(WebGLCompressedTextureASTC::kBlockSizeCompressASTC); i++) {
    /* GL_COMPRESSED_RGBA_ASTC(0x93B0 ~ 0x93BD) */
    context->AddCompressedTextureFormat(
        WebGLCompressedTextureASTC::kBlockSizeCompressASTC[i].compress_type);
    /* GL_COMPRESSED_SRGB8_ALPHA8_ASTC(0x93D0 ~ 0x93DD) */
    context->AddCompressedTextureFormat(
        WebGLCompressedTextureASTC::kBlockSizeCompressASTC[i].compress_type +
        kAlphaFormatGap);
  }
}

WebGLExtensionName WebGLCompressedTextureASTC::GetName() const {
  return kWebGLCompressedTextureASTCName;
}

bool WebGLCompressedTextureASTC::Supported(WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensions_util = context->ExtensionsUtil();
  return extensions_util->SupportsExtension(
      "GL_KHR_texture_compression_astc_ldr");
}

const char* WebGLCompressedTextureASTC::ExtensionName() {
  return "WEBGL_compressed_texture_astc";
}

Vector<String> WebGLCompressedTextureASTC::getSupportedProfiles() {
  Vector<String> result = {"ldr"};
  if (supports_hdr) {
    result.emplace_back("hdr");
  }
  return result;
}

}  // namespace blink
