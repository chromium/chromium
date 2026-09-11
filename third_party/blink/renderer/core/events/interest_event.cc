// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/interest_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_interest_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

InterestEvent::InterestEvent(const AtomicString& type,
                             const InterestEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasSource()) {
    source_ = initializer->source();
    related_target_ = initializer->source();
  }
}

InterestEvent::InterestEvent(const AtomicString& type,
                             Element* source,
                             Event::Cancelable cancelable)
    : Event(type,
            Bubbles::kNo,
            cancelable,
            RuntimeEnabledFeatures::InterestEventsNonComposedEnabled()
                ? ComposedMode::kScoped
                : ComposedMode::kComposed),
      source_(source),
      related_target_(source) {}

Element* InterestEvent::source() const {
  return DynamicTo<Element>(Retarget(source_));
}

DispatchEventResult InterestEvent::DispatchEvent(EventDispatcher& dispatcher) {
  if (RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          dispatcher.GetNode().GetExecutionContext())) {
    GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(),
                                          relatedTarget());
  }
  return dispatcher.Dispatch();
}

void InterestEvent::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(related_target_);
  Event::Trace(visitor);
}

}  // namespace blink
