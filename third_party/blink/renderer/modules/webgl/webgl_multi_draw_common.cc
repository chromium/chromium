// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw_common.h"

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
  return true;
}

// static
base::span<const int32_t> WebGLMultiDrawCommon::MakeSpan(
    const Int32ArrayOrLongSequence& array) {
  if (array.IsInt32Array()) {
    return base::span<const int32_t>(array.GetAsInt32Array().View()->Data(),
                                     array.GetAsInt32Array().View()->length());
  }
  return base::span<const int32_t>(array.GetAsLongSequence().data(),
                                   array.GetAsLongSequence().size());
}

// static
base::span<const uint32_t> WebGLMultiDrawCommon::MakeSpan(
    const Uint32ArrayOrUnsignedLongSequence& array) {
  if (array.IsUint32Array()) {
    return base::span<const uint32_t>(
        array.GetAsUint32Array().View()->Data(),
        array.GetAsUint32Array().View()->length());
  }
  return base::span<const uint32_t>(array.GetAsUnsignedLongSequence().data(),
                                    array.GetAsUnsignedLongSequence().size());
}

}  // namespace blink
