// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/interest_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_interest_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {

InterestEvent::InterestEvent(const AtomicString& type,
                             const InterestEventInit* initializer)
    : Event(type, initializer) {
  DCHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());
  if (initializer->hasSource()) {
    source_ = initializer->source();
  }
}

InterestEvent::InterestEvent(const AtomicString& type,
                             Element* source,
                             Event::Cancelable cancelable)
    : Event(type, Bubbles::kNo, cancelable, ComposedMode::kComposed),
      source_(source) {
  DCHECK(RuntimeEnabledFeatures::HTMLInterestForAttributeEnabled());
}

Element* InterestEvent::source() const {
  Element* source = source_.Get();
  if (!source) {
    return nullptr;
  }

  if (auto* current = currentTarget()) {
    CHECK(current->ToNode());
    return &current->ToNode()->GetTreeScope().Retarget(*source);
  }
  DCHECK_EQ(eventPhase(), Event::PhaseType::kNone);
  return source;
}

void InterestEvent::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  Event::Trace(visitor);
}

}  // namespace blink
