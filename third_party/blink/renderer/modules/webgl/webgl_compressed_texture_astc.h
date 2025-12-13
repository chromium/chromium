// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_COMPRESSED_TEXTURE_ASTC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_COMPRESSED_TEXTURE_ASTC_H_

#include <array>

#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExecutionContext;

class WebGLCompressedTextureASTC final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

  bool supports_hdr;

 public:
  typedef struct {
    int compress_type;
    int block_width;
    int block_height;
  } BlockSizeCompressASTC;

  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  WebGLCompressedTextureASTC(WebGLRenderingContextBase*, ExecutionContext*);

  WebGLExtensionName GetName() const override;
  static const std::array<BlockSizeCompressASTC, 14> kBlockSizeCompressASTC;

  Vector<String> getSupportedProfiles();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_COMPRESSED_TEXTURE_ASTC_H_
