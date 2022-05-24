// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_accelerator_event.h"

#include "third_party/blink/renderer/extensions/chromeos/event_chromeos.h"
#include "third_party/blink/renderer/extensions/chromeos/event_type_chromeos_names.h"

namespace blink {

CrosAcceleratorEvent* CrosAcceleratorEvent::Create() {
  return MakeGarbageCollected<CrosAcceleratorEvent>();
}

// TODO(b/221123297): Support both `acceleratordown` and `acceleratorup`.
CrosAcceleratorEvent::CrosAcceleratorEvent()
    : Event(event_type_names::kAcceleratordown,
            Bubbles::kYes,
            Cancelable::kNo) {}

CrosAcceleratorEvent::~CrosAcceleratorEvent() = default;

void CrosAcceleratorEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

const AtomicString& CrosAcceleratorEvent::InterfaceName() const {
  return event_interface_names::kCrosAcceleratorEvent;
}

}  // namespace blink
