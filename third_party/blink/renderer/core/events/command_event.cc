// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/command_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_command_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {

CommandEvent::CommandEvent(const AtomicString& type,
                         const CommandEventInit* initializer)
    : Event(type, initializer) {
  DCHECK(RuntimeEnabledFeatures::HTMLInvokeTargetAttributeEnabled());
  if (initializer->hasInvoker()) {
    invoker_ = initializer->invoker();
  }

  if (initializer->hasCommand()) {
    command_ = initializer->command();
  }
}

CommandEvent::CommandEvent(const AtomicString& type,
                         const String& command,
                         Element* invoker)
    : Event(type, Bubbles::kNo, Cancelable::kYes, ComposedMode::kComposed),
      invoker_(invoker) {
  DCHECK(RuntimeEnabledFeatures::HTMLInvokeTargetAttributeEnabled());
  command_ = command;
}

Element* CommandEvent::invoker() const {
  auto* current = currentTarget();
  Element* invoker = invoker_.Get();
  if (!invoker) {
    return nullptr;
  }

  if (current) {
    return &current->ToNode()->GetTreeScope().Retarget(*invoker);
  }
  DCHECK_EQ(eventPhase(), Event::PhaseType::kNone);
  return invoker;
}

void CommandEvent::Trace(Visitor* visitor) const {
  visitor->Trace(invoker_);
  Event::Trace(visitor);
}

}  // namespace blink
