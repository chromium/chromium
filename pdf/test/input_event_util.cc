// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/input_event_util.h"

#include "pdf/test/mouse_event_builder.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

blink::WebMouseEvent CreateLeftClickWebMouseEventAtPosition(
    const gfx::PointF& position) {
  return MouseEventBuilder().CreateLeftClickAtPosition(position).Build();
}

blink::WebMouseEvent CreateLeftClickWebMouseMoveEventAtPosition(
    const gfx::PointF& point) {
  return MouseEventBuilder()
      .SetType(blink::WebInputEvent::Type::kMouseMove)
      .SetPosition(point)
      .SetButton(blink::WebPointerProperties::Button::kLeft)
      .Build();
}

blink::WebMouseEvent CreateLeftClickWebMouseUpEventAtPosition(
    const gfx::PointF& position) {
  return MouseEventBuilder().CreateLeftMouseUpAtPosition(position).Build();
}

blink::WebMouseEvent CreateMoveWebMouseEventToPosition(
    const gfx::PointF& position) {
  return MouseEventBuilder()
      .SetType(blink::WebInputEvent::Type::kMouseMove)
      .SetPosition(position)
      .Build();
}

}  // namespace chrome_pdf
