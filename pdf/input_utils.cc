// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/input_utils.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"

namespace chrome_pdf {

blink::WebMouseEvent NormalizeMouseEvent(const blink::WebMouseEvent& event) {
  blink::WebMouseEvent normalized_event = event;
#if BUILDFLAG(IS_MAC)
  using Modifiers = blink::WebInputEvent::Modifiers;
  if ((event.GetModifiers() & Modifiers::kControlKey) &&
      event.button == blink::WebPointerProperties::Button::kLeft &&
      event.GetType() == blink::WebInputEvent::Type::kMouseDown) {
    constexpr int kUnsetModifiers =
        Modifiers::kControlKey | Modifiers::kLeftButtonDown;
    normalized_event.SetModifiers((event.GetModifiers() & ~kUnsetModifiers) |
                                  Modifiers::kRightButtonDown);
    normalized_event.button = blink::WebPointerProperties::Button::kRight;
  }
#endif
  return normalized_event;
}

}  // namespace chrome_pdf
