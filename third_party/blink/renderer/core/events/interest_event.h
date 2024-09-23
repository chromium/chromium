// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_INTEREST_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_INTEREST_EVENT_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class InterestEventInit;

class InterestEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static InterestEvent* Create(const AtomicString& type,
                               const InterestEventInit* initializer) {
    return MakeGarbageCollected<InterestEvent>(type, initializer);
  }

  static InterestEvent* Create(const AtomicString& type,
                               const String& action,
                               Element* invoker) {
    return MakeGarbageCollected<InterestEvent>(type, action, invoker);
  }

  InterestEvent(const AtomicString& type, const InterestEventInit* initializer);
  InterestEvent(const AtomicString& type,
                const String& action,
                Element* invoker);

  const AtomicString& InterfaceName() const override {
    return event_interface_names::kInterestEvent;
  }

  void Trace(Visitor*) const override;

  const String& action() const { return action_; }

  Element* invoker() const;
  void SetInvoker(Element* invoker) { invoker_ = invoker; }

 private:
  Member<Element> invoker_;
  String action_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_INTEREST_EVENT_H_
