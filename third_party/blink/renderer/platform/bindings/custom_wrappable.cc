// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/custom_wrappable.h"

#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

namespace blink {

namespace {

void InstallCustomWrappableTemplate(
    v8::Isolate*,
    const DOMWrapperWorld&,
    v8::Local<v8::Template> interface_template) {
  v8::Local<v8::ObjectTemplate> instance_template =
      interface_template.As<v8::FunctionTemplate>()->InstanceTemplate();
  instance_template->SetInternalFieldCount(kV8DefaultWrapperInternalFieldCount);
}

const WrapperTypeInfo custom_wrappable_info = {
    gin::kEmbedderBlink,
    InstallCustomWrappableTemplate,
    nullptr,
    "CustomWrappableAdapter",
    nullptr,
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    WrapperTypeInfo::kCustomWrappableId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kCustomWrappableKind,
};

}  // namespace

v8::Local<v8::Object> CustomWrappable::Wrap(ScriptState* script_state) {
  CHECK(wrapper_.IsEmpty());
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Object> wrapper_object =
      V8DOMWrapper::CreateWrapper(script_state, &custom_wrappable_info)
          .ToLocalChecked();
  V8DOMWrapper::AssociateObjectWithWrapper(
      isolate, this, &custom_wrappable_info, wrapper_object);
  wrapper_.Reset(isolate, wrapper_object);
  custom_wrappable_info.ConfigureWrapper(&wrapper_);
  return wrapper_object;
}

void CustomWrappable::Trace(Visitor* visitor) const {
  visitor->Trace(wrapper_);
}

}  // namespace blink
