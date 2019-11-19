/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/events/wheel_event.h"

#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

unsigned ConvertDeltaMode(const WebMouseWheelEvent& event) {
  // WebMouseWheelEvent only supports these units for the delta.
  DCHECK(
      event.delta_units == ui::input_types::ScrollGranularity::kScrollByPage ||
      event.delta_units == ui::input_types::ScrollGranularity::kScrollByPixel ||
      event.delta_units ==
          ui::input_types::ScrollGranularity::kScrollByPrecisePixel);
  return event.delta_units == ui::input_types::ScrollGranularity::kScrollByPage
             ? WheelEvent::kDomDeltaPage
             : WheelEvent::kDomDeltaPixel;
}

// Negate a long value without integer overflow.
int32_t NegateIfPossible(int32_t value) {
  if (value == std::numeric_limits<int32_t>::min())
    return value;
  return -value;
}

MouseEventInit* GetMouseEventInitForWheel(const WebMouseWheelEvent& event,
                                          AbstractView* view) {
  MouseEventInit* initializer = MouseEventInit::Create();
  initializer->setBubbles(true);
  initializer->setCancelable(event.IsCancelable());
  auto* local_dom_window = DynamicTo<LocalDOMWindow>(view);
  MouseEvent::SetCoordinatesFromWebPointerProperties(
      event.FlattenTransform(), local_dom_window, initializer);
  initializer->setButton(static_cast<int16_t>(event.button));
  initializer->setButtons(
      MouseEvent::WebInputEventModifiersToButtons(event.GetModifiers()));
  initializer->setView(view);
  initializer->setComposed(true);
  initializer->setDetail(event.click_count);
  UIEventWithKeyState::SetFromWebInputEventModifiers(
      initializer, static_cast<WebInputEvent::Modifiers>(event.GetModifiers()));

  // TODO(zino): Should support canvas hit region because the
  // wheel event is a kind of mouse event. Please see
  // http://crbug.com/594075

  return initializer;
}

}  // namespace

WheelEvent* WheelEvent::Create(const WebMouseWheelEvent& event,
                               AbstractView* view) {
  return MakeGarbageCollected<WheelEvent>(event, view);
}

WheelEvent::WheelEvent()
    : delta_x_(0), delta_y_(0), delta_z_(0), delta_mode_(kDomDeltaPixel) {}

WheelEvent::WheelEvent(const AtomicString& type,
                       const WheelEventInit* initializer)
    : MouseEvent(type, initializer),
      wheel_delta_(initializer->wheelDeltaX()
                       ? initializer->wheelDeltaX()
                       : NegateIfPossible(-initializer->deltaX()),
                   initializer->wheelDeltaY()
                       ? initializer->wheelDeltaY()
                       : NegateIfPossible(-initializer->deltaY())),
      delta_x_(initializer->deltaX()
                   ? initializer->deltaX()
                   : NegateIfPossible(initializer->wheelDeltaX())),
      delta_y_(initializer->deltaY()
                   ? initializer->deltaY()
                   : NegateIfPossible(initializer->wheelDeltaY())),
      delta_z_(initializer->deltaZ()),
      delta_mode_(initializer->deltaMode()) {}

WheelEvent::WheelEvent(const WebMouseWheelEvent& event, AbstractView* view)
    : MouseEvent(event_type_names::kWheel,
                 GetMouseEventInitForWheel(event, view),
                 event.TimeStamp()),
      wheel_delta_(event.wheel_ticks_x * kTickMultiplier,
                   event.wheel_ticks_y * kTickMultiplier),
      delta_x_(-event.DeltaXInRootFrame()),
      delta_y_(-event.DeltaYInRootFrame()),
      delta_z_(0),
      delta_mode_(ConvertDeltaMode(event)),
      native_event_(event) {}

const AtomicString& WheelEvent::InterfaceName() const {
  return event_interface_names::kWheelEvent;
}

bool WheelEvent::IsMouseEvent() const {
  return false;
}

bool WheelEvent::IsWheelEvent() const {
  return true;
}

void WheelEvent::preventDefault() {
  MouseEvent::preventDefault();

  PassiveMode passive_mode = HandlingPassive();
  if (passive_mode == PassiveMode::kPassiveForcedDocumentLevel) {
    String id = "PreventDefaultPassive";
    String message =
        "Unable to preventDefault inside passive event listener due to "
        "target being treated as passive. See "
        "https://www.chromestatus.com/features/6662647093133312";
    auto* local_dom_window = DynamicTo<LocalDOMWindow>(view());
    if (local_dom_window && local_dom_window->GetFrame()) {
      Intervention::GenerateReport(local_dom_window->GetFrame(), id, message);
    }
  }

  if (!currentTarget() || !currentTarget()->IsTopLevelNode())
    return;

  if (passive_mode == PassiveMode::kPassiveForcedDocumentLevel ||
      passive_mode == PassiveMode::kNotPassiveDefault) {
    if (ExecutionContext* context = currentTarget()->GetExecutionContext()) {
      UseCounter::Count(
          context,
          WebFeature::kDocumentLevelPassiveDefaultEventListenerPreventedWheel);
    }
  }
}

DispatchEventResult WheelEvent::DispatchEvent(EventDispatcher& dispatcher) {
  return dispatcher.Dispatch();
}

void WheelEvent::Trace(blink::Visitor* visitor) {
  MouseEvent::Trace(visitor);
}

}  // namespace blink
