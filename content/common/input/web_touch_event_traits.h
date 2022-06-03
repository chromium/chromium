// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_WEB_TOUCH_EVENT_TRAITS_H_
#define CONTENT_COMMON_INPUT_WEB_TOUCH_EVENT_TRAITS_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

namespace blink {
class WebTouchEvent;
}

namespace content {

// Utility class for performing operations on and with WebTouchEvents.
class CONTENT_EXPORT WebTouchEventTraits {
 public:
  // Returns whether all touches in the event have the specified state.
  static bool AllTouchPointsHaveState(const blink::WebTouchEvent& event,
                                      blink::WebTouchPoint::State state);

  // Returns whether this event represents a transition from no active
  // touches to some active touches (the start of a new "touch sequence").
  static bool IsTouchSequenceStart(const blink::WebTouchEvent& event);

  // Returns whether this event represents a transition from active
  // touches to no active touches (the end of a "touch sequence").
  static bool IsTouchSequenceEnd(const blink::WebTouchEvent& event);

  // Sets the type of |event| to |type|, resetting any other type-specific
  // properties and updating the timestamp.
  static void ResetType(blink::WebInputEvent::Type type,
                        base::TimeTicks timestamp,
                        blink::WebTouchEvent* event);

  // Like ResetType but also resets the state of all active touches
  // to match the event type.  This is particularly useful, for example,
  // in sending a touchcancel for all active touches.
  static void ResetTypeAndTouchStates(blink::WebInputEvent::Type type,
                                      base::TimeTicks timestamp,
                                      blink::WebTouchEvent* event);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_WEB_TOUCH_EVENT_TRAITS_H_
