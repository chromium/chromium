// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/multi_worlds_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_data_store.h"

namespace blink {

MultiWorldsV8Reference::MultiWorldsV8Reference(v8::Isolate* isolate,
                                               v8::Local<v8::Value> value)
    : value_(isolate, value) {
  if (value->IsObject()) {
    script_state_ =
        ScriptState::ForRelevantRealm(isolate, value.As<v8::Object>());
  } else {
    script_state_ = nullptr;
  }
}

v8::Local<v8::Value> MultiWorldsV8Reference::GetValue(
    ScriptState* script_state) {
  v8::Local<v8::Value> value = value_.Get(script_state->GetIsolate());
  if (value->IsObject()) {
    return GetObject(script_state);
  } else {
    return value;
  }
}

void MultiWorldsV8Reference::Trace(Visitor* visitor) const {
  visitor->Trace(value_);
  visitor->Trace(script_state_);
}

v8::Local<v8::Object> MultiWorldsV8Reference::GetObject(
    ScriptState* script_state) {
  if (&script_state->World() == &script_state_->World()) {
    return value_.Get(script_state->GetIsolate()).As<v8::Object>();
  }

  V8ObjectDataStore& map = script_state->World().GetV8ObjectDataStore();
  v8::Local<v8::Object> obj = map.Get(script_state_->GetIsolate(), this);
  if (!obj.IsEmpty()) {
    return obj;
  }

  // TODO(nonoohara): We will create an object that is a clone of object_
  // and put it in copy_object.
  NOTIMPLEMENTED();
  v8::Local<v8::Object>
      copy_object;  // Suppose it contains a copy of the object.
  map.Set(script_state_->GetIsolate(), this, copy_object);
  return copy_object;
}

}  // namespace blink
