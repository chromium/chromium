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
  DCHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled());
  if (initializer->hasInvoker()) {
    invoker_ = initializer->invoker();
  }
  if (initializer->hasAction()) {
    action_ = initializer->action();
  }
}

InterestEvent::InterestEvent(const AtomicString& type,
                             const String& action,
                             Element* invoker)
    : Event(type, Bubbles::kNo, Cancelable::kYes, ComposedMode::kComposed),
      invoker_(invoker),
      action_(action) {
  DCHECK(RuntimeEnabledFeatures::HTMLInterestTargetAttributeEnabled());
}

Element* InterestEvent::invoker() const {
  Element* invoker = invoker_.Get();
  if (!invoker) {
    return nullptr;
  }

  if (auto* current = currentTarget()) {
    CHECK(current->ToNode());
    return &current->ToNode()->GetTreeScope().Retarget(*invoker);
  }
  DCHECK_EQ(eventPhase(), Event::PhaseType::kNone);
  return invoker;
}

void InterestEvent::Trace(Visitor* visitor) const {
  visitor->Trace(invoker_);
  Event::Trace(visitor);
}

}  // namespace blink
