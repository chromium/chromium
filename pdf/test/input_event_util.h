// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_INPUT_EVENT_UTIL_H_
#define PDF_TEST_INPUT_EVENT_UTIL_H_

#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace gfx {
class PointF;
}

namespace chrome_pdf {

// A set of utility functions to create commonly used input events.
// To build custom events, use MouseEventBuilder or instantiate the event
// directly.

// Left button down at `position`.
blink::WebMouseEvent CreateLeftClickWebMouseEventAtPosition(
    const gfx::PointF& position);

// Move to `position` with the left button down.
blink::WebMouseEvent CreateLeftClickWebMouseMoveEventAtPosition(
    const gfx::PointF& position);

// Left button up at `position`.
blink::WebMouseEvent CreateLeftClickWebMouseUpEventAtPosition(
    const gfx::PointF& position);

// Move to `position` with no associated button.
blink::WebMouseEvent CreateMoveWebMouseEventToPosition(
    const gfx::PointF& position);

}  // namespace chrome_pdf

#endif  // PDF_TEST_INPUT_EVENT_UTIL_H_
