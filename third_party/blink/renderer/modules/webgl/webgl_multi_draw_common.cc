// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw_common.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_int32arrayallowshared_longsequence.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_uint32arrayallowshared_unsignedlongsequence.h"
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

// static
base::span<const int32_t> WebGLMultiDrawCommon::MakeSpan(
    const V8UnionInt32ArrayAllowSharedOrLongSequence* array) {
  DCHECK(array);
  switch (array->GetContentType()) {
    case V8UnionInt32ArrayAllowSharedOrLongSequence::ContentType::
        kInt32ArrayAllowShared:
      return base::span<const int32_t>(
          array->GetAsInt32ArrayAllowShared()->DataMaybeShared(),
          array->GetAsInt32ArrayAllowShared()->length());
    case V8UnionInt32ArrayAllowSharedOrLongSequence::ContentType::kLongSequence:
      return base::span<const int32_t>(array->GetAsLongSequence().data(),
                                       array->GetAsLongSequence().size());
  }
  NOTREACHED_IN_MIGRATION();
  return {};
}

// static
base::span<const uint32_t> WebGLMultiDrawCommon::MakeSpan(
    const V8UnionUint32ArrayAllowSharedOrUnsignedLongSequence* array) {
  DCHECK(array);
  switch (array->GetContentType()) {
    case V8UnionUint32ArrayAllowSharedOrUnsignedLongSequence::ContentType::
        kUint32ArrayAllowShared:
      return base::span<const uint32_t>(
          array->GetAsUint32ArrayAllowShared()->DataMaybeShared(),
          array->GetAsUint32ArrayAllowShared()->length());
    case V8UnionUint32ArrayAllowSharedOrUnsignedLongSequence::ContentType::
        kUnsignedLongSequence:
      return base::span<const uint32_t>(
          array->GetAsUnsignedLongSequence().data(),
          array->GetAsUnsignedLongSequence().size());
  }
  NOTREACHED_IN_MIGRATION();
  return {};
}

}  // namespace blink
