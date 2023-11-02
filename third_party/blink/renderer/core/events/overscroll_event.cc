// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/overscroll_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_overscroll_event_init.h"

namespace blink {

OverscrollEvent::OverscrollEvent(const AtomicString& type,
                                 bool bubbles,
                                 double delta_x,
                                 double delta_y)
    : Event(type, (bubbles ? Bubbles::kYes : Bubbles::kNo), Cancelable::kNo),
      delta_x_(delta_x),
      delta_y_(delta_y) {}

OverscrollEvent::OverscrollEvent(const AtomicString& type,
                                 bool bubbles,
                                 const OverscrollEventInit* initializer)
    : Event(type, (bubbles ? Bubbles::kYes : Bubbles::kNo), Cancelable::kNo),
      delta_x_(initializer->deltaX()),
      delta_y_(initializer->deltaY()) {}

void OverscrollEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
