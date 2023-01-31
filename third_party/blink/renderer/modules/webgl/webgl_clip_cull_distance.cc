// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_clip_cull_distance.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

WebGLClipCullDistance::WebGLClipCullDistance(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_ANGLE_clip_cull_distance");
}

WebGLExtensionName WebGLClipCullDistance::GetName() const {
  return kWebGLClipCullDistanceName;
}

bool WebGLClipCullDistance::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_ANGLE_clip_cull_distance");
}

const char* WebGLClipCullDistance::ExtensionName() {
  return "WEBGL_clip_cull_distance";
}

}  // namespace blink
