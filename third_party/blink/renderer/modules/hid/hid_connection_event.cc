// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_connection_event.h"

#include "third_party/blink/renderer/modules/hid/hid_connection_event_init.h"
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
    : Event(type, Bubbles::kNo, Cancelable::kNo) {}

void HIDConnectionEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
}

}  // namespace blink
