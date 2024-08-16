// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/serialized_data_for_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

namespace blink {

SerializedDataForEvent::SerializedDataForEvent(
    scoped_refptr<SerializedScriptValue> value)
    : data_as_serialized_script_value_(
          SerializedScriptValue::Unpack(std::move(value))) {
  RegisterAmountOfExternallyAllocatedMemory();
}

SerializedDataForEvent::~SerializedDataForEvent() {
  UnregisterAmountOfExternallyAllocatedMemory();
}

void SerializedDataForEvent::Trace(Visitor* visitor) const {
  visitor->Trace(data_as_serialized_script_value_);
}

ScriptValue SerializedDataForEvent::Deserialize(ScriptState* script_state) {
  SerializedScriptValue::DeserializeOptions options;
  return Deserialize(script_state, options);
}

ScriptValue SerializedDataForEvent::Deserialize(
    ScriptState* script_state,
    const SerializedScriptValue::DeserializeOptions& options) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> value;
  if (data_as_serialized_script_value_) {
    // The data is put on the V8 GC heap here, and therefore the V8 GC does
    // the accounting from here on. We unregister the registered memory to
    // avoid double accounting.
    UnregisterAmountOfExternallyAllocatedMemory();
    value = data_as_serialized_script_value_->Deserialize(isolate, options);
  } else {
    value = v8::Null(isolate);
  }
  return ScriptValue(isolate, value);
}

size_t SerializedDataForEvent::SizeOfExternalMemoryInBytes() {
  if (!data_as_serialized_script_value_) {
    return 0;
  }
  size_t result = 0;
  for (auto const& array_buffer :
       data_as_serialized_script_value_->ArrayBuffers()) {
    result += array_buffer->ByteLength();
  }
  return result;
}

void SerializedDataForEvent::RegisterAmountOfExternallyAllocatedMemory() {
  CHECK_EQ(amount_of_external_memory_, 0u);

  size_t size = SizeOfExternalMemoryInBytes();
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      static_cast<int64_t>(size));
  amount_of_external_memory_ = size;
}

void SerializedDataForEvent::UnregisterAmountOfExternallyAllocatedMemory() {
  if (amount_of_external_memory_ > 0) {
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int64_t>(amount_of_external_memory_));
    amount_of_external_memory_ = 0;
  }
}

bool SerializedDataForEvent::CanDeserializeIn(
    ExecutionContext* execution_context) const {
  return !data_as_serialized_script_value_ ||
         data_as_serialized_script_value_->Value()->CanDeserializeIn(
             execution_context);
}

}  // namespace blink
