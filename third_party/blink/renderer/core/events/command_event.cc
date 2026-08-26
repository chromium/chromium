// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/command_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_command_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

CommandEvent::CommandEvent(const AtomicString& type,
                         const CommandEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasSource()) {
    source_ = initializer->source();
    related_target_ = initializer->source();
  }

  if (initializer->hasCommand()) {
    command_ = initializer->command();
  }
}

CommandEvent::CommandEvent(const AtomicString& type,
                           const String& command,
                           Element* source)
    : Event(type, Bubbles::kNo, Cancelable::kYes, ComposedMode::kScoped),
      source_(source),
      related_target_(source) {
  command_ = command;
}

Element* CommandEvent::source() const {
  if (!source_) {
    CHECK(!related_target_);
    return nullptr;
  }

  if (RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          source_->GetExecutionContext())) {
    EventTarget* related_target = related_target_.Get();
    return related_target ? DynamicTo<Element>(related_target->ToNode())
                          : nullptr;
  }

  return DynamicTo<Element>(Retarget(source_));
}

DispatchEventResult CommandEvent::DispatchEvent(EventDispatcher& dispatcher) {
  if (RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          dispatcher.GetNode().GetExecutionContext())) {
    GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(),
                                          relatedTarget());
  }
  return dispatcher.Dispatch();
}

void CommandEvent::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(related_target_);
  Event::Trace(visitor);
}

}  // namespace blink
