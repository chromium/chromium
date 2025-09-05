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

EventTrigger::EventTrigger(AnimationTrigger::Behavior behavior,
                           String event_type,
                           EventTarget& event_target)
    : AnimationTrigger(behavior),
      event_type_(event_type),
      event_target_(&event_target) {}

/* static */
EventTrigger* EventTrigger::Create(ExecutionContext* execution_context,
                                   EventTriggerOptions* options,
                                   ExceptionState& exception_state) {
  if (options->eventType().empty()) {
    exception_state.ThrowTypeError(
        "EventTrigger must have a non-empty eventType");
    return nullptr;
  }
  return MakeGarbageCollected<EventTrigger>(
      options->behavior(), options->eventType(), *options->eventTarget());
}

bool EventTrigger::CanTrigger() const {
  return event_target_ && event_listener_;
}

bool EventTrigger::IsEventTrigger() const {
  return true;
}

void EventTrigger::Invoke() {
  switch (behavior().AsEnum()) {
    case Behavior::Enum::kOnce:
      for (Animation* animation : animations()) {
        animation->play();
      }
      event_target_.Clear();
      event_listener_.Clear();
      break;
    case Behavior::Enum::kRepeat:
      for (Animation* animation : animations()) {
        switch (animation->CalculateAnimationPlayState()) {
          case V8AnimationPlayState::Enum::kRunning:
          case V8AnimationPlayState::Enum::kPaused:
            animation->finish();
            break;
          default:
            break;
        }
        animation->play();
      }
      break;
    case Behavior::Enum::kAlternate:
      for (Animation* animation : animations()) {
        if (animation->CalculateAnimationPlayState() ==
            V8AnimationPlayState::Enum::kIdle) {
          animation->play();
        } else {
          animation->reverse();
        }
      }
      break;
    case Behavior::Enum::kState:
      for (Animation* animation : animations()) {
        if (animation->CalculateAnimationPlayState() ==
            V8AnimationPlayState::Enum::kRunning) {
          animation->pause();
        } else {
          animation->play();
        }
      }
      break;
    default:
      NOTREACHED();
  }
}

void EventTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(event_target_);
  visitor->Trace(event_listener_);
  AnimationTrigger::Trace(visitor);
}

void EventTrigger::DidAddAnimation(Animation* animation,
                                   ExceptionState& exception_state) {
  if (event_target_ && !event_listener_) {
    auto* options = MakeGarbageCollected<AddEventListenerOptionsResolved>();
    options->SetAnimationTrigger(true);
    options->setOnce(behavior() == Behavior::Enum::kOnce);
    event_listener_ = MakeGarbageCollected<TriggerEventListener>(*this);
    event_target_->addEventListener(AtomicString(event_type_),
                                    event_listener_.Get(), options);
  }
}

void EventTrigger::DidRemoveAnimation(Animation* animation) {
  if (event_target_ && event_listener_ && animations().empty()) {
    event_target_->removeEventListener(AtomicString(event_type_),
                                       event_listener_.Get(),
                                       /*use_capture=*/false);
    event_listener_.Clear();
  }
}

}  // namespace blink
