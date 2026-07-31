// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_connection_event.h"

#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"

namespace blink {

SerialConnectionEvent::SerialConnectionEvent(const AtomicString& type,
                                             const DOMWrapperWorld* world)
    // Note: Bubbles::kYes is required here because in Web Serial, connection
    // and disconnection events are dispatched directly onto the originating
    // SerialPort instances. They must bubble up to the frame-level Serial
    // (navigator.serial) parent object to support global event listeners.
    : Event(type, Bubbles::kYes, Cancelable::kNo), world_(world) {}

bool SerialConnectionEvent::CanBeDispatchedInWorld(
    const DOMWrapperWorld& world) const {
  return !world_ || &world == world_.Get();
}

void SerialConnectionEvent::Trace(Visitor* visitor) const {
  visitor->Trace(world_);
  Event::Trace(visitor);
}

}  // namespace blink
