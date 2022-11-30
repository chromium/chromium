// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/custom_wrappable_adapter.h"

#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

namespace blink {

namespace {

void InstallCustomWrappableTemplate(v8::Isolate* isolate,
                                    const DOMWrapperWorld& world,
                                    v8::Local<v8::Template> interface_template);

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

void InstallCustomWrappableTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> interface_template) {
  v8::Local<v8::FunctionTemplate> interface_function_template =
      interface_template.As<v8::FunctionTemplate>();
  v8::Local<v8::ObjectTemplate> instance_object_template =
      interface_function_template->InstanceTemplate();
  v8::Local<v8::ObjectTemplate> prototype_object_template =
      interface_function_template->PrototypeTemplate();
  v8::Local<v8::FunctionTemplate> parent_interface_template;
  bindings::SetupIDLInterfaceTemplate(
      isolate, &custom_wrappable_info, instance_object_template,
      prototype_object_template, interface_function_template,
      parent_interface_template);
}

}  // namespace

CustomWrappableAdapter* CustomWrappableAdapter::LookupInternal(
    v8::Local<v8::Object> object,
    const V8PrivateProperty::Symbol& property) {
  v8::Local<v8::Value> custom_wrappable_adapter_value;
  if (!property.GetOrUndefined(object).ToLocal(&custom_wrappable_adapter_value))
    return nullptr;

  if (!custom_wrappable_adapter_value->IsUndefined()) {
    return static_cast<CustomWrappableAdapter*>(
        ToCustomWrappable(custom_wrappable_adapter_value.As<v8::Object>()));
  }
  return nullptr;
}

void CustomWrappableAdapter::Attach(ScriptState* script_state,
                                    v8::Local<v8::Object> object,
                                    const V8PrivateProperty::Symbol& property) {
  v8::Local<v8::Object> wrapper_object =
      CreateAndInitializeWrapper(script_state);
  property.Set(object, wrapper_object);
}

v8::Local<v8::Object> CustomWrappableAdapter::CreateAndInitializeWrapper(
    ScriptState* script_state) {
  DCHECK(wrapper_.IsEmpty());
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

}  // namespace blink
