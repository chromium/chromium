/*
 * Copyright 2008, The Android Open Source Project
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/events/touch_event.h"

#include "third_party/blink/public/platform/web_coalesced_input_event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/intervention.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

// Helper function to get WebTouchEvent from WebCoalescedInputEvent.
const WebTouchEvent* GetWebTouchEvent(const WebCoalescedInputEvent& event) {
  return static_cast<const WebTouchEvent*>(&event.Event());
}
}  // namespace

TouchEvent::TouchEvent()
    : current_touch_action_(TouchAction::kTouchActionAuto) {}

TouchEvent::TouchEvent(const WebCoalescedInputEvent& event,
                       TouchList* touches,
                       TouchList* target_touches,
                       TouchList* changed_touches,
                       const AtomicString& type,
                       AbstractView* view,
                       TouchAction current_touch_action)
    // Pass a sourceCapabilities including the ability to fire touchevents when
    // creating this touchevent, which is always created from input device
    // capabilities from EventHandler.
    : UIEventWithKeyState(
          type,
          Bubbles::kYes,
          GetWebTouchEvent(event)->IsCancelable() ? Cancelable::kYes
                                                  : Cancelable::kNo,
          view,
          0,
          static_cast<WebInputEvent::Modifiers>(event.Event().GetModifiers()),
          event.Event().TimeStamp(),
          view ? view->GetInputDeviceCapabilities()->FiresTouchEvents(true)
               : nullptr),
      touches_(touches),
      target_touches_(target_touches),
      changed_touches_(changed_touches),
      current_touch_action_(current_touch_action) {
  DCHECK(WebInputEvent::IsTouchEventType(event.Event().GetType()));
  native_event_.reset(new WebCoalescedInputEvent(event));
}

TouchEvent::TouchEvent(const AtomicString& type,
                       const TouchEventInit* initializer)
    : UIEventWithKeyState(type, initializer),
      touches_(TouchList::Create(initializer->touches())),
      target_touches_(TouchList::Create(initializer->targetTouches())),
      changed_touches_(TouchList::Create(initializer->changedTouches())),
      current_touch_action_(TouchAction::kTouchActionAuto) {}

TouchEvent::~TouchEvent() = default;

const AtomicString& TouchEvent::InterfaceName() const {
  return event_interface_names::kTouchEvent;
}

bool TouchEvent::IsTouchEvent() const {
  return true;
}

void TouchEvent::preventDefault() {
  UIEventWithKeyState::preventDefault();

  // A common developer error is to wait too long before attempting to stop
  // scrolling by consuming a touchmove event. Generate an error if this
  // event is uncancelable.
  String id;
  String message;
  switch (HandlingPassive()) {
    case PassiveMode::kNotPassive:
    case PassiveMode::kNotPassiveDefault:
      if (!cancelable()) {
        id = "IgnoredEventCancel";
        message = "Ignored attempt to cancel a " + type() +
                  " event with cancelable=false, for example "
                  "because scrolling is in progress and "
                  "cannot be interrupted.";
      }
      break;
    case PassiveMode::kPassiveForcedDocumentLevel:
      // Only enable the warning when the current touch action is auto because
      // an author may use touch action but call preventDefault for interop with
      // browsers that don't support touch-action.
      if (current_touch_action_ == TouchAction::kTouchActionAuto) {
        id = "PreventDefaultPassive";
        message =
            "Unable to preventDefault inside passive event listener due to "
            "target being treated as passive. See "
            "https://www.chromestatus.com/features/5093566007214080";
      }
      break;
    default:
      break;
  }

  auto* local_dom_window = DynamicTo<LocalDOMWindow>(view());
  if (!message.IsEmpty() && local_dom_window && local_dom_window->GetFrame()) {
    Intervention::GenerateReport(local_dom_window->GetFrame(), id, message);
  }

  if ((type() == event_type_names::kTouchstart ||
       type() == event_type_names::kTouchmove) &&
      local_dom_window) {
    auto* local_frame = DynamicTo<LocalFrame>(view()->GetFrame());
    if (local_frame && current_touch_action_ == TouchAction::kTouchActionAuto) {
      switch (HandlingPassive()) {
        case PassiveMode::kNotPassiveDefault:
          UseCounter::Count(local_dom_window->document(),
                            WebFeature::kTouchEventPreventedNoTouchAction);
          break;
        case PassiveMode::kPassiveForcedDocumentLevel:
          UseCounter::Count(
              local_dom_window->document(),
              WebFeature::
                  kTouchEventPreventedForcedDocumentPassiveNoTouchAction);
          break;
        default:
          break;
      }
    }
  }
}

bool TouchEvent::IsTouchStartOrFirstTouchMove() const {
  if (!native_event_)
    return false;
  return GetWebTouchEvent(*native_event_)->touch_start_or_first_touch_move;
}

void TouchEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(touches_);
  visitor->Trace(target_touches_);
  visitor->Trace(changed_touches_);
  UIEventWithKeyState::Trace(visitor);
}

DispatchEventResult TouchEvent::DispatchEvent(EventDispatcher& dispatcher) {
  GetEventPath().AdjustForTouchEvent(*this);
  return dispatcher.Dispatch();
}

}  // namespace blink
