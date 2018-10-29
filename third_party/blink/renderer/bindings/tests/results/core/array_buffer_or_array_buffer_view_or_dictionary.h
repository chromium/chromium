// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_ARRAY_BUFFER_OR_ARRAY_BUFFER_VIEW_OR_DICTIONARY_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_ARRAY_BUFFER_OR_ARRAY_BUFFER_VIEW_OR_DICTIONARY_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class TestArrayBuffer;

class CORE_EXPORT ArrayBufferOrArrayBufferViewOrDictionary final {
  DISALLOW_NEW();
 public:
  ArrayBufferOrArrayBufferViewOrDictionary();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsArrayBuffer() const { return type_ == SpecificType::kArrayBuffer; }
  TestArrayBuffer* GetAsArrayBuffer() const;
  void SetArrayBuffer(TestArrayBuffer*);
  static ArrayBufferOrArrayBufferViewOrDictionary FromArrayBuffer(TestArrayBuffer*);

  bool IsArrayBufferView() const { return type_ == SpecificType::kArrayBufferView; }
  NotShared<TestArrayBufferView> GetAsArrayBufferView() const;
  void SetArrayBufferView(NotShared<TestArrayBufferView>);
  static ArrayBufferOrArrayBufferViewOrDictionary FromArrayBufferView(NotShared<TestArrayBufferView>);

  bool IsDictionary() const { return type_ == SpecificType::kDictionary; }
  Dictionary GetAsDictionary() const;
  void SetDictionary(Dictionary);
  static ArrayBufferOrArrayBufferViewOrDictionary FromDictionary(Dictionary);

  ArrayBufferOrArrayBufferViewOrDictionary(const ArrayBufferOrArrayBufferViewOrDictionary&);
  ~ArrayBufferOrArrayBufferViewOrDictionary();
  ArrayBufferOrArrayBufferViewOrDictionary& operator=(const ArrayBufferOrArrayBufferViewOrDictionary&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kArrayBuffer,
    kArrayBufferView,
    kDictionary,
  };
  SpecificType type_;

  Member<TestArrayBuffer> array_buffer_;
  Member<TestArrayBufferView> array_buffer_view_;
  Dictionary dictionary_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const ArrayBufferOrArrayBufferViewOrDictionary&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8ArrayBufferOrArrayBufferViewOrDictionary final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, ArrayBufferOrArrayBufferViewOrDictionary&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const ArrayBufferOrArrayBufferViewOrDictionary&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, ArrayBufferOrArrayBufferViewOrDictionary& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, ArrayBufferOrArrayBufferViewOrDictionary& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<ArrayBufferOrArrayBufferViewOrDictionary> : public NativeValueTraitsBase<ArrayBufferOrArrayBufferViewOrDictionary> {
  CORE_EXPORT static ArrayBufferOrArrayBufferViewOrDictionary NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static ArrayBufferOrArrayBufferViewOrDictionary NullValue() { return ArrayBufferOrArrayBufferViewOrDictionary(); }
};

template <>
struct V8TypeOf<ArrayBufferOrArrayBufferViewOrDictionary> {
  typedef V8ArrayBufferOrArrayBufferViewOrDictionary Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::ArrayBufferOrArrayBufferViewOrDictionary);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_ARRAY_BUFFER_OR_ARRAY_BUFFER_VIEW_OR_DICTIONARY_H_
