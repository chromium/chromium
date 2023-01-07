// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/css_toggle_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_css_toggle_event_init.h"
#include "third_party/blink/renderer/core/dom/css_toggle.h"

namespace blink {

CSSToggleEvent::CSSToggleEvent(const AtomicString& type,
                               const CSSToggleEventInit* init)
    : Event(type, init) {
  if (init) {
    toggle_name_ = init->toggleName();
    toggle_ = init->toggle();
  }
}

CSSToggleEvent::CSSToggleEvent(const AtomicString& type,
                               const AtomicString& toggle_name,
                               CSSToggle* toggle)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      toggle_name_(toggle_name),
      toggle_(toggle) {}

void CSSToggleEvent::Trace(Visitor* visitor) const {
  visitor->Trace(toggle_);

  Event::Trace(visitor);
}

}  // namespace blink
