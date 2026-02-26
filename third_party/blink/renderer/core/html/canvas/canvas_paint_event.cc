// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_paint_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

CanvasPaintEvent::CanvasPaintEvent(const AtomicString& type,
                                   const CanvasPaintEventInit* initializer)
    : Event(type, initializer) {
  CHECK(initializer);
  if (initializer->hasChangedElements()) {
    changed_elements_ = initializer->changedElements();
  }
}

CanvasPaintEvent::~CanvasPaintEvent() = default;

const HeapVector<Member<Element>>& CanvasPaintEvent::changedElements() const {
  return changed_elements_;
}

const AtomicString& CanvasPaintEvent::InterfaceName() const {
  return event_interface_names::kCanvasPaintEvent;
}

void CanvasPaintEvent::Trace(Visitor* visitor) const {
  visitor->Trace(changed_elements_);
  Event::Trace(visitor);
}

}  // namespace blink
