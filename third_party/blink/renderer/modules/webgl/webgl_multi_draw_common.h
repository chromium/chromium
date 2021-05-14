// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_COMMON_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_COMMON_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/modules/v8/int32_array_or_long_sequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/uint32_array_or_unsigned_long_sequence.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"

namespace blink {

class V8UnionInt32ArrayOrLongSequence;
class V8UnionUint32ArrayOrUnsignedLongSequence;
class WebGLExtensionScopedContext;

class WebGLMultiDrawCommon {
 protected:
  bool ValidateDrawcount(WebGLExtensionScopedContext* scoped,
                         const char* function_name,
                         GLsizei drawcount);

  bool ValidateArray(WebGLExtensionScopedContext* scoped,
                     const char* function_name,
                     const char* outOfBoundsDescription,
                     size_t size,
                     GLuint offset,
                     GLsizei drawcount);

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static base::span<const int32_t> MakeSpan(
      const V8UnionInt32ArrayOrLongSequence* array);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static base::span<const int32_t> MakeSpan(
      const Int32ArrayOrLongSequence& array);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static base::span<const uint32_t> MakeSpan(
      const V8UnionUint32ArrayOrUnsignedLongSequence* array);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  static base::span<const uint32_t> MakeSpan(
      const Uint32ArrayOrUnsignedLongSequence& array);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_MULTI_DRAW_COMMON_H_
