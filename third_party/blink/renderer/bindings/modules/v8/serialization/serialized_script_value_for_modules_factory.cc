// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/modules/v8/serialization/serialized_script_value_for_modules_factory.h"

#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_deserializer_for_modules.h"
#include "third_party/blink/renderer/bindings/modules/v8/serialization/v8_script_value_serializer_for_modules.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

bool SerializedScriptValueForModulesFactory::ExtractTransferable(
    v8::Isolate* isolate,
    v8::Local<v8::Value> object,
    wtf_size_t object_index,
    Transferables& transferables,
    ExceptionState& exception_state) {
  return V8ScriptValueSerializerForModules::ExtractTransferable(
      isolate, object, object_index, transferables, exception_state);
}

scoped_refptr<SerializedScriptValue>
SerializedScriptValueForModulesFactory::Create(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const SerializedScriptValue::SerializeOptions& options,
    ExceptionState& exception_state) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::create");
  V8ScriptValueSerializerForModules serializer(
      ScriptState::ForCurrentRealm(isolate), options);
  return serializer.Serialize(value, exception_state);
}

v8::Local<v8::Value> SerializedScriptValueForModulesFactory::Deserialize(
    scoped_refptr<SerializedScriptValue> value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializerForModules deserializer(
      ScriptState::ForCurrentRealm(isolate), std::move(value), options);
  return deserializer.Deserialize();
}

v8::Local<v8::Value> SerializedScriptValueForModulesFactory::Deserialize(
    UnpackedSerializedScriptValue* value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializerForModules deserializer(
      ScriptState::ForCurrentRealm(isolate), value, options);
  return deserializer.Deserialize();
}

bool SerializedScriptValueForModulesFactory::ExecutionContextExposesInterface(
    ExecutionContext* execution_context,
    SerializationTag interface_tag) {
  return V8ScriptValueDeserializerForModules::ExecutionContextExposesInterface(
      execution_context, interface_tag);
}

}  // namespace blink
