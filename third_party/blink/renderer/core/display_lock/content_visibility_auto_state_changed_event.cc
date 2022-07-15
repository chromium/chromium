// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/content_visibility_auto_state_changed_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_content_visibility_auto_state_changed_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

ContentVisibilityAutoStateChangedEvent::
    ContentVisibilityAutoStateChangedEvent() = default;

ContentVisibilityAutoStateChangedEvent::
    ~ContentVisibilityAutoStateChangedEvent() = default;

ContentVisibilityAutoStateChangedEvent::ContentVisibilityAutoStateChangedEvent(
    const AtomicString& type,
    const ContentVisibilityAutoStateChangedEventInit* initializer)
    : Event(type, initializer), skipped_(initializer->skipped()) {}

ContentVisibilityAutoStateChangedEvent::ContentVisibilityAutoStateChangedEvent(
    const AtomicString& type,
    bool skipped)
    : Event(type, Bubbles::kYes, Cancelable::kYes), skipped_(skipped) {}

bool ContentVisibilityAutoStateChangedEvent::skipped() const {
  return skipped_;
}

const AtomicString& ContentVisibilityAutoStateChangedEvent::InterfaceName()
    const {
  return event_interface_names::kContentVisibilityAutoStateChangedEvent;
}

void ContentVisibilityAutoStateChangedEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
