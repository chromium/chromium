// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_clip_control.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

EXTClipControl::EXTClipControl(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_EXT_clip_control");
}

WebGLExtensionName EXTClipControl::GetName() const {
  return kEXTClipControlName;
}

bool EXTClipControl::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension("GL_EXT_clip_control");
}

const char* EXTClipControl::ExtensionName() {
  return "EXT_clip_control";
}

void EXTClipControl::clipControlEXT(GLenum origin, GLenum depth) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    return;
  }
  scoped.Context()->ContextGL()->ClipControlEXT(origin, depth);
}

}  // namespace blink
