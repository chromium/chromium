// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/page_reveal_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

PageRevealEvent::PageRevealEvent()
    : Event(event_type_names::kPagereveal, Bubbles::kNo, Cancelable::kNo) {
  CHECK(RuntimeEnabledFeatures::PageRevealEventEnabled());
}

PageRevealEvent::~PageRevealEvent() = default;

const AtomicString& PageRevealEvent::InterfaceName() const {
  return event_interface_names::kPageRevealEvent;
}

void PageRevealEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
