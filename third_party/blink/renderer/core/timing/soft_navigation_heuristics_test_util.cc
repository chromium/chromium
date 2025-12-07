// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics_test_util.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {
AtomicString KeyboardEventScopeTypeToEventName(
    SoftNavigationHeuristics::EventScope::Type type) {
  switch (type) {
    case SoftNavigationHeuristics::EventScope::Type::kKeydown:
      return event_type_names::kKeydown;
    case SoftNavigationHeuristics::EventScope::Type::kKeypress:
      return event_type_names::kKeypress;
    case SoftNavigationHeuristics::EventScope::Type::kKeyup:
      return event_type_names::kKeyup;
    default:
      NOTREACHED();
  }
}
}  // namespace

Event* CreateEventForEventScopeType(
    SoftNavigationHeuristics::EventScope::Type type,
    ScriptState* script_state,
    EventTarget* event_target) {
  Event* event = nullptr;
  switch (type) {
    case SoftNavigationHeuristics::EventScope::Type::kKeydown:
    case SoftNavigationHeuristics::EventScope::Type::kKeypress:
    case SoftNavigationHeuristics::EventScope::Type::kKeyup:
      event = KeyboardEvent::Create(script_state,
                                    KeyboardEventScopeTypeToEventName(type),
                                    KeyboardEventInit::Create());
      event->SetTarget(event_target);
      break;
    case SoftNavigationHeuristics::EventScope::Type::kClick:
      event = MouseEvent::Create(script_state, event_type_names::kClick,
                                 MouseEventInit::Create());
      break;
    case SoftNavigationHeuristics::EventScope::Type::kNavigate:
      event = Event::Create(event_type_names::kNavigate);
      break;
  }
  event->SetTrusted(true);
  return event;
}

TextRecord* CreateTextRecordForTest(Node* node,
                                    int width,
                                    int height,
                                    SoftNavigationContext* context) {
  return MakeGarbageCollected<TextRecord>(
      node, width * height, gfx::RectF(width, height), gfx::Rect(width, height),
      gfx::RectF(width, height),
      /* is_needed_for_timing=*/false, context);
}

}  // namespace blink
