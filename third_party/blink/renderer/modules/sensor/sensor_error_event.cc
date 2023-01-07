// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_error_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "v8/include/v8.h"

namespace blink {

SensorErrorEvent::~SensorErrorEvent() = default;

SensorErrorEvent::SensorErrorEvent(const AtomicString& event_type,
                                   DOMException* error)
    : Event(event_type, Bubbles::kNo, Cancelable::kNo), error_(error) {
  DCHECK(error_);
}

SensorErrorEvent::SensorErrorEvent(const AtomicString& event_type,
                                   const SensorErrorEventInit* initializer)
    : Event(event_type, initializer), error_(initializer->error()) {
  DCHECK(error_);
}

const AtomicString& SensorErrorEvent::InterfaceName() const {
  return event_interface_names::kSensorErrorEvent;
}

void SensorErrorEvent::Trace(Visitor* visitor) const {
  visitor->Trace(error_);
  Event::Trace(visitor);
}

}  // namespace blink
