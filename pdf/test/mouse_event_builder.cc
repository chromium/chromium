// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/mouse_event_builder.h"

#include "base/check.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

MouseEventBuilder::MouseEventBuilder() = default;

MouseEventBuilder::~MouseEventBuilder() = default;

blink::WebMouseEvent MouseEventBuilder::Build() const {
  CHECK(blink::WebInputEvent::IsMouseEventType(type_));
  int actual_modifiers = modifiers_;
  if (type_ == blink::WebInputEvent::Type::kMouseDown) {
    switch (button_) {
      case blink::WebPointerProperties::Button::kLeft:
        actual_modifiers |= blink::WebInputEvent::Modifiers::kLeftButtonDown;
        break;
      case blink::WebPointerProperties::Button::kMiddle:
        actual_modifiers |= blink::WebInputEvent::Modifiers::kMiddleButtonDown;
        break;
      case blink::WebPointerProperties::Button::kRight:
        actual_modifiers |= blink::WebInputEvent::Modifiers::kRightButtonDown;
        break;
      default:
        NOTREACHED();  // Implement more `button_` types as-needed.
    }
  }
  return blink::WebMouseEvent(
      type_, /*position=*/position_,
      /*global_position=*/position_, button_, click_count_, actual_modifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
}

}  // namespace chrome_pdf
