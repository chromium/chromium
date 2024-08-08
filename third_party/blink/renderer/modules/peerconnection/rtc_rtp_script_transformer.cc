// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transformer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/serialized_data_for_event.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

RTCRtpScriptTransformer::RTCRtpScriptTransformer(ScriptState* script_state,
                                                 CustomEventMessage options)
    : serialized_data_(MakeGarbageCollected<SerializedDataForEvent>(
          std::move(options.message))),
      ports_(MessagePort::EntanglePorts(*ExecutionContext::From(script_state),
                                        std::move(options.ports))) {}

void RTCRtpScriptTransformer::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(serialized_data_);
  visitor->Trace(ports_);
}

//  Relies on [CachedAttribute] to ensure it isn't run more than once.
ScriptValue RTCRtpScriptTransformer::options(ScriptState* script_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MessagePortArray message_ports = ports_ ? *ports_ : MessagePortArray();
  SerializedScriptValue::DeserializeOptions options;
  options.message_ports = &message_ports;
  return serialized_data_->Deserialize(script_state, options);
}

bool RTCRtpScriptTransformer::IsOptionsDirty() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return serialized_data_->IsDataDirty();
}

}  // namespace blink
