// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_s3tc_srgb.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLCompressedTextureS3TCsRGB::WebGLCompressedTextureS3TCsRGB(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_texture_compression_s3tc_srgb");

  // TODO(kainino): update these with _EXT versions once
  // GL_EXT_compressed_texture_s3tc_srgb is ratified
  context->AddCompressedTextureFormat(GL_COMPRESSED_SRGB_S3TC_DXT1_NV);
  context->AddCompressedTextureFormat(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV);
  context->AddCompressedTextureFormat(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV);
  context->AddCompressedTextureFormat(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV);
}

WebGLExtensionName WebGLCompressedTextureS3TCsRGB::GetName() const {
  return kWebGLCompressedTextureS3TCsRGBName;
}

bool WebGLCompressedTextureS3TCsRGB::Supported(
    WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensions_util = context->ExtensionsUtil();
  return extensions_util->SupportsExtension(
      "GL_EXT_texture_compression_s3tc_srgb");
}

const char* WebGLCompressedTextureS3TCsRGB::ExtensionName() {
  return "WEBGL_compressed_texture_s3tc_srgb";
}

}  // namespace blink
