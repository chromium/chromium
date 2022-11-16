// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/content_visibility_auto_state_change_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_content_visibility_auto_state_change_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

ContentVisibilityAutoStateChangeEvent::ContentVisibilityAutoStateChangeEvent() =
    default;

ContentVisibilityAutoStateChangeEvent::
    ~ContentVisibilityAutoStateChangeEvent() = default;

ContentVisibilityAutoStateChangeEvent::ContentVisibilityAutoStateChangeEvent(
    const AtomicString& type,
    const ContentVisibilityAutoStateChangeEventInit* initializer)
    : Event(type, initializer), skipped_(initializer->skipped()) {}

ContentVisibilityAutoStateChangeEvent::ContentVisibilityAutoStateChangeEvent(
    const AtomicString& type,
    bool skipped)
    : Event(type, Bubbles::kYes, Cancelable::kYes), skipped_(skipped) {}

bool ContentVisibilityAutoStateChangeEvent::skipped() const {
  return skipped_;
}

const AtomicString& ContentVisibilityAutoStateChangeEvent::InterfaceName()
    const {
  return event_interface_names::kContentVisibilityAutoStateChangeEvent;
}

void ContentVisibilityAutoStateChangeEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
