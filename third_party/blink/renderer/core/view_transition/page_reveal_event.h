// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_PAGE_REVEAL_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_PAGE_REVEAL_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class DOMViewTransition;

// Implementation for the pagereveal event. Fired before the first
// rendering update after a Document is activated (loaded, restored from
// BFCache, prerender activated).
// TODO(bokan): Update spec link once it's settled.
// https://drafts.csswg.org/css-view-transitions-2/#reveal-event
class PageRevealEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PageRevealEvent(DOMViewTransition*);
  ~PageRevealEvent() override;

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

  DOMViewTransition* viewTransition() const;

 private:
  Member<DOMViewTransition> dom_view_transition_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_VIEW_TRANSITION_PAGE_REVEAL_EVENT_H_
