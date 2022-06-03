// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/web_touch_event_traits.h"

#include <stddef.h>

#include "base/check.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

bool WebTouchEventTraits::AllTouchPointsHaveState(
    const WebTouchEvent& event,
    blink::WebTouchPoint::State state) {
  if (!event.touches_length)
    return false;
  for (size_t i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].state != state)
      return false;
  }
  return true;
}

bool WebTouchEventTraits::IsTouchSequenceStart(const WebTouchEvent& event) {
  DCHECK(event.touches_length ||
         event.GetType() == WebInputEvent::Type::kTouchScrollStarted);
  if (event.GetType() != WebInputEvent::Type::kTouchStart)
    return false;
  return AllTouchPointsHaveState(event,
                                 blink::WebTouchPoint::State::kStatePressed);
}

bool WebTouchEventTraits::IsTouchSequenceEnd(const WebTouchEvent& event) {
  if (event.GetType() != WebInputEvent::Type::kTouchEnd &&
      event.GetType() != WebInputEvent::Type::kTouchCancel)
    return false;
  if (!event.touches_length)
    return true;
  for (size_t i = 0; i < event.touches_length; ++i) {
    if (event.touches[i].state != blink::WebTouchPoint::State::kStateReleased &&
        event.touches[i].state != blink::WebTouchPoint::State::kStateCancelled)
      return false;
  }
  return true;
}

void WebTouchEventTraits::ResetType(WebInputEvent::Type type,
                                    base::TimeTicks timestamp,
                                    WebTouchEvent* event) {
  DCHECK(WebInputEvent::IsTouchEventType(type));
  DCHECK(type != WebInputEvent::Type::kTouchScrollStarted);

  event->SetType(type);
  event->dispatch_type = type == WebInputEvent::Type::kTouchCancel
                             ? WebInputEvent::DispatchType::kEventNonBlocking
                             : WebInputEvent::DispatchType::kBlocking;
  event->SetTimeStamp(timestamp);
}

void WebTouchEventTraits::ResetTypeAndTouchStates(WebInputEvent::Type type,
                                                  base::TimeTicks timestamp,
                                                  WebTouchEvent* event) {
  ResetType(type, timestamp, event);

  WebTouchPoint::State newState = WebTouchPoint::State::kStateUndefined;
  switch (event->GetType()) {
    case WebInputEvent::Type::kTouchStart:
      newState = WebTouchPoint::State::kStatePressed;
      break;
    case WebInputEvent::Type::kTouchMove:
      newState = WebTouchPoint::State::kStateMoved;
      break;
    case WebInputEvent::Type::kTouchEnd:
      newState = WebTouchPoint::State::kStateReleased;
      break;
    case WebInputEvent::Type::kTouchCancel:
      newState = WebTouchPoint::State::kStateCancelled;
      break;
    default:
      NOTREACHED();
      break;
  }
  for (size_t i = 0; i < event->touches_length; ++i)
    event->touches[i].state = newState;
}

}  // namespace content
