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

#include "third_party/blink/renderer/bindings/core/v8/v8_wheel_event_init.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

unsigned ConvertDeltaMode(const WebMouseWheelEvent& event) {
  // WebMouseWheelEvent only supports these units for the delta.
  DCHECK(event.delta_units == ui::ScrollGranularity::kScrollByPage ||
         event.delta_units == ui::ScrollGranularity::kScrollByPixel ||
         event.delta_units == ui::ScrollGranularity::kScrollByPrecisePixel);
  return event.delta_units == ui::ScrollGranularity::kScrollByPage
             ? WheelEvent::kDomDeltaPage
             : WheelEvent::kDomDeltaPixel;
}

MouseEventInit* GetMouseEventInitForWheel(const WebMouseWheelEvent& event,
                                          LocalDOMWindow& window) {
  MouseEventInit* initializer = MouseEventInit::Create();
  initializer->setBubbles(true);
  initializer->setCancelable(event.IsCancelable());
  MouseEvent::SetCoordinatesFromWebPointerProperties(event.FlattenTransform(),
                                                     &window, initializer);
  initializer->setButton(static_cast<int16_t>(event.button));
  initializer->setButtons(
      MouseEvent::WebInputEventModifiersToButtons(event.GetModifiers()));
  initializer->setView(&window);
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
                               LocalDOMWindow& window) {
  return MakeGarbageCollected<WheelEvent>(event, window);
}

WheelEvent* WheelEvent::Create(const WebMouseWheelEvent& event,
                               const gfx::Vector2dF& delta_in_pixels,
                               LocalDOMWindow& window) {
  return MakeGarbageCollected<WheelEvent>(event, delta_in_pixels, window);
}

WheelEvent::WheelEvent()
    : delta_x_(0), delta_y_(0), delta_z_(0), delta_mode_(kDomDeltaPixel) {}

// crbug.com/1173525: tweak the initialization behavior.
WheelEvent::WheelEvent(const AtomicString& type,
                       const WheelEventInit* initializer)
    : MouseEvent(type, initializer),
      wheel_delta_(
          initializer->wheelDeltaX() ? initializer->wheelDeltaX()
                                     : ClampTo<int32_t>(initializer->deltaX()),
          initializer->wheelDeltaY() ? initializer->wheelDeltaY()
                                     : ClampTo<int32_t>(initializer->deltaY())),
      delta_x_(initializer->deltaX() ? initializer->deltaX()
                                     : ClampTo<int32_t>(-static_cast<double>(
                                           initializer->wheelDeltaX()))),
      delta_y_(initializer->deltaY() ? initializer->deltaY()
                                     : ClampTo<int32_t>(-static_cast<double>(
                                           initializer->wheelDeltaY()))),
      delta_z_(initializer->deltaZ()),
      delta_mode_(initializer->deltaMode()) {}

WheelEvent::WheelEvent(const WebMouseWheelEvent& event, LocalDOMWindow& window)
    : MouseEvent(event_type_names::kWheel,
                 GetMouseEventInitForWheel(event, window),
                 event.TimeStamp()),
      wheel_delta_(
          (event.wheel_ticks_x * kTickMultiplier) / window.devicePixelRatio(),
          (event.wheel_ticks_y * kTickMultiplier) / window.devicePixelRatio()),
      delta_x_(-event.DeltaXInRootFrame() / window.devicePixelRatio()),
      delta_y_(-event.DeltaYInRootFrame() / window.devicePixelRatio()),
      delta_z_(0),
      delta_mode_(ConvertDeltaMode(event)),
      native_event_(event) {}

WheelEvent::WheelEvent(const WebMouseWheelEvent& event,
                       const gfx::Vector2dF& delta_in_pixels,
                       LocalDOMWindow& window)
    : MouseEvent(event_type_names::kWheel,
                 GetMouseEventInitForWheel(event, window),
                 event.TimeStamp()),
      wheel_delta_(event.wheel_ticks_x * kTickMultiplier,
                   event.wheel_ticks_y * kTickMultiplier),
      delta_x_(delta_in_pixels.x()),
      delta_y_(delta_in_pixels.y()),
      delta_z_(0),
      delta_mode_(WheelEvent::kDomDeltaPixel),
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
        "https://www.chromestatus.com/feature/6662647093133312";
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

void WheelEvent::Trace(Visitor* visitor) const {
  MouseEvent::Trace(visitor);
}

}  // namespace blink
