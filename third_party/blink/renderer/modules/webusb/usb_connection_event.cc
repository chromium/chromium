// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/usb_connection_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_usb_connection_event_init.h"
#include "third_party/blink/renderer/modules/webusb/usb_device.h"

namespace blink {

USBConnectionEvent* USBConnectionEvent::Create(
    const AtomicString& type,
    const USBConnectionEventInit* initializer) {
  return MakeGarbageCollected<USBConnectionEvent>(type, initializer);
}

USBConnectionEvent* USBConnectionEvent::Create(const AtomicString& type,
                                               USBDevice* device) {
  return MakeGarbageCollected<USBConnectionEvent>(type, device);
}

USBConnectionEvent::USBConnectionEvent(
    const AtomicString& type,
    const USBConnectionEventInit* initializer)
    : Event(type, initializer), device_(initializer->device()) {}

USBConnectionEvent::USBConnectionEvent(const AtomicString& type,
                                       USBDevice* device)
    : Event(type, Bubbles::kNo, Cancelable::kNo), device_(device) {}

void USBConnectionEvent::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  Event::Trace(visitor);
}

}  // namespace blink
