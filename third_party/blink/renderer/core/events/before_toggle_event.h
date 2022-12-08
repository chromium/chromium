// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_BEFORE_TOGGLE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_BEFORE_TOGGLE_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class BeforeToggleEventInit;

class BeforeToggleEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BeforeToggleEvent* Create() {
    return MakeGarbageCollected<BeforeToggleEvent>();
  }
  static BeforeToggleEvent* Create(const AtomicString& type,
                                   const BeforeToggleEventInit* initializer) {
    return MakeGarbageCollected<BeforeToggleEvent>(type, initializer);
  }
  static BeforeToggleEvent* CreateBubble(const AtomicString& type,
                                         Event::Cancelable cancelable,
                                         const String& current_state,
                                         const String& new_state) {
    auto* event = MakeGarbageCollected<BeforeToggleEvent>(
        type, cancelable, current_state, new_state);
    event->SetBubbles(true);
    return event;
  }

  BeforeToggleEvent();
  BeforeToggleEvent(const AtomicString& type,
                    Event::Cancelable cancelable,
                    const String& current_state,
                    const String& new_state);
  BeforeToggleEvent(const AtomicString& type,
                    const BeforeToggleEventInit* initializer);
  ~BeforeToggleEvent() override;

  const String& currentState() const;
  const String& newState() const;

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

 private:
  String current_state_;
  String new_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_BEFORE_TOGGLE_EVENT_H_
