// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_TIMEOUT_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_TIMEOUT_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"

namespace content {

class PassthroughTouchEventQueue;

class TouchTimeoutHandler {
 public:
  TouchTimeoutHandler(PassthroughTouchEventQueue* touch_queue,
                      base::TimeDelta desktop_timeout_delay,
                      base::TimeDelta mobile_timeout_delay);

  ~TouchTimeoutHandler();

  void StartIfNecessary(const TouchEventWithLatencyInfo& event);
  bool ConfirmTouchEvent(uint32_t unique_touch_event_id,
                         blink::mojom::InputEventResultState ack_result,
                         bool should_stop_timeout_monitor);
  bool FilterEvent(const blink::WebTouchEvent& event);
  void SetEnabled(bool enabled);
  void SetUseMobileTimeout(bool use_mobile_timeout);
  void StopTimeoutMonitor();
  bool IsTimeoutTimerRunning() const { return timeout_monitor_.IsRunning(); }
  bool IsEnabled() const { return enabled_ && !GetTimeoutDelay().is_zero(); }

 private:
  enum PendingAckState {
    PENDING_ACK_NONE,
    PENDING_ACK_ORIGINAL_EVENT,
    PENDING_ACK_CANCEL_EVENT,
  };

  void OnTimeOut();
  // Skip a cancel event if the timed-out event had no consumer and was the
  // initial event in the gesture.
  bool AckedTimeoutEventRequiresCancel(
      blink::mojom::InputEventResultState ack_result) const;
  void SetPendingAckState(PendingAckState new_pending_ack_state);
  void LogSequenceStartForUMA();
  void LogSequenceEndForUMAIfNecessary(bool timed_out);
  base::TimeDelta GetTimeoutDelay() const;
  bool HasTimeoutEvent() const;

  PassthroughTouchEventQueue* touch_queue_;

  // How long to wait on a touch ack before cancelling the touch sequence.
  const base::TimeDelta desktop_timeout_delay_;
  const base::TimeDelta mobile_timeout_delay_;
  bool use_mobile_timeout_;

  // The touch event source for which we expect the next ack.
  PendingAckState pending_ack_state_;

  // The event for which the ack timeout is triggered.
  TouchEventWithLatencyInfo timeout_event_;

  // Provides timeout-based callback behavior.
  TimeoutMonitor timeout_monitor_;

  bool enabled_;
  bool enabled_for_current_sequence_;

  // Bookkeeping to classify and log whether a touch sequence times out.
  bool sequence_awaiting_uma_update_;
  bool sequence_using_mobile_timeout_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_TIMEOUT_HANDLER_H_
