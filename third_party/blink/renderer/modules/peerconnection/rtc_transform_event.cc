// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transform_event.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transformer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

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
