// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_SERIALIZED_DATA_FOR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_SERIALIZED_DATA_FOR_EVENT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ExecutionContext;

// This class encapsulates serialized data sent from an Web API running on the
// main thread and received on a DedicatedWorker via an event. It is used to
// implement the options field of RTCRtpScriptTransformer which is itself the
// transformer field of RTCTransformEvent, which is fired by the execution of
// the RTCRtpScriptTransform constructor. Note that this class is only used to
// support data transfer between contexts within the same agent cluster.
class MODULES_EXPORT SerializedDataForEvent
    : public GarbageCollected<SerializedDataForEvent> {
 public:
  explicit SerializedDataForEvent(scoped_refptr<SerializedScriptValue> value);
  ~SerializedDataForEvent();

  ScriptValue Deserialize(ScriptState*);
  ScriptValue Deserialize(
      ScriptState*,
      const SerializedScriptValue::DeserializeOptions& options);

  bool CanDeserializeIn(ExecutionContext* execution_context) const;

  // Never invalidates the cache because data is immutable.
  bool IsDataDirty() { return false; }

  void Trace(Visitor*) const;

 private:
  size_t SizeOfExternalMemoryInBytes();
  void RegisterAmountOfExternallyAllocatedMemory();
  void UnregisterAmountOfExternallyAllocatedMemory();

  Member<UnpackedSerializedScriptValue> data_as_serialized_script_value_;
  size_t amount_of_external_memory_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_SERIALIZED_DATA_FOR_EVENT_H_
