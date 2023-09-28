// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DISPOSITION_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DISPOSITION_HANDLER_H_

#include "content/common/input/event_with_latency_info.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"

namespace content {

// Provided customized disposition response for input events.
class InputDispositionHandler {
 public:
  virtual ~InputDispositionHandler() {}

  // Called upon event ack receipt from the renderer.
  virtual void OnWheelEventAck(
      const MouseWheelEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
  virtual void OnTouchEventAck(
      const TouchEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) = 0;
  virtual void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result,
      blink::mojom::ScrollResultDataPtr scroll_result_data) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DISPOSITION_HANDLER_H_
