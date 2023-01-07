// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_

#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/tap_suppression_controller.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {

// Controls the suppression of touchpad taps immediately following the dispatch
// of a GestureFlingCancel event.
class TouchpadTapSuppressionController : public TapSuppressionController {
 public:
  // The |client| must outlive the TouchpadTapSupressionController.
  TouchpadTapSuppressionController(
      const TapSuppressionController::Config& config);

  TouchpadTapSuppressionController(const TouchpadTapSuppressionController&) =
      delete;
  TouchpadTapSuppressionController& operator=(
      const TouchpadTapSuppressionController&) = delete;

  ~TouchpadTapSuppressionController() override;

  // Should be called on arrival of MouseDown events. Returns true if the caller
  // should stop normal handling of the MouseDown.
  bool ShouldSuppressMouseDown(const MouseEventWithLatencyInfo& event);

  // Should be called on arrival of MouseUp events. Returns true if the caller
  // should stop normal handling of the MouseUp.
  bool ShouldSuppressMouseUp();

 private:
  friend class MockRenderWidgetHost;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCHPAD_TAP_SUPPRESSION_CONTROLLER_H_
