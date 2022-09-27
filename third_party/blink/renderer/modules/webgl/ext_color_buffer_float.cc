// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_color_buffer_float.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTColorBufferFloat::EXTColorBufferFloat(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_color_buffer_float");

  // https://github.com/KhronosGroup/WebGL/pull/2830
  // Spec requires EXT_float_blend to be implicitly turned on here if
  // it's supported.
  context->EnableExtensionIfSupported("EXT_float_blend");
}

WebGLExtensionName EXTColorBufferFloat::GetName() const {
  return kEXTColorBufferFloatName;
}

bool EXTColorBufferFloat::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_EXT_color_buffer_float");
}

const char* EXTColorBufferFloat::ExtensionName() {
  return "EXT_color_buffer_float";
}

}  // namespace blink
