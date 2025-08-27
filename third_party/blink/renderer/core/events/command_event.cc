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
  if (initializer->hasSource()) {
    source_ = initializer->source();
  }

  if (initializer->hasCommand()) {
    command_ = initializer->command();
  }
}

CommandEvent::CommandEvent(const AtomicString& type,
                           const String& command,
                           Element* source)
    : Event(type,
            Bubbles::kNo,
            Cancelable::kYes,
            RuntimeEnabledFeatures::CommandEventNotComposedEnabled()
                ? ComposedMode::kScoped
                : ComposedMode::kComposed),
      source_(source) {
  command_ = command;
}

Element* CommandEvent::source() const {
  auto* current = currentTarget();
  Element* source = source_.Get();
  if (!source) {
    return nullptr;
  }

  if (RuntimeEnabledFeatures::ImprovedSourceRetargetingEnabled()) {
    return Retarget(source);
  }

  if (current) {
    return &current->ToNode()->GetTreeScope().Retarget(*source);
  }
  DCHECK_EQ(eventPhase(), Event::PhaseType::kNone);
  return source;
}

void CommandEvent::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  Event::Trace(visitor);
}

}  // namespace blink
