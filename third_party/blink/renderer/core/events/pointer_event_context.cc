// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/pointer_event_context.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void PointerEventContext::HandleLocalEvents(Event& event) const {
  CHECK(RuntimeEnabledFeatures::RetargetEventsInPointerEventListsEnabled());
  CHECK(IsA<PointerEvent>(event));
  auto& pointer_event = To<PointerEvent>(event);
  pointer_event.SetCoalescedEvents(coalesced_events_);
  pointer_event.SetPredictedEvents(predicted_events_);
}

void PointerEventContext::Trace(Visitor* visitor) const {
  visitor->Trace(coalesced_events_);
  visitor->Trace(predicted_events_);
}

}  // namespace blink
