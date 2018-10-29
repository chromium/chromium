// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_STRING_OR_STRING_SEQUENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_STRING_OR_STRING_SEQUENCE_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CORE_EXPORT StringOrStringSequence final {
  DISALLOW_NEW();
 public:
  StringOrStringSequence();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsString() const { return type_ == SpecificType::kString; }
  const String& GetAsString() const;
  void SetString(const String&);
  static StringOrStringSequence FromString(const String&);

  bool IsStringSequence() const { return type_ == SpecificType::kStringSequence; }
  const Vector<String>& GetAsStringSequence() const;
  void SetStringSequence(const Vector<String>&);
  static StringOrStringSequence FromStringSequence(const Vector<String>&);

  StringOrStringSequence(const StringOrStringSequence&);
  ~StringOrStringSequence();
  StringOrStringSequence& operator=(const StringOrStringSequence&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kString,
    kStringSequence,
  };
  SpecificType type_;

  String string_;
  Vector<String> string_sequence_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const StringOrStringSequence&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8StringOrStringSequence final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, StringOrStringSequence&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const StringOrStringSequence&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, StringOrStringSequence& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, StringOrStringSequence& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<StringOrStringSequence> : public NativeValueTraitsBase<StringOrStringSequence> {
  CORE_EXPORT static StringOrStringSequence NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static StringOrStringSequence NullValue() { return StringOrStringSequence(); }
};

template <>
struct V8TypeOf<StringOrStringSequence> {
  typedef V8StringOrStringSequence Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::StringOrStringSequence);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_STRING_OR_STRING_SEQUENCE_H_
