// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_error_event.h"

#include "base/check.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_error_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/webrtc/api/rtc_error.h"

namespace blink {

// static
RTCErrorEvent* RTCErrorEvent::Create(const AtomicString& type,
                                     const RTCErrorEventInit* event_init_dict) {
  return MakeGarbageCollected<RTCErrorEvent>(type, event_init_dict);
}

RTCErrorEvent::RTCErrorEvent(const AtomicString& type,
                             const RTCErrorEventInit* event_init_dict)
    : Event(type, event_init_dict), error_(event_init_dict->error()) {
  DCHECK(event_init_dict);
}

RTCErrorEvent::RTCErrorEvent(const AtomicString& type, webrtc::RTCError error)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      error_(MakeGarbageCollected<RTCError>(error)) {}

RTCError* RTCErrorEvent::error() const {
  return error_.Get();
}

void RTCErrorEvent::Trace(Visitor* visitor) const {
  visitor->Trace(error_);
  Event::Trace(visitor);
}

}  // namespace blink
