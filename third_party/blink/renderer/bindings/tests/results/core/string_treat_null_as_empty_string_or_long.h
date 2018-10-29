// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/union_container.h.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_STRING_TREAT_NULL_AS_EMPTY_STRING_OR_LONG_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_STRING_TREAT_NULL_AS_EMPTY_STRING_OR_LONG_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class CORE_EXPORT StringTreatNullAsEmptyStringOrLong final {
  DISALLOW_NEW();
 public:
  StringTreatNullAsEmptyStringOrLong();
  bool IsNull() const { return type_ == SpecificType::kNone; }

  bool IsLong() const { return type_ == SpecificType::kLong; }
  int32_t GetAsLong() const;
  void SetLong(int32_t);
  static StringTreatNullAsEmptyStringOrLong FromLong(int32_t);

  bool IsString() const { return type_ == SpecificType::kStringTreatNullAsEmptyString; }
  const String& GetAsString() const;
  void SetString(const String&);
  static StringTreatNullAsEmptyStringOrLong FromString(const String&);

  StringTreatNullAsEmptyStringOrLong(const StringTreatNullAsEmptyStringOrLong&);
  ~StringTreatNullAsEmptyStringOrLong();
  StringTreatNullAsEmptyStringOrLong& operator=(const StringTreatNullAsEmptyStringOrLong&);
  void Trace(blink::Visitor*);

 private:
  enum class SpecificType {
    kNone,
    kLong,
    kStringTreatNullAsEmptyString,
  };
  SpecificType type_;

  int32_t long_;
  String string_treat_null_as_empty_string_;

  friend CORE_EXPORT v8::Local<v8::Value> ToV8(const StringTreatNullAsEmptyStringOrLong&, v8::Local<v8::Object>, v8::Isolate*);
};

class V8StringTreatNullAsEmptyStringOrLong final {
 public:
  CORE_EXPORT static void ToImpl(v8::Isolate*, v8::Local<v8::Value>, StringTreatNullAsEmptyStringOrLong&, UnionTypeConversionMode, ExceptionState&);
};

CORE_EXPORT v8::Local<v8::Value> ToV8(const StringTreatNullAsEmptyStringOrLong&, v8::Local<v8::Object>, v8::Isolate*);

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, StringTreatNullAsEmptyStringOrLong& impl) {
  V8SetReturnValue(callbackInfo, ToV8(impl, callbackInfo.Holder(), callbackInfo.GetIsolate()));
}

template <class CallbackInfo>
inline void V8SetReturnValue(const CallbackInfo& callbackInfo, StringTreatNullAsEmptyStringOrLong& impl, v8::Local<v8::Object> creationContext) {
  V8SetReturnValue(callbackInfo, ToV8(impl, creationContext, callbackInfo.GetIsolate()));
}

template <>
struct NativeValueTraits<StringTreatNullAsEmptyStringOrLong> : public NativeValueTraitsBase<StringTreatNullAsEmptyStringOrLong> {
  CORE_EXPORT static StringTreatNullAsEmptyStringOrLong NativeValue(v8::Isolate*, v8::Local<v8::Value>, ExceptionState&);
  CORE_EXPORT static StringTreatNullAsEmptyStringOrLong NullValue() { return StringTreatNullAsEmptyStringOrLong(); }
};

template <>
struct V8TypeOf<StringTreatNullAsEmptyStringOrLong> {
  typedef V8StringTreatNullAsEmptyStringOrLong Type;
};

}  // namespace blink

// We need to set canInitializeWithMemset=true because HeapVector supports
// items that can initialize with memset or have a vtable. It is safe to
// set canInitializeWithMemset=true for a union type object in practice.
// See https://codereview.chromium.org/1118993002/#msg5 for more details.
WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::StringTreatNullAsEmptyStringOrLong);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_TESTS_RESULTS_CORE_STRING_TREAT_NULL_AS_EMPTY_STRING_OR_LONG_H_
