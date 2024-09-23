// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_deserializer.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_serializer.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

SerializedScriptValueFactory* SerializedScriptValueFactory::instance_ = nullptr;

bool SerializedScriptValueFactory::ExtractTransferable(
    v8::Isolate* isolate,
    v8::Local<v8::Value> object,
    wtf_size_t object_index,
    Transferables& transferables,
    ExceptionState& exception_state) {
  return V8ScriptValueSerializer::ExtractTransferable(
      isolate, object, object_index, transferables, exception_state);
}

scoped_refptr<SerializedScriptValue> SerializedScriptValueFactory::Create(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    const SerializedScriptValue::SerializeOptions& options,
    ExceptionState& exception_state) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::create");
  V8ScriptValueSerializer serializer(ScriptState::ForCurrentRealm(isolate),
                                     options);
  return serializer.Serialize(value, exception_state);
}

v8::Local<v8::Value> SerializedScriptValueFactory::Deserialize(
    scoped_refptr<SerializedScriptValue> value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializer deserializer(ScriptState::ForCurrentRealm(isolate),
                                         std::move(value), options);
  return deserializer.Deserialize();
}

v8::Local<v8::Value> SerializedScriptValueFactory::Deserialize(
    UnpackedSerializedScriptValue* value,
    v8::Isolate* isolate,
    const SerializedScriptValue::DeserializeOptions& options) {
  TRACE_EVENT0("blink", "SerializedScriptValueFactory::deserialize");
  V8ScriptValueDeserializer deserializer(ScriptState::ForCurrentRealm(isolate),
                                         value, options);
  return deserializer.Deserialize();
}

bool SerializedScriptValueFactory::ExecutionContextExposesInterface(
    ExecutionContext* execution_context,
    SerializationTag interface_tag) {
  return V8ScriptValueDeserializer::ExecutionContextExposesInterface(
      execution_context, interface_tag);
}

}  // namespace blink
