// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_DOM_WINDOW_GAMEPAD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_DOM_WINDOW_GAMEPAD_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

class DOMWindowGamepad {
 public:
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(gamepadconnected, kGamepadconnected)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(gamepaddisconnected,
                                         kGamepaddisconnected)
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(gamepadrawinputchanged,
                                         kGamepadrawinputchanged)
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_DOM_WINDOW_GAMEPAD_H_
