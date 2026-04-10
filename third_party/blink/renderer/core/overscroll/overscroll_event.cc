// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/overscroll/overscroll_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_overscroll_event_init.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

OverscrollEvent::OverscrollEvent(const AtomicString& event_type,
                                 const OverscrollEventInit* init)
    : Event(event_type,
            init->bubbles() ? Bubbles::kYes : Bubbles::kNo,
            Cancelable::kNo),
      overscroll_target_(init->overscrollTarget()),
      overscrolling_(init->overscrolling()) {}

OverscrollEvent::~OverscrollEvent() = default;

const AtomicString& OverscrollEvent::InterfaceName() const {
  return event_interface_names::kOverscrollEvent;
}

void OverscrollEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
  visitor->Trace(overscroll_target_);
}

Element* OverscrollEvent::overscrollTarget() const {
  return overscroll_target_;
}

bool OverscrollEvent::overscrolling() const {
  return overscrolling_;
}

}  // namespace blink
