/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/events/gesture_event.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

GestureEvent* GestureEvent::Create(AbstractView* view,
                                   const WebGestureEvent& event) {
  AtomicString event_type;

  switch (event.GetType()) {
    case WebInputEvent::Type::kGestureTap:
      event_type = event_type_names::kGesturetap;
      break;
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      event_type = event_type_names::kGesturetapunconfirmed;
      break;
    case WebInputEvent::Type::kGestureTapDown:
      event_type = event_type_names::kGesturetapdown;
      break;
    case WebInputEvent::Type::kGestureShowPress:
      event_type = event_type_names::kGestureshowpress;
      break;
    case WebInputEvent::Type::kGestureLongPress:
      event_type = event_type_names::kGesturelongpress;
      break;
    case WebInputEvent::Type::kGestureFlingStart:
      event_type = event_type_names::kGestureflingstart;
      break;
    case WebInputEvent::Type::kGestureTwoFingerTap:
    case WebInputEvent::Type::kGesturePinchBegin:
    case WebInputEvent::Type::kGesturePinchEnd:
    case WebInputEvent::Type::kGesturePinchUpdate:
    case WebInputEvent::Type::kGestureTapCancel:
    default:
      return nullptr;
  }
  return MakeGarbageCollected<GestureEvent>(event_type, view, event);
}

GestureEvent::GestureEvent(const AtomicString& event_type,
                           AbstractView* view,
                           const WebGestureEvent& event)
    : UIEventWithKeyState(
          event_type,
          Bubbles::kYes,
          Cancelable::kYes,
          view,
          0,
          static_cast<WebInputEvent::Modifiers>(event.GetModifiers()),
          event.TimeStamp(),
          nullptr),
      native_event_(event) {}

const AtomicString& GestureEvent::InterfaceName() const {
  // FIXME: when a GestureEvent.idl interface is defined, return the string
  // "GestureEvent".  Until that happens, do not advertise an interface that
  // does not exist, since it will trip up the bindings integrity checks.
  return UIEvent::InterfaceName();
}

bool GestureEvent::IsGestureEvent() const {
  return true;
}

void GestureEvent::Trace(Visitor* visitor) const {
  UIEvent::Trace(visitor);
}

}  // namespace blink
