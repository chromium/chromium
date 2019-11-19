/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_s3tc.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLCompressedTextureS3TC::WebGLCompressedTextureS3TC(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_texture_compression_s3tc");
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_texture_compression_dxt1");
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_texture_compression_dxt3");
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_texture_compression_dxt5");

  context->AddCompressedTextureFormat(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
  context->AddCompressedTextureFormat(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
  context->AddCompressedTextureFormat(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
  context->AddCompressedTextureFormat(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
}

WebGLExtensionName WebGLCompressedTextureS3TC::GetName() const {
  return kWebGLCompressedTextureS3TCName;
}

WebGLCompressedTextureS3TC* WebGLCompressedTextureS3TC::Create(
    WebGLRenderingContextBase* context) {
  return MakeGarbageCollected<WebGLCompressedTextureS3TC>(context);
}

bool WebGLCompressedTextureS3TC::Supported(WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensions_util = context->ExtensionsUtil();
  return extensions_util->SupportsExtension(
             "GL_EXT_texture_compression_s3tc") ||
         (extensions_util->SupportsExtension(
              "GL_ANGLE_texture_compression_dxt1") &&
          extensions_util->SupportsExtension(
              "GL_ANGLE_texture_compression_dxt3") &&
          extensions_util->SupportsExtension(
              "GL_ANGLE_texture_compression_dxt5"));
}

const char* WebGLCompressedTextureS3TC::ExtensionName() {
  return "WEBGL_compressed_texture_s3tc";
}

}  // namespace blink
