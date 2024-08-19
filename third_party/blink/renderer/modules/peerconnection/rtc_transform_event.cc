// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transform_event.h"

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transformer.h"

namespace blink {

RTCTransformEvent::RTCTransformEvent(
    ScriptState* script_state,
    CustomEventMessage data,
    scoped_refptr<base::SequencedTaskRunner> transform_task_runner,
    CrossThreadWeakHandle<RTCRtpScriptTransform> transform)
    : Event(event_type_names::kRtctransform, Bubbles::kNo, Cancelable::kNo),
      transformer_(MakeGarbageCollected<RTCRtpScriptTransformer>(
          script_state,
          std::move(data),
          transform_task_runner,
          std::move(transform))) {}

void RTCTransformEvent::Trace(Visitor* visitor) const {
  visitor->Trace(transformer_);
  Event::Trace(visitor);
}

}  // namespace blink
