// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_POINTER_EVENT_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_POINTER_EVENT_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Event;
class PointerEvent;

class CORE_EXPORT PointerEventContext
    : public GarbageCollected<PointerEventContext> {
 public:
  PointerEventContext() = default;

  void HandleLocalEvents(Event&) const;
  HeapVector<Member<PointerEvent>>& CoalescedEvents() {
    return coalesced_events_;
  }
  HeapVector<Member<PointerEvent>>& PredictedEvents() {
    return predicted_events_;
  }

  void Trace(Visitor*) const;

 private:
  HeapVector<Member<PointerEvent>> coalesced_events_;
  HeapVector<Member<PointerEvent>> predicted_events_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_POINTER_EVENT_CONTEXT_H_
