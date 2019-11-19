/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_input_event.h"

#include <ctype.h>
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_keyboard_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

namespace blink {

struct SameSizeAsWebInputEvent {
  int input_data[8];
};

struct SameSizeAsWebKeyboardEvent : public SameSizeAsWebInputEvent {
  int keyboard_data[9];
};

struct SameSizeAsWebMouseEvent : public SameSizeAsWebInputEvent {
  int mouse_data[17];
};

struct SameSizeAsWebMouseWheelEvent : public SameSizeAsWebMouseEvent {
  int mousewheel_data[13];
};

struct SameSizeAsWebGestureEvent : public SameSizeAsWebInputEvent {
  int gesture_data[15];
};

struct SameSizeAsWebTouchEvent : public SameSizeAsWebInputEvent {
  WebTouchPoint touch_points[WebTouchEvent::kTouchesLengthCap];
  int touch_data[4];
};

static_assert(sizeof(WebInputEvent) == sizeof(SameSizeAsWebInputEvent),
              "WebInputEvent should not have gaps");
static_assert(sizeof(WebKeyboardEvent) == sizeof(SameSizeAsWebKeyboardEvent),
              "WebKeyboardEvent should not have gaps");
static_assert(sizeof(WebMouseEvent) == sizeof(SameSizeAsWebMouseEvent),
              "WebMouseEvent should not have gaps");
static_assert(sizeof(WebMouseWheelEvent) ==
                  sizeof(SameSizeAsWebMouseWheelEvent),
              "WebMouseWheelEvent should not have gaps");
static_assert(sizeof(WebGestureEvent) == sizeof(SameSizeAsWebGestureEvent),
              "WebGestureEvent should not have gaps");
static_assert(sizeof(WebTouchEvent) == sizeof(SameSizeAsWebTouchEvent),
              "WebTouchEvent should not have gaps");

}  // namespace blink
