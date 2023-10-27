// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/invoke_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_invoke_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {

InvokeEvent::InvokeEvent(const AtomicString& type,
                         const InvokeEventInit* initializer)
    : Event(type, initializer) {
  DCHECK(RuntimeEnabledFeatures::HTMLInvokeTargetAttributeEnabled());
  if (initializer->hasInvoker()) {
    invoker_ = initializer->invoker();
  }

  if (initializer->hasAction()) {
    action_ = initializer->action();
  }
}

InvokeEvent::InvokeEvent(const AtomicString& type,
                         const String& action,
                         Element* invoker)
    : Event(type, Bubbles::kNo, Cancelable::kYes), invoker_(invoker) {
  DCHECK(RuntimeEnabledFeatures::HTMLInvokeTargetAttributeEnabled());
  action_ = action;
}

Element* InvokeEvent::invoker() const {
  auto* current = currentTarget();
  Element* invoker = invoker_.Get();
  if (current) {
    return &current->ToNode()->GetTreeScope().Retarget(*invoker);
  }
  DCHECK_EQ(eventPhase(), Event::PhaseType::kNone);
  return invoker;
}

void InvokeEvent::Trace(Visitor* visitor) const {
  visitor->Trace(invoker_);
  Event::Trace(visitor);
}

}  // namespace blink
