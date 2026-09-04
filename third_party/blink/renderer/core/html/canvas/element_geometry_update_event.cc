// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/element_geometry_update_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

ElementGeometryUpdateEvent::ElementGeometryUpdateEvent(
    const AtomicString& type,
    const ElementGeometryUpdateEventInit* initializer)
    : Event(type, initializer) {
  CHECK(initializer);
  if (initializer->hasElements()) {
    elements_ = initializer->elements();
  }
}

ElementGeometryUpdateEvent::~ElementGeometryUpdateEvent() = default;

const HeapVector<Member<Element>>& ElementGeometryUpdateEvent::elements()
    const {
  return elements_;
}

const AtomicString& ElementGeometryUpdateEvent::InterfaceName() const {
  return event_interface_names::kElementGeometryUpdateEvent;
}

void ElementGeometryUpdateEvent::Trace(Visitor* visitor) const {
  visitor->Trace(elements_);
  Event::Trace(visitor);
}

}  // namespace blink
