// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_BOOLEAN_OR_ELEMENT_SEQUENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_BOOLEAN_OR_ELEMENT_SEQUENCE_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Element;

class CORE_EXPORT BooleanOrElementSequence final {
  DISALLOW_NEW();
 public:
  BooleanOrElementSequence();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsBoolean() const { return type_ == SpecificType::kBoolean; }
  bool GetAsBoolean() const;
  void SetBoolean(bool);
  static BooleanOrElementSequence FromBoolean(bool);

  bool IsElementSequence() const { return type_ == SpecificType::kElementSequence; }
  const HeapVector<Member<Element>>& GetAsElementSequence() const;
  void SetElementSequence(const HeapVector<Member<Element>>&);
  static BooleanOrElementSequence FromElementSequence(const HeapVector<Member<Element>>&);

  BooleanOrElementSequence(const BooleanOrElementSequence&);
  ~BooleanOrElementSequence();
  BooleanOrElementSequence& operator=(const BooleanOrElementSequence&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kBoolean,
    kElementSequence,
  };
  SpecificType type_;

  bool boolean_;
  HeapVector<Member<Element>> element_sequence_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const BooleanOrElementSequence&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8BooleanOrElementSequence final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, BooleanOrElementSequence&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const BooleanOrElementSequence&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, BooleanOrElementSequence& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, BooleanOrElementSequence& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<BooleanOrElementSequence> : public NativeValueTraitsBase<BooleanOrElementSequence> {
  CORE_EXPORT static BooleanOrElementSequence NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static BooleanOrElementSequence NullValue() { return BooleanOrElementSequence(); }
};

template <>
struct V8TypeOf<BooleanOrElementSequence> {
  typedef V8BooleanOrElementSequence Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::BooleanOrElementSequence);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_BOOLEAN_OR_ELEMENT_SEQUENCE_H_
