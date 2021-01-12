// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/simulated_event_util.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/mouse_event_init.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/pointer_event_init.h"
#include "third_party/blink/renderer/core/events/ui_event.h"
#include "third_party/blink/renderer/core/events/ui_event_with_key_state.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

void PopulateSimulatedMouseEventInit(const AtomicString& event_type,
                                     AbstractView* view,
                                     const Event* underlying_event,
                                     MouseEventInit* initializer) {
  WebInputEvent::Modifiers modifiers = WebInputEvent::kNoModifiers;
  if (const UIEventWithKeyState* key_state_event =
          FindEventWithKeyState(underlying_event)) {
    modifiers = key_state_event->GetModifiers();
  }

  if (const auto* mouse_event = DynamicTo<MouseEvent>(underlying_event)) {
    initializer->setScreenX(mouse_event->screen_location_.X());
    initializer->setScreenY(mouse_event->screen_location_.Y());
    initializer->setSourceCapabilities(
        view ? view->GetInputDeviceCapabilities()->FiresTouchEvents(false)
             : nullptr);
  }

  initializer->setBubbles(true);
  initializer->setCancelable(true);
  initializer->setView(view);
  initializer->setComposed(true);
  UIEventWithKeyState::SetFromWebInputEventModifiers(initializer, modifiers);
  initializer->setButtons(
      MouseEvent::WebInputEventModifiersToButtons(modifiers));
}

enum class EventClassType { kMouse, kPointer };

MouseEvent* CreateMouseOrPointerEvent(
    EventClassType event_class_type,
    const AtomicString& event_type,
    AbstractView* view,
    const Event* underlying_event,
    SimulatedClickCreationScope creation_scope) {
  // We picked |PointerEventInit| object to be able to create either
  // |MouseEvent| or |PointerEvent| below.  When a |PointerEvent| is created,
  // any event attributes not initialized in the |PointerEventInit| below get
  // their default values, all of which are appropriate for a simulated
  // |PointerEvent|.
  //
  // TODO(mustaq): Set |pointerId| to -1 after we have a spec change to fix the
  // issue https://github.com/w3c/pointerevents/issues/343.
  PointerEventInit* initializer = PointerEventInit::Create();
  PopulateSimulatedMouseEventInit(event_type, view, underlying_event,
                                  initializer);

  base::TimeTicks timestamp = underlying_event
                                  ? underlying_event->PlatformTimeStamp()
                                  : base::TimeTicks::Now();
  MouseEvent::SyntheticEventType synthetic_type = MouseEvent::kPositionless;
  if (const auto* mouse_event = DynamicTo<MouseEvent>(underlying_event)) {
    synthetic_type = MouseEvent::kRealOrIndistinguishable;
  }

  MouseEvent* created_event;
  if (event_class_type == EventClassType::kPointer) {
    created_event = MakeGarbageCollected<PointerEvent>(
        event_type, initializer, timestamp, synthetic_type);
  } else {
    created_event = MakeGarbageCollected<MouseEvent>(event_type, initializer,
                                                     timestamp, synthetic_type);
  }

  created_event->SetTrusted(creation_scope ==
                            SimulatedClickCreationScope::kFromUserAgent);
  created_event->SetUnderlyingEvent(underlying_event);
  if (synthetic_type == MouseEvent::kRealOrIndistinguishable) {
    auto* mouse_event = To<MouseEvent>(created_event->UnderlyingEvent());
    created_event->InitCoordinates(mouse_event->client_location_.X(),
                                   mouse_event->client_location_.Y());
  }

  return created_event;
}

}  // namespace

Event* SimulatedEventUtil::CreateEvent(
    const AtomicString& event_type,
    AbstractView* view,
    const Event* underlying_event,
    SimulatedClickCreationScope creation_scope) {
  DCHECK(event_type == event_type_names::kClick ||
         event_type == event_type_names::kMousedown ||
         event_type == event_type_names::kMouseup);

  EventClassType event_class_type = EventClassType::kMouse;
  if (RuntimeEnabledFeatures::ClickPointerEventEnabled() &&
      event_type == event_type_names::kClick) {
    event_class_type = EventClassType::kPointer;
  }
  return CreateMouseOrPointerEvent(event_class_type, event_type, view,
                                   underlying_event, creation_scope);
}

}  // namespace blink
