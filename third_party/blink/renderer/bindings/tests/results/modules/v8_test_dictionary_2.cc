// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_v8.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/modules/v8_test_dictionary_2.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_dictionary.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

static const v8::Eternal<v8::Name>* eternalV8TestDictionary2Keys(v8::Isolate* isolate) {
  static const char* const kKeys[] = {
    "defaultEmptyDictionary",
    "defaultEmptyDictionaryForUnion",
  };
  return V8PerIsolateData::From(isolate)->FindOrCreateEternalNameCache(
      kKeys, kKeys, base::size(kKeys));
}

void V8TestDictionary2::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8_value, TestDictionary2* impl, ExceptionState& exception_state) {
  if (IsUndefinedOrNull(v8_value)) {
    return;
  }
  if (!v8_value->IsObject()) {
    exception_state.ThrowTypeError("cannot convert to dictionary.");
    return;
  }
  v8::Local<v8::Object> v8Object = v8_value.As<v8::Object>();
  ALLOW_UNUSED_LOCAL(v8Object);

  const v8::Eternal<v8::Name>* keys = eternalV8TestDictionary2Keys(isolate);
  v8::TryCatch block(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> default_empty_dictionary_value;
  if (!v8Object->Get(context, keys[0].Get(isolate)).ToLocal(&default_empty_dictionary_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (default_empty_dictionary_value.IsEmpty() || default_empty_dictionary_value->IsUndefined()) {
    // Do nothing.
  } else {
    TestDictionary* default_empty_dictionary_cpp_value = NativeValueTraits<TestDictionary>::NativeValue(isolate, default_empty_dictionary_value, exception_state);
    if (exception_state.HadException())
      return;
    impl->setDefaultEmptyDictionary(default_empty_dictionary_cpp_value);
  }

  v8::Local<v8::Value> default_empty_dictionary_for_union_value;
  if (!v8Object->Get(context, keys[1].Get(isolate)).ToLocal(&default_empty_dictionary_for_union_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }
  if (default_empty_dictionary_for_union_value.IsEmpty() || default_empty_dictionary_for_union_value->IsUndefined()) {
    // Do nothing.
  } else {
    TestDictionaryOrLong default_empty_dictionary_for_union_cpp_value;
    V8TestDictionaryOrLong::ToImpl(isolate, default_empty_dictionary_for_union_value, default_empty_dictionary_for_union_cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;
    impl->setDefaultEmptyDictionaryForUnion(default_empty_dictionary_for_union_cpp_value);
  }
}

v8::Local<v8::Value> TestDictionary2::ToV8Impl(v8::Local<v8::Object> creationContext, v8::Isolate* isolate) const {
  v8::Local<v8::Object> v8Object = v8::Object::New(isolate);
  if (!toV8TestDictionary2(this, v8Object, creationContext, isolate))
    return v8::Undefined(isolate);
  return v8Object;
}

bool toV8TestDictionary2(const TestDictionary2* impl, v8::Local<v8::Object> dictionary, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  const v8::Eternal<v8::Name>* keys = eternalV8TestDictionary2Keys(isolate);
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

  v8::Local<v8::Value> default_empty_dictionary_value;
  bool default_empty_dictionary_has_value_or_default = false;
  if (impl->hasDefaultEmptyDictionary()) {
    default_empty_dictionary_value = ToV8(impl->defaultEmptyDictionary(), creationContext, isolate);
    default_empty_dictionary_has_value_or_default = true;
  } else {
    default_empty_dictionary_value = ToV8(MakeGarbageCollected<TestDictionary>(), creationContext, isolate);
    default_empty_dictionary_has_value_or_default = true;
  }
  if (default_empty_dictionary_has_value_or_default &&
      !create_property(0, default_empty_dictionary_value)) {
    return false;
  }

  v8::Local<v8::Value> default_empty_dictionary_for_union_value;
  bool default_empty_dictionary_for_union_has_value_or_default = false;
  if (impl->hasDefaultEmptyDictionaryForUnion()) {
    default_empty_dictionary_for_union_value = ToV8(impl->defaultEmptyDictionaryForUnion(), creationContext, isolate);
    default_empty_dictionary_for_union_has_value_or_default = true;
  } else {
    default_empty_dictionary_for_union_value = ToV8(TestDictionaryOrLong::FromTestDictionary(MakeGarbageCollected<TestDictionary>()), creationContext, isolate);
    default_empty_dictionary_for_union_has_value_or_default = true;
  }
  if (default_empty_dictionary_for_union_has_value_or_default &&
      !create_property(1, default_empty_dictionary_for_union_value)) {
    return false;
  }

  return true;
}

TestDictionary2* NativeValueTraits<TestDictionary2>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestDictionary2* impl = TestDictionary2::Create();
  V8TestDictionary2::ToImpl(isolate, value, impl, exception_state);
  return impl;
}

}  // namespace blink
