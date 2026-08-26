// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/toggle_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_toggle_event_init.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ToggleEvent::ToggleEvent() = default;

ToggleEvent::ToggleEvent(const AtomicString& type,
                         Event::Cancelable cancelable,
                         const String& old_state,
                         const String& new_state,
                         Element* source)
    : Event(type, Bubbles::kNo, cancelable),
      old_state_(old_state),
      new_state_(new_state),
      source_(source),
      related_target_(source) {
  DCHECK(old_state == keywords::kClosed || old_state == keywords::kOpen)
      << " old_state should be \"closed\" or \"open\". Was: " << old_state;
  DCHECK(new_state == keywords::kClosed || new_state == keywords::kOpen)
      << " new_state should be \"closed\" or \"open\". Was: " << new_state;
}

ToggleEvent::ToggleEvent(const AtomicString& type,
                         const ToggleEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasOldState()) {
    old_state_ = initializer->oldState();
  }
  if (initializer->hasNewState()) {
    new_state_ = initializer->newState();
  }
  if (initializer->hasSource()) {
    source_ = initializer->source();
    related_target_ = initializer->source();
  }
}

ToggleEvent::~ToggleEvent() = default;

const String& ToggleEvent::oldState() const {
  return old_state_;
}

const String& ToggleEvent::newState() const {
  return new_state_;
}

Element* ToggleEvent::source() const {
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

const AtomicString& ToggleEvent::InterfaceName() const {
  return event_interface_names::kToggleEvent;
}

DispatchEventResult ToggleEvent::DispatchEvent(EventDispatcher& dispatcher) {
  if (RuntimeEnabledFeatures::ShadowRootReferenceTargetEnabled(
          dispatcher.GetNode().GetExecutionContext())) {
    GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(),
                                          relatedTarget());
  }
  return dispatcher.Dispatch();
}

void ToggleEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
  visitor->Trace(source_);
  visitor->Trace(related_target_);
}

}  // namespace blink
