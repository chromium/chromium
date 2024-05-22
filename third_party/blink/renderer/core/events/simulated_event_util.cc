// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/simulated_event_util.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_ui_event_init.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/pointer_type_names.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

void PopulateMouseEventInitCoordinates(
    Node& node,
    MouseEventInit* initializer,
    SimulatedClickCreationScope creation_scope) {
  Element* element = DynamicTo<Element>(node);
  LocalDOMWindow* dom_window = node.GetDocument().domWindow();

  if (element && dom_window && element->GetLayoutObject() &&
      element->GetLayoutBox() &&
      creation_scope == SimulatedClickCreationScope::kFromAccessibility) {
    // If we have an element we will set coordinates to the center of the
    // element.
    // TODO(crbug.com/1171924): User Agent Simulated Clicks should change
    // hover states, fire events like mouseout/mouseover etc.
    LayoutBox* layout_box = element->GetLayoutBox();
    LayoutObject* layout_object = element->GetLayoutObject();
    PhysicalOffset center = layout_box->PhysicalBorderBoxRect().Center();
    PhysicalOffset root_frame_center = layout_object->LocalToAncestorPoint(
        center, nullptr, MapCoordinatesMode::kTraverseDocumentBoundaries);
    PhysicalOffset frame_center =
        dom_window->GetFrame()->View()->ConvertFromRootFrame(root_frame_center);
    gfx::Point frame_center_point = ToRoundedPoint(frame_center);
    // We are only interested in the top left corner.
    gfx::Rect center_rect(frame_center_point.x(), frame_center_point.y(), 1, 1);
    gfx::Point screen_center =
        dom_window->GetFrame()->View()->FrameToScreen(center_rect).origin();

    initializer->setScreenX(
        AdjustForAbsoluteZoom::AdjustInt(screen_center.x(), layout_object));
    initializer->setScreenY(
        AdjustForAbsoluteZoom::AdjustInt(screen_center.y(), layout_object));
    initializer->setClientX(AdjustForAbsoluteZoom::AdjustInt(
        frame_center_point.x(), layout_object));
    initializer->setClientY(AdjustForAbsoluteZoom::AdjustInt(
        frame_center_point.y(), layout_object));
  }
}

void PopulateSimulatedMouseEventInit(
    const AtomicString& event_type,
    Node& node,
    const Event* underlying_event,
    MouseEventInit* initializer,
    SimulatedClickCreationScope creation_scope) {
  WebInputEvent::Modifiers modifiers = WebInputEvent::kNoModifiers;
  if (const UIEventWithKeyState* key_state_event =
          FindEventWithKeyState(underlying_event)) {
    modifiers = key_state_event->GetModifiers();
  }

  PopulateMouseEventInitCoordinates(node, initializer, creation_scope);
  LocalDOMWindow* dom_window = node.GetDocument().domWindow();
  if (const auto* mouse_event = DynamicTo<MouseEvent>(underlying_event)) {
    initializer->setScreenX(mouse_event->screenX());
    initializer->setScreenY(mouse_event->screenY());
    initializer->setSourceCapabilities(
        dom_window
            ? dom_window->GetInputDeviceCapabilities()->FiresTouchEvents(false)
            : nullptr);
  }

  initializer->setBubbles(true);
  initializer->setCancelable(true);
  initializer->setView(dom_window);
  initializer->setComposed(true);
  UIEventWithKeyState::SetFromWebInputEventModifiers(initializer, modifiers);
  initializer->setButtons(
      MouseEvent::WebInputEventModifiersToButtons(modifiers));
}

enum class EventClassType { kMouse, kPointer };

MouseEvent* CreateMouseOrPointerEvent(
    EventClassType event_class_type,
    const AtomicString& event_type,
    Node& node,
    const Event* underlying_event,
    SimulatedClickCreationScope creation_scope) {
  // We picked |PointerEventInit| object to be able to create either
  // |MouseEvent| or |PointerEvent| below.  When a |PointerEvent| is created,
  // any event attributes not initialized in the |PointerEventInit| below get
  // their default values, all of which are appropriate for a simulated
  // |PointerEvent|.
  PointerEventInit* initializer = PointerEventInit::Create();
  PopulateSimulatedMouseEventInit(event_type, node, underlying_event,
                                  initializer, creation_scope);

  base::TimeTicks timestamp = underlying_event
                                  ? underlying_event->PlatformTimeStamp()
                                  : base::TimeTicks::Now();
  MouseEvent::SyntheticEventType synthetic_type = MouseEvent::kPositionless;
  if (IsA<MouseEvent>(underlying_event)) {
    synthetic_type = MouseEvent::kRealOrIndistinguishable;
  }
  if (creation_scope == SimulatedClickCreationScope::kFromAccessibility) {
    if (event_type == event_type_names::kClick ||
        event_type == event_type_names::kPointerdown ||
        event_type == event_type_names::kMousedown) {
      // Set primary button pressed.
      initializer->setButton(
          static_cast<int>(WebPointerProperties::Button::kLeft));
      initializer->setButtons(MouseEvent::WebInputEventModifiersToButtons(
          WebInputEvent::Modifiers::kLeftButtonDown));
    }
    if (event_type == event_type_names::kPointerup ||
        event_type == event_type_names::kMouseup) {
      // Set primary button pressed.
      initializer->setButton(
          static_cast<int>(WebPointerProperties::Button::kLeft));
    }
    if (event_type == event_type_names::kClick) {
      // Set number of clicks for click event.
      initializer->setDetail(1);
    }
  }

  MouseEvent* created_event;
  if (event_class_type == EventClassType::kPointer) {
    if (creation_scope == SimulatedClickCreationScope::kFromAccessibility) {
      initializer->setPointerId(PointerEventFactory::kMouseId);
      initializer->setPointerType(pointer_type_names::kMouse);
      initializer->setIsPrimary(true);
    } else {
      initializer->setPointerId(PointerEventFactory::kReservedNonPointerId);
    }
    created_event = MakeGarbageCollected<PointerEvent>(
        event_type, initializer, timestamp, synthetic_type);
  } else {
    created_event = MakeGarbageCollected<MouseEvent>(event_type, initializer,
                                                     timestamp, synthetic_type);
  }

  created_event->SetTrusted(
      creation_scope == SimulatedClickCreationScope::kFromUserAgent ||
      creation_scope == SimulatedClickCreationScope::kFromAccessibility);
  created_event->SetUnderlyingEvent(underlying_event);
  if (synthetic_type == MouseEvent::kRealOrIndistinguishable) {
    auto* mouse_event = To<MouseEvent>(created_event->UnderlyingEvent());
    created_event->InitCoordinates(mouse_event->clientX(),
                                   mouse_event->clientY());
  }

  return created_event;
}

}  // namespace

Event* SimulatedEventUtil::CreateEvent(
    const AtomicString& event_type,
    Node& node,
    const Event* underlying_event,
    SimulatedClickCreationScope creation_scope) {
  DCHECK(event_type == event_type_names::kClick ||
         event_type == event_type_names::kMousedown ||
         event_type == event_type_names::kMouseup ||
         event_type == event_type_names::kPointerdown ||
         event_type == event_type_names::kPointerup);

  EventClassType event_class_type = EventClassType::kMouse;
  if (event_type == event_type_names::kClick ||
      event_type == event_type_names::kPointerdown ||
      event_type == event_type_names::kPointerup) {
    event_class_type = EventClassType::kPointer;
  }

  return CreateMouseOrPointerEvent(event_class_type, event_type, node,
                                   underlying_event, creation_scope);
}

}  // namespace blink
