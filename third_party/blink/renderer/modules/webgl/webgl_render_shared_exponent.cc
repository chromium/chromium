// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_render_shared_exponent.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLRenderSharedExponent::WebGLRenderSharedExponent(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_QCOM_render_shared_exponent");
}

WebGLExtensionName WebGLRenderSharedExponent::GetName() const {
  return kWebGLRenderSharedExponentName;
}

bool WebGLRenderSharedExponent::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_QCOM_render_shared_exponent");
}

const char* WebGLRenderSharedExponent::ExtensionName() {
  return "WEBGL_render_shared_exponent";
}

}  // namespace blink
