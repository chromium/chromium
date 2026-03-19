// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PAGE_HIDE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PAGE_HIDE_EVENT_H_

#include "third_party/blink/renderer/core/events/page_transition_event.h"
#include "third_party/blink/renderer/core/events/speculation_data.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class PageHideEventInit;

class PageHideEvent final : public PageTransitionEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PageHideEvent* Create(PageTransitionEventPersistence persistence,
                               SpeculationData* speculations) {
    return MakeGarbageCollected<PageHideEvent>(persistence, speculations);
  }

  static PageHideEvent* Create(const AtomicString& type,
                               const PageHideEventInit* initializer) {
    return MakeGarbageCollected<PageHideEvent>(type, initializer);
  }

  PageHideEvent(PageTransitionEventPersistence persistence,
                SpeculationData* speculations);
  PageHideEvent(const AtomicString& type, const PageHideEventInit* initializer);
  ~PageHideEvent() override;

  const AtomicString& InterfaceName() const override;

  SpeculationData* speculations() const { return speculations_.Get(); }

  void Trace(Visitor*) const override;

 private:
  Member<SpeculationData> speculations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_PAGE_HIDE_EVENT_H_
