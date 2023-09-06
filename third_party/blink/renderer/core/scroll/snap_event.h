// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SNAP_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SNAP_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// This class implements the SnapEvent interface for scroll-snap-related
// JavaScript events, snapchanged and snapchanging.
// SnapEvents are sent to a scroller when it snaps to a different element from
// the element to which it was previously snapped along either axis.
// https://drafts.csswg.org/css-scroll-snap-2/#snapchanged-and-snapchanging
class SnapEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SnapEvent* Create(const AtomicString& type,
                           HeapVector<Member<Node>>& targets);
  SnapEvent(const AtomicString& type, HeapVector<Member<Node>>& targets);

  StaticNodeList* snapTargets() { return snap_targets_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(snap_targets_);
    Event::Trace(visitor);
  }

 private:
  // This contains elements to which the scrolling container is currently
  // snapped along both axes.
  Member<StaticNodeList> snap_targets_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SNAP_EVENT_H_
