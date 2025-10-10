// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/event_trigger.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_event_trigger_options.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

class TriggerEventListener : public NativeEventListener {
 public:
  explicit TriggerEventListener(EventTrigger& trigger) : trigger_(&trigger) {}

  void Trace(Visitor* visitor) const override {
    NativeEventListener::Trace(visitor);
    visitor->Trace(trigger_);
  }

 private:
  void Invoke(ExecutionContext*, Event*) override { trigger_->Invoke(); }
  Member<EventTrigger> trigger_;
};

}  // namespace

EventTrigger::EventTrigger(String event_type, EventTarget& event_target)
    : event_type_(event_type), event_target_(&event_target) {}

/* static */
EventTrigger* EventTrigger::Create(ExecutionContext* execution_context,
                                   EventTriggerOptions* options,
                                   ExceptionState& exception_state) {
  if (options->getEventTypeOr(String()).empty()) {
    exception_state.ThrowTypeError(
        "EventTrigger must have a non-empty eventType");
    return nullptr;
  }
  return MakeGarbageCollected<EventTrigger>(options->eventType(),
                                            *options->eventTarget());
}

bool EventTrigger::CanTrigger() const {
  return event_target_ && event_listener_;
}

bool EventTrigger::IsEventTrigger() const {
  return true;
}

void EventTrigger::Invoke() {
  // TODO(441908430): EventTriggers should account for deactivate events &
  // behaviors. When an event occurs, we should determine, based on the event
  // and the trigger's state, whether activate or deactivate has occurred.
  PerformActivate();
}

void EventTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(event_target_);
  visitor->Trace(event_listener_);
  AnimationTrigger::Trace(visitor);
}

void EventTrigger::DidAddAnimation() {
  if (event_target_ && !event_listener_) {
    auto* options = MakeGarbageCollected<AddEventListenerOptionsResolved>();
    options->SetAnimationTrigger(true);
    event_listener_ = MakeGarbageCollected<TriggerEventListener>(*this);
    event_target_->addEventListener(AtomicString(event_type_),
                                    event_listener_.Get(), options);
  }
}

void EventTrigger::DidRemoveAnimation(Animation* animation) {
  if (event_target_ && event_listener_ && BehaviorMap().empty()) {
    event_target_->removeEventListener(AtomicString(event_type_),
                                       event_listener_.Get(),
                                       /*use_capture=*/false);
    event_listener_.Clear();
  }
}

}  // namespace blink
