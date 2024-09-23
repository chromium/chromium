// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"

#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"

namespace blink {

namespace bindings {

v8::Local<v8::Object> CreatePropertyDescriptorObject(
    v8::Isolate* isolate,
    const v8::PropertyDescriptor& desc) {
  // https://tc39.es/ecma262/#sec-frompropertydescriptor
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
  v8::Local<v8::Object> object = v8::Object::New(isolate);

  auto add_property = [&](const char* name, v8::Local<v8::Value> value) {
    return object->CreateDataProperty(current_context, V8String(isolate, name),
                                      value);
  };
  auto add_property_bool = [&](const char* name, bool value) {
    return add_property(name, value ? v8::True(isolate) : v8::False(isolate));
  };

  bool result;
  if (desc.has_value()) {
    if (!(add_property("value", desc.value()).To(&result) &&
          add_property_bool("writable", desc.writable()).To(&result)))
      return v8::Local<v8::Object>();
  } else {
    if (!(add_property("get", desc.get()).To(&result) &&
          add_property("set", desc.set()).To(&result)))
      return v8::Local<v8::Object>();
  }
  if (!(add_property_bool("enumerable", desc.enumerable()).To(&result) &&
        add_property_bool("configurable", desc.configurable()).To(&result)))
    return v8::Local<v8::Object>();

  return object;
}

v8::Local<v8::Value> GetExposedInterfaceObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const WrapperTypeInfo* wrapper_type_info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      isolate, "Blink_GetInterfaceObjectExposedOnGlobal");
  ScriptState* script_state =
      ScriptState::ForRelevantRealm(isolate, creation_context);
  if (!script_state->ContextIsValid())
    return v8::Undefined(isolate);

  return script_state->PerContextData()->ConstructorForType(wrapper_type_info);
}

v8::Local<v8::Value> GetExposedNamespaceObject(
    v8::Isolate* isolate,
    v8::Local<v8::Object> creation_context,
    const WrapperTypeInfo* wrapper_type_info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      isolate, "Blink_GetInterfaceObjectExposedOnGlobal");
  ScriptState* script_state =
      ScriptState::ForRelevantRealm(isolate, creation_context);
  if (!script_state->ContextIsValid())
    return v8::Undefined(isolate);

  v8::Local<v8::Context> v8_context = script_state->GetContext();
  v8::Context::Scope v8_context_scope(v8_context);
  v8::Local<v8::ObjectTemplate> namespace_template =
      wrapper_type_info->GetV8ClassTemplate(isolate, script_state->World())
          .As<v8::ObjectTemplate>();
  v8::Local<v8::Object> namespace_object =
      namespace_template->NewInstance(v8_context).ToLocalChecked();
  wrapper_type_info->InstallConditionalFeatures(
      v8_context, script_state->World(),
      v8::Local<v8::Object>(),  // instance_object
      v8::Local<v8::Object>(),  // prototype_object
      namespace_object,         // interface_object
      namespace_template);
  return namespace_object;
}

}  // namespace bindings

}  // namespace blink
