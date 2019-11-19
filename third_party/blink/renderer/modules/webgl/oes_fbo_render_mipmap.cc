// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/oes_fbo_render_mipmap.h"

namespace blink {

OESFboRenderMipmap::OESFboRenderMipmap(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_OES_fbo_render_mipmap");
}

WebGLExtensionName OESFboRenderMipmap::GetName() const {
  return kOESFboRenderMipmapName;
}

OESFboRenderMipmap* OESFboRenderMipmap::Create(
    WebGLRenderingContextBase* context) {
  return MakeGarbageCollected<OESFboRenderMipmap>(context);
}

bool OESFboRenderMipmap::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_OES_fbo_render_mipmap");
}

const char* OESFboRenderMipmap::ExtensionName() {
  return "OES_fbo_render_mipmap";
}

}  // namespace blink
