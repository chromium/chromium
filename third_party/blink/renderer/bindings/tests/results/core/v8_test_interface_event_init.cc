// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/dictionary_v8.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_event_init.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

static const v8::Eternal<v8::Name>* eternalV8TestInterfaceEventInitKeys(v8::Isolate* isolate) {
  static const char* const kKeys[] = {
    "stringMember",
  };
  return V8PerIsolateData::From(isolate)->FindOrCreateEternalNameCache(
      kKeys, kKeys, base::size(kKeys));
}

void V8TestInterfaceEventInit::ToImpl(v8::Isolate* isolate, v8::Local<v8::Value> v8Value, TestInterfaceEventInit& impl, ExceptionState& exceptionState) {
  if (IsUndefinedOrNull(v8Value)) {
    return;
  }
  if (!v8Value->IsObject()) {
    exceptionState.ThrowTypeError("cannot convert to dictionary.");
    return;
  }
  v8::Local<v8::Object> v8Object = v8Value.As<v8::Object>();
  ALLOW_UNUSED_LOCAL(v8Object);

  V8EventInit::ToImpl(isolate, v8Value, impl, exceptionState);
  if (exceptionState.HadException())
    return;

  const v8::Eternal<v8::Name>* keys = eternalV8TestInterfaceEventInitKeys(isolate);
  v8::TryCatch block(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> string_member_value;
  if (!v8Object->Get(context, keys[0].Get(isolate)).ToLocal(&string_member_value)) {
    exceptionState.RethrowV8Exception(block.Exception());
    return;
  }
  if (string_member_value.IsEmpty() || string_member_value->IsUndefined()) {
    // Do nothing.
  } else {
    V8StringResource<> string_member_cpp_value = string_member_value;
    if (!string_member_cpp_value.Prepare(exceptionState))
      return;
    impl.setStringMember(string_member_cpp_value);
  }
}

v8::Local<v8::Value> TestInterfaceEventInit::ToV8Impl(v8::Local<v8::Object> creationContext, v8::Isolate* isolate) const {
  v8::Local<v8::Object> v8Object = v8::Object::New(isolate);
  if (!toV8TestInterfaceEventInit(*this, v8Object, creationContext, isolate))
    return v8::Undefined(isolate);
  return v8Object;
}

bool toV8TestInterfaceEventInit(const TestInterfaceEventInit& impl, v8::Local<v8::Object> dictionary, v8::Local<v8::Object> creationContext, v8::Isolate* isolate) {
  if (!toV8EventInit(impl, dictionary, creationContext, isolate))
    return false;

  const v8::Eternal<v8::Name>* keys = eternalV8TestInterfaceEventInitKeys(isolate);
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

  v8::Local<v8::Value> string_member_value;
  bool string_member_has_value_or_default = false;
  if (impl.hasStringMember()) {
    string_member_value = V8String(isolate, impl.stringMember());
    string_member_has_value_or_default = true;
  }
  if (string_member_has_value_or_default &&
      !create_property(0, string_member_value)) {
    return false;
  }

  return true;
}

TestInterfaceEventInit NativeValueTraits<TestInterfaceEventInit>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceEventInit impl;
  V8TestInterfaceEventInit::ToImpl(isolate, value, impl, exceptionState);
  return impl;
}

}  // namespace blink
