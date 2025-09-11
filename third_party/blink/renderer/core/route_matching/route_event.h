// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class RouteEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RouteEvent* Create(const AtomicString& type) {
    return MakeGarbageCollected<RouteEvent>(type);
  }

  RouteEvent(const AtomicString& type)
      : Event(type, Bubbles::kNo, Cancelable::kNo) {}

  const AtomicString& InterfaceName() const override {
    return event_interface_names::kRouteEvent;
  }

  bool IsRouteEvent() const override { return true; }
};

template <>
struct DowncastTraits<RouteEvent> {
  static bool AllowFrom(const Event& event) { return event.IsRouteEvent(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_ROUTE_EVENT_H_
