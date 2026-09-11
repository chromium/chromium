// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_TOGGLE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_TOGGLE_EVENT_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class ToggleEventInit;

class ToggleEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ToggleEvent* Create(const AtomicString& type,
                             const ToggleEventInit* initializer) {
    return MakeGarbageCollected<ToggleEvent>(type, initializer);
  }
  static ToggleEvent* Create(const AtomicString& type,
                             Event::Cancelable cancelable,
                             const String& old_state,
                             const String& new_state,
                             Element* source) {
    auto* event = MakeGarbageCollected<ToggleEvent>(type, cancelable, old_state,
                                                    new_state, source);
    DCHECK(!event->bubbles());
    return event;
  }

  ToggleEvent();
  ToggleEvent(const AtomicString& type,
              Event::Cancelable cancelable,
              const String& old_state,
              const String& new_state,
              Element* source);
  ToggleEvent(const AtomicString& type, const ToggleEventInit* initializer);
  ~ToggleEvent() override;

  const String& oldState() const;
  const String& newState() const;
  Element* source() const;

  EventTarget* relatedTarget() const override { return related_target_.Get(); }
  void SetRelatedTarget(EventTarget* related_target) override {
    related_target_ = related_target;
  }

  const AtomicString& InterfaceName() const override;

  DispatchEventResult DispatchEvent(EventDispatcher&) override;

  void Trace(Visitor*) const override;

 private:
  String old_state_;
  String new_state_;
  Member<Element> source_;
  // Used only to shape the path for browser-generated cross-tree events.
  Member<EventTarget> related_target_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_TOGGLE_EVENT_H_
