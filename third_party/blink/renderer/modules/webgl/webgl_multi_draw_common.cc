// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw_common.h"

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

bool WebGLMultiDrawCommon::ValidateDrawcount(
    WebGLExtensionScopedContext* scoped,
    const char* function_name,
    GLsizei drawcount) {
  if (drawcount < 0) {
    scoped->Context()->SynthesizeGLError(GL_INVALID_VALUE, function_name,
                                         "negative drawcount");
    return false;
  }

  constexpr size_t kMaxIntsReadPerDrawCall =
      WebGLRenderingContextBase::kMaximumSupportedArrayBufferSize /
      sizeof(int32_t);

  if (static_cast<size_t>(drawcount) > kMaxIntsReadPerDrawCall) {
    scoped->Context()->SynthesizeGLError(
        GL_INVALID_VALUE, function_name,
        "data touched by drawcount exceeds maximum ArrayBuffer size");
    return false;
  }
  return true;
}

bool WebGLMultiDrawCommon::ValidateArray(WebGLExtensionScopedContext* scoped,
                                         const char* function_name,
                                         const char* outOfBoundsDescription,
                                         size_t size,
                                         GLuint offset,
                                         GLsizei drawcount) {
  if (static_cast<uint32_t>(drawcount) > size) {
    scoped->Context()->SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                                         "drawcount out of bounds");
    return false;
  }
  if (offset >= size) {
    scoped->Context()->SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                                         outOfBoundsDescription);
    return false;
  }
  if (static_cast<uint64_t>(drawcount) + offset > size) {
    scoped->Context()->SynthesizeGLError(GL_INVALID_OPERATION, function_name,
                                         "drawcount plus offset out of bounds");
    return false;
  }
  return true;
}

}  // namespace blink
