// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/observable_array_base.h"

#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-template.h"

namespace blink {

namespace bindings {

ObservableArrayBase::ObservableArrayBase(
    ScriptWrappable* platform_object,
    ObservableArrayExoticObject* observable_array_exotic_object)
    : platform_object_(platform_object),
      observable_array_exotic_object_(observable_array_exotic_object) {
  DCHECK(platform_object_);
}

v8::Local<v8::Object> ObservableArrayBase::GetProxyHandlerObject(
    ScriptState* script_state) {
  v8::Local<v8::FunctionTemplate> v8_function_template =
      GetProxyHandlerFunctionTemplate(script_state);
  v8::Local<v8::Context> v8_context = script_state->GetContext();
  v8::Local<v8::Function> v8_function =
      v8_function_template->GetFunction(v8_context).ToLocalChecked();
  v8::Local<v8::Object> v8_object =
      v8_function->NewInstance(v8_context).ToLocalChecked();
  CHECK(
      v8_object->SetPrototype(v8_context, v8::Null(script_state->GetIsolate()))
          .ToChecked());
  return v8_object;
}

void ObservableArrayBase::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(platform_object_);
  visitor->Trace(observable_array_exotic_object_);
}

}  // namespace bindings

ObservableArrayExoticObject::ObservableArrayExoticObject(
    bindings::ObservableArrayBase* observable_array_backing_list_object)
    : observable_array_backing_list_object_(
          observable_array_backing_list_object) {}

void ObservableArrayExoticObject::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(observable_array_backing_list_object_);
}

}  // namespace blink
