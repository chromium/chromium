// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_EXT_FLOAT_BLEND_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_EXT_FLOAT_BLEND_H_

#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"

namespace blink {

class EXTFloatBlend final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static EXTFloatBlend* Create(WebGLRenderingContextBase*);
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  explicit EXTFloatBlend(WebGLRenderingContextBase*);

  WebGLExtensionName GetName() const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_EXT_FLOAT_BLEND_H_
