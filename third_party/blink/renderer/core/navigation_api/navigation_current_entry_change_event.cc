// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/navigation_api/navigation_current_entry_change_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_current_entry_change_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"

namespace blink {

NavigationCurrentEntryChangeEvent::NavigationCurrentEntryChangeEvent(
    const AtomicString& type,
    NavigationCurrentEntryChangeEventInit* init)
    : Event(type, init), from_(init->from()) {
  if (init->navigationType()) {
    navigation_type_ = init->navigationType()->AsEnum();
  }
}

std::optional<V8NavigationType>
NavigationCurrentEntryChangeEvent::navigationType() {
  if (!navigation_type_) {
    return std::nullopt;
  }
  return V8NavigationType(navigation_type_.value());
}

const AtomicString& NavigationCurrentEntryChangeEvent::InterfaceName() const {
  return event_interface_names::kNavigationCurrentEntryChangeEvent;
}

void NavigationCurrentEntryChangeEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
  visitor->Trace(from_);
}

}  // namespace blink
