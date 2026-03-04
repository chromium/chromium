// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class OverscrollEventInit;
class OverscrollEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OverscrollEvent* Create(const AtomicString& event_type,
                                 const OverscrollEventInit* init) {
    return MakeGarbageCollected<OverscrollEvent>(event_type, init);
  }

  OverscrollEvent(const AtomicString& event_type,
                  const OverscrollEventInit* init);
  ~OverscrollEvent() override;

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

  Element* overscrollElement() const;

 private:
  Member<Element> overscroll_element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_OVERSCROLL_OVERSCROLL_EVENT_H_
