// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/page_reveal_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_page_reveal_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/view_transition/dom_view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

PageRevealEvent::PageRevealEvent()
    : Event(event_type_names::kPagereveal, Bubbles::kNo, Cancelable::kNo) {
  CHECK(RuntimeEnabledFeatures::PageRevealEventEnabled());
}

PageRevealEvent::PageRevealEvent(const AtomicString& type,
                                 const PageRevealEventInit* initializer)
    : Event(type, initializer),
      dom_view_transition_(initializer ? initializer->viewTransition()
                                       : nullptr) {
  CHECK(RuntimeEnabledFeatures::PageRevealEventEnabled());
}

PageRevealEvent::~PageRevealEvent() = default;

const AtomicString& PageRevealEvent::InterfaceName() const {
  return event_interface_names::kPageRevealEvent;
}

void PageRevealEvent::Trace(Visitor* visitor) const {
  visitor->Trace(dom_view_transition_);
  Event::Trace(visitor);
}

DOMViewTransition* PageRevealEvent::viewTransition() const {
  return dom_view_transition_.Get();
}

void PageRevealEvent::SetViewTransition(
    DOMViewTransition* dom_view_transition) {
  dom_view_transition_ = dom_view_transition;
}

}  // namespace blink
