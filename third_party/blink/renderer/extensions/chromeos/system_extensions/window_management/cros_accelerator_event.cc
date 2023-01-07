// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_accelerator_event.h"

#include "third_party/blink/renderer/bindings/extensions_chromeos/v8/v8_cros_accelerator_event_init.h"
#include "third_party/blink/renderer/extensions/chromeos/event_interface_chromeos_names.h"

namespace blink {

CrosAcceleratorEvent* CrosAcceleratorEvent::Create(
    const AtomicString& type,
    const CrosAcceleratorEventInit* event_init) {
  return MakeGarbageCollected<CrosAcceleratorEvent>(type, event_init);
}

CrosAcceleratorEvent::CrosAcceleratorEvent(
    const AtomicString& type,
    const CrosAcceleratorEventInit* event_init)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      accelerator_name_(event_init->acceleratorName()),
      repeat_(event_init->repeat()) {}

CrosAcceleratorEvent::~CrosAcceleratorEvent() = default;

void CrosAcceleratorEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

const AtomicString& CrosAcceleratorEvent::InterfaceName() const {
  return event_interface_names::kCrosAcceleratorEvent;
}

}  // namespace blink
