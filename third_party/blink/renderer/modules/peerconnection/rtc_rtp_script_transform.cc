// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transform.h"

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "rtc_rtp_script_transform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transform_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// This method runs in the worker context, triggered by a callback.
Event* CreateRTCTransformEvent(ScriptState* script_state,
                               CustomEventMessage data) {
  return MakeGarbageCollected<RTCTransformEvent>(script_state, std::move(data));
}

}  // namespace

RTCRtpScriptTransform* RTCRtpScriptTransform::Create(
    ScriptState* script_state,
    DedicatedWorker* worker,
    ExceptionState& exception_state) {
  HeapVector<ScriptValue> transfer;
  return Create(script_state, worker, ScriptValue(), transfer, exception_state);
}

RTCRtpScriptTransform* RTCRtpScriptTransform::Create(
    ScriptState* script_state,
    DedicatedWorker* worker,
    const ScriptValue& message,
    ExceptionState& exception_state) {
  HeapVector<ScriptValue> transfer;
  return Create(script_state, worker, message, transfer, exception_state);
}

RTCRtpScriptTransform* RTCRtpScriptTransform::Create(
    ScriptState* script_state,
    DedicatedWorker* worker,
    const ScriptValue& message,
    HeapVector<ScriptValue>& transfer,
    ExceptionState& exception_state) {
  worker->PostCustomEvent(TaskType::kInternalMediaRealTime, script_state,
                          CrossThreadBindRepeating(&CreateRTCTransformEvent),
                          CrossThreadFunction<Event*(ScriptState*)>(), message,
                          transfer, exception_state);
  return MakeGarbageCollected<RTCRtpScriptTransform>();
}

}  // namespace blink
