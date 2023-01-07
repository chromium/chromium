// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_connection_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_connection_event_init.h"
#include "third_party/blink/renderer/modules/hid/hid_device.h"

namespace blink {

HIDConnectionEvent* HIDConnectionEvent::Create(
    const AtomicString& type,
    const HIDConnectionEventInit* initializer) {
  return MakeGarbageCollected<HIDConnectionEvent>(type, initializer);
}

HIDConnectionEvent* HIDConnectionEvent::Create(const AtomicString& type,
                                               HIDDevice* device) {
  return MakeGarbageCollected<HIDConnectionEvent>(type, device);
}

HIDConnectionEvent::HIDConnectionEvent(
    const AtomicString& type,
    const HIDConnectionEventInit* initializer)
    : Event(type, initializer) {}

HIDConnectionEvent::HIDConnectionEvent(const AtomicString& type,
                                       HIDDevice* device)
    : Event(type, Bubbles::kNo, Cancelable::kNo), device_(device) {}

void HIDConnectionEvent::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  Event::Trace(visitor);
}

}  // namespace blink
