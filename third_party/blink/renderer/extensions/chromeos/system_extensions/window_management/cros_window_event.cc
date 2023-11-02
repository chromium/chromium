// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_event.h"

#include "third_party/blink/renderer/bindings/extensions_chromeos/v8/v8_cros_window_event_init.h"
#include "third_party/blink/renderer/extensions/chromeos/event_interface_chromeos_names.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window.h"

namespace blink {

CrosWindowEvent* CrosWindowEvent::Create(
    const AtomicString& type,
    const CrosWindowEventInit* event_init) {
  return MakeGarbageCollected<CrosWindowEvent>(type, event_init);
}

CrosWindowEvent::CrosWindowEvent(const AtomicString& type,
                                 const CrosWindowEventInit* event_init)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      window_(event_init->window()) {}

CrosWindowEvent::~CrosWindowEvent() = default;

void CrosWindowEvent::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  Event::Trace(visitor);
}

const AtomicString& CrosWindowEvent::InterfaceName() const {
  return event_interface_names::kCrosWindowEvent;
}

}  // namespace blink
