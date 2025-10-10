// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_EVENT_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_EVENT_TRIGGER_H_

#include "third_party/blink/renderer/core/animation/animation_trigger.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class EventListener;
class EventTarget;
class EventTriggerOptions;
class ExecutionContext;

class CORE_EXPORT EventTrigger : public AnimationTrigger {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static EventTrigger* Create(ExecutionContext* execution_context,
                              EventTriggerOptions* options,
                              ExceptionState& exception_state);

  EventTrigger(String event_type, EventTarget& event_target);

  void Invoke();

  // AnimationTrigger
  bool CanTrigger() const override;
  bool IsEventTrigger() const override;

  // IDL
  String eventType() const { return event_type_; }
  EventTarget* eventTarget() const { return event_target_.Get(); }

  void Trace(Visitor* visitor) const override;

 private:
  void DidAddAnimation() override;
  void DidRemoveAnimation(Animation* animation) override;
  void ClearListenerIfNecessary();

  String event_type_;
  WeakMember<EventTarget> event_target_;
  WeakMember<EventListener> event_listener_;
};

template <>
struct DowncastTraits<EventTrigger> {
  static bool AllowFrom(const AnimationTrigger& trigger) {
    return trigger.IsEventTrigger();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_EVENT_TRIGGER_H_
