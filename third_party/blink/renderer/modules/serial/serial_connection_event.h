// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_CONNECTION_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_CONNECTION_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class DOMWrapperWorld;

// Represents a connection or disconnection event for WebSerial.
// This class is used internally in C++ to restrict the event dispatch
// to the specific V8 world it was created for, preventing duplicate events
// and cross-world leaks (e.g. into extensions). In JavaScript, this event
// is exposed as a plain `Event` object to match the Web Serial specification
// (since Chrome 89 removed the `SerialConnectionEvent` interface).
class MODULES_EXPORT SerialConnectionEvent final : public Event {
 public:
  SerialConnectionEvent(const AtomicString& type, const DOMWrapperWorld* world);

  // Returns true if the event is allowed to be dispatched in the given `world`.
  // Restricts dispatch to the world the event was created in, unless `world_`
  // is null.
  bool CanBeDispatchedInWorld(const DOMWrapperWorld& world) const override;
  void Trace(Visitor*) const override;

 private:
  // The V8 world in which this event's port wrapper was created.
  // Used to restrict the event dispatch to this world only.
  // When `WebSerialWorldIsolatedCache` is disabled, this is `nullptr`,
  // in which case `CanBeDispatchedInWorld` will always return true (legacy
  // behavior).
  Member<const DOMWrapperWorld> world_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_CONNECTION_EVENT_H_
