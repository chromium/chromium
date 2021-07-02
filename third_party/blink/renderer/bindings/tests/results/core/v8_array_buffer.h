// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_ARRAY_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_ARRAY_BUFFER_H_

#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_array_buffer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

CORE_EXPORT extern const WrapperTypeInfo v8_array_buffer_wrapper_type_info;

class V8ArrayBuffer {
  STATIC_ONLY(V8ArrayBuffer);
 public:
  // Migration adapter
  CORE_EXPORT static bool HasInstance(v8::Isolate*, v8::Local<v8::Value> value) {
    return value->IsArrayBuffer();
  }

  CORE_EXPORT static TestArrayBuffer* ToImpl(v8::Local<v8::Object> object);
  CORE_EXPORT static TestArrayBuffer* ToImplWithTypeCheck(v8::Isolate*, v8::Local<v8::Value>);

  CORE_EXPORT static const WrapperTypeInfo* GetWrapperTypeInfo() {
    return TestArrayBuffer::GetStaticWrapperTypeInfo();
  }

  static constexpr int kInternalFieldCount = kV8DefaultWrapperInternalFieldCount;

  // Callback functions

  CORE_EXPORT static void ByteLengthAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
};

template <>
struct V8TypeOf<TestArrayBuffer> {
  typedef V8ArrayBuffer Type;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_V8_ARRAY_BUFFER_H_
