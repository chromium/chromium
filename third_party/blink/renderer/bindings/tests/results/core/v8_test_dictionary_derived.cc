// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_v8.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_dictionary_derived.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_dictionary.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

static const v8::Eternal<v8::Name>* eternalV8TestDictionaryDerivedImplementedAsKeys(v8::Isolate* isolate) {
  static const char* const kKeys[] = {
    "derivedStringMember",
    "derivedStringMemberWithDefault",
    "requiredLongMember",
    "stringOrDoubleSequenceMember",
  };
  return V8PerIsolateData::From(isolate)->FindOrCreateEternalNameCache(
      kKeys, kKeys, base::size(kKeys));
}

void V8TestDictionaryDerivedImplementedAs::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, TestDictionaryDerivedImplementedAs& impl, ExceptionState& exceptionState) {
  if (IsUndefinedOrNull(v8Value)) {
    exceptionState.ThrowTypeError("Missing required member(s): requiredLongMember.");
    return;
  }
  if (!v8Value->IsObject()) {
    exceptionState.ThrowTypeError("cannot convert to dictionary.");
    return;
  }
  v8::Local<v8::Object> v8Object = v8Value.As<v8::Object>();
  ALLOW_UNUSED_LOCAL(v8Object);

  V8TestDictionary::ToImpl(isolate, v8Value, impl, exceptionState);
  if (exceptionState.HadException())
    return;

  const v8::Eternal<v8::Name>* keys = eternalV8TestDictionaryDerivedImplementedAsKeys(isolate);
  v8::TryCatch block(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> derived_string_member_value;
  if (!v8Object->Get(context, keys[0].Get(isolate)).ToLocal(&derived_string_member_value)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (derived_string_member_value.IsEmpty() || derived_string_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<> derived_string_member_cpp_value = derived_string_member_value;
    if (!derived_string_member_cpp_value.Prepare(exceptionState))
      return;
    impl.setDerivedStringMember(derived_string_member_cpp_value);
  }

  v8::Local<v8::Value> derived_string_member_with_default_value;
  if (!v8Object->Get(context, keys[1].Get(isolate)).ToLocal(&derived_string_member_with_default_value)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (derived_string_member_with_default_value.IsEmpty() || derived_string_member_with_default_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<> derived_string_member_with_default_cpp_value = derived_string_member_with_default_value;
    if (!derived_string_member_with_default_cpp_value.Prepare(exceptionState))
      return;
    impl.setDerivedStringMemberWithDefault(derived_string_member_with_default_cpp_value);
  }

  v8::Local<v8::Value> required_long_member_value;
  if (!v8Object->Get(context, keys[2].Get(isolate)).ToLocal(&required_long_member_value)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (required_long_member_value.IsEmpty() || required_long_member_value->IsUndefined()) {
    exceptionState.ThrowTypeError("required member requiredLongMember is undefined.");
    return;
  } else {
    int32_t required_long_member_cpp_value = NativeValueTraits<IDLLong>::NativeValue(isolate, required_long_member_value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setRequiredLongMember(required_long_member_cpp_value);
  }

  v8::Local<v8::Value> string_or_double_sequence_member_value;
  if (!v8Object->Get(context, keys[3].Get(isolate)).ToLocal(&string_or_double_sequence_member_value)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (string_or_double_sequence_member_value.IsEmpty() || string_or_double_sequence_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    HeapVector<StringOrDouble> string_or_double_sequence_member_cpp_value = NativeValueTraits<IDLSequence<StringOrDouble>>::NativeValue(isolate, string_or_double_sequence_member_value, exceptionState);
    if (exceptionState.HadException())
      return;
    impl.setStringOrDoubleSequenceMember(string_or_double_sequence_member_cpp_value);
  }
}

v8::Local<v8::Value> TestDictionaryDerivedImplementedAs::ToV8Impl(v8::Local<v8::Object> creationContext, v8::Isolate* isolate) const {
  v8::Local<v8::Object> v8Object = v8::Object::New(isolate);
  if (!toV8TestDictionaryDerivedImplementedAs(*this, v8Object, creationContext, isolate))
    return v8::Undefined(isolate);
  return v8Object;
}

bool toV8TestDictionaryDerivedImplementedAs(const TestDictionaryDerivedImplementedAs& impl, v8::Local<v8::Object> dictionary, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  if (!toV8TestDictionary(impl, dictionary, creationContext, isolate))
    return false;

  const v8::Eternal<v8::Name>* keys = eternalV8TestDictionaryDerivedImplementedAsKeys(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  auto create_property = [dictionary, context, keys, isolate](
                             size_t key_index, v8::Local<v8::Value> value) {
    bool added_property;
    v8::Local<v8::Name> key = keys[key_index].Get(isolate);
    if (!dictionary->CreateDataProperty(context, key, value)
             .To(&added_property)) {
      return false;
    }
    return added_property;
  };

  v8::Local<v8::Value> derived_string_member_value;
  bool derived_string_member_has_value_or_default = false;
  if (impl.hasDerivedStringMember()) {
    derived_string_member_value = V8String(isolate, impl.derivedStringMember());
    derived_string_member_has_value_or_default = true;
  }
  if (derived_string_member_has_value_or_default &&
      !create_property(0, derived_string_member_value)) {
    return false;
  }

  v8::Local<v8::Value> derived_string_member_with_default_value;
  bool derived_string_member_with_default_has_value_or_default = false;
  if (impl.hasDerivedStringMemberWithDefault()) {
    derived_string_member_with_default_value = V8String(isolate, impl.derivedStringMemberWithDefault());
    derived_string_member_with_default_has_value_or_default = true;
  } else {
    derived_string_member_with_default_value = V8String(isolate, "default string value");
    derived_string_member_with_default_has_value_or_default = true;
  }
  if (derived_string_member_with_default_has_value_or_default &&
      !create_property(1, derived_string_member_with_default_value)) {
    return false;
  }

  v8::Local<v8::Value> required_long_member_value;
  bool required_long_member_has_value_or_default = false;
  if (impl.hasRequiredLongMember()) {
    required_long_member_value = v8::Integer::New(isolate, impl.requiredLongMember());
    required_long_member_has_value_or_default = true;
  } else {
    NOTREACHED();
  }
  if (required_long_member_has_value_or_default &&
      !create_property(2, required_long_member_value)) {
    return false;
  }

  v8::Local<v8::Value> string_or_double_sequence_member_value;
  bool string_or_double_sequence_member_has_value_or_default = false;
  if (impl.hasStringOrDoubleSequenceMember()) {
    string_or_double_sequence_member_value = ToV8(impl.stringOrDoubleSequenceMember(), creationContext, isolate);
    string_or_double_sequence_member_has_value_or_default = true;
  }
  if (string_or_double_sequence_member_has_value_or_default &&
      !create_property(3, string_or_double_sequence_member_value)) {
    return false;
  }

  return true;
}

TestDictionaryDerivedImplementedAs NativeValueTraits<TestDictionaryDerivedImplementedAs>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestDictionaryDerivedImplementedAs impl;
  V8TestDictionaryDerivedImplementedAs::ToImpl(isolate, value, impl, exceptionState);
  return impl;
}

}  // namespace blink
