// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/page_hide_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_page_hide_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

PageHideEvent::PageHideEvent(PageTransitionEventPersistence persistence,
                             SpeculationData* speculations)
    : PageTransitionEvent(event_type_names::kPagehide, persistence),
      speculations_(speculations) {}

PageHideEvent::PageHideEvent(const AtomicString& type,
                             const PageHideEventInit* initializer)
    : PageTransitionEvent(type, initializer), speculations_(nullptr) {
  if (initializer->hasSpeculations()) {
    speculations_ = initializer->speculations();
  }
}

PageHideEvent::~PageHideEvent() = default;

const AtomicString& PageHideEvent::InterfaceName() const {
  return event_interface_names::kPageHideEvent;
}

void PageHideEvent::Trace(Visitor* visitor) const {
  visitor->Trace(speculations_);
  PageTransitionEvent::Trace(visitor);
}

}  // namespace blink
