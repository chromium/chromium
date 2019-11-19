// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"

namespace blink {

V8ObjectBuilder::V8ObjectBuilder(ScriptState* script_state)
    : script_state_(script_state),
      object_(v8::Object::New(script_state->GetIsolate())) {}

V8ObjectBuilder& V8ObjectBuilder::Add(const StringView& name,
                                      const V8ObjectBuilder& value) {
  AddInternal(name, value.V8Value());
  return *this;
}

V8ObjectBuilder& V8ObjectBuilder::AddNull(const StringView& name) {
  AddInternal(name, v8::Null(script_state_->GetIsolate()));
  return *this;
}

V8ObjectBuilder& V8ObjectBuilder::AddBoolean(const StringView& name,
                                             bool value) {
  AddInternal(name, value ? v8::True(script_state_->GetIsolate())
                          : v8::False(script_state_->GetIsolate()));
  return *this;
}

V8ObjectBuilder& V8ObjectBuilder::AddNumber(const StringView& name,
                                            double value) {
  AddInternal(name, v8::Number::New(script_state_->GetIsolate(), value));
  return *this;
}

V8ObjectBuilder& V8ObjectBuilder::AddString(const StringView& name,
                                            const StringView& value) {
  AddInternal(name, V8String(script_state_->GetIsolate(), value));
  return *this;
}

V8ObjectBuilder& V8ObjectBuilder::AddStringOrNull(const StringView& name,
                                                  const StringView& value) {
  if (value.IsNull()) {
    AddInternal(name, v8::Null(script_state_->GetIsolate()));
  } else {
    AddInternal(name, V8String(script_state_->GetIsolate(), value));
  }
  return *this;
}

ScriptValue V8ObjectBuilder::GetScriptValue() const {
  return ScriptValue(script_state_->GetIsolate(), object_);
}

void V8ObjectBuilder::AddInternal(const StringView& name,
                                  v8::Local<v8::Value> value) {
  if (object_.IsEmpty())
    return;
  if (value.IsEmpty() ||
      object_
          ->CreateDataProperty(
              script_state_->GetContext(),
              V8AtomicString(script_state_->GetIsolate(), name), value)
          .IsNothing())
    object_.Clear();
}

}  // namespace blink
