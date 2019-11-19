// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DISPOSITION_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DISPOSITION_HANDLER_H_

#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/input_event_ack_source.h"
#include "content/public/common/input_event_ack_state.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {

// Provided customized disposition response for input events.
class CONTENT_EXPORT InputDispositionHandler {
 public:
  virtual ~InputDispositionHandler() {}

  // Called upon event ack receipt from the renderer.
  virtual void OnWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                               InputEventAckSource ack_source,
                               InputEventAckState ack_result) = 0;
  virtual void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                               InputEventAckSource ack_source,
                               InputEventAckState ack_result) = 0;
  virtual void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                                 InputEventAckSource ack_source,
                                 InputEventAckState ack_result) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DISPOSITION_HANDLER_H_
