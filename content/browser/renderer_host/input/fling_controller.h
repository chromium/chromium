// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_CONTROLLER_H_

#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"
#include "content/public/common/input_event_ack_state.h"

namespace blink {
class WebGestureCurve;
}

namespace ui {
class FlingBooster;
}

namespace content {

class FlingController;

// Interface with which the FlingController can forward generated fling progress
// events.
class CONTENT_EXPORT FlingControllerEventSenderClient {
 public:
  virtual ~FlingControllerEventSenderClient() {}

  virtual void SendGeneratedWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) = 0;

  virtual void SendGeneratedGestureScrollEvents(
      const GestureEventWithLatencyInfo& gesture_event) = 0;
};

// Interface with which the fling progress gets scheduled.
class CONTENT_EXPORT FlingControllerSchedulerClient {
 public:
  virtual ~FlingControllerSchedulerClient() {}

  virtual void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) = 0;

  virtual void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) = 0;

  virtual bool NeedsBeginFrameForFlingProgress() = 0;
};

class CONTENT_EXPORT FlingController {
 public:
  struct CONTENT_EXPORT Config {
    Config();

    // Controls touchpad-related tap suppression, disabled by default.
    TapSuppressionController::Config touchpad_tap_suppression_config;

    // Controls touchscreen-related tap suppression, disabled by default.
    TapSuppressionController::Config touchscreen_tap_suppression_config;
  };

  struct ActiveFlingParameters {
    gfx::Vector2dF velocity;
    gfx::PointF point;
    gfx::PointF global_point;
    int modifiers;
    blink::WebGestureDevice source_device;
    base::TimeTicks start_time;

    ActiveFlingParameters() : modifiers(0) {}
  };

  FlingController(FlingControllerEventSenderClient* event_sender_client,
                  FlingControllerSchedulerClient* scheduler_client,
                  const Config& config);

  ~FlingController();

  // Used to progress an active fling on every begin frame.
  void ProgressFling(base::TimeTicks current_time);

  // Used to halt an active fling progress whenever needed.
  void StopFling();

  bool FilterGestureEvent(const GestureEventWithLatencyInfo& gesture_event);

  void OnGestureEventAck(const GestureEventWithLatencyInfo& acked_event,
                         InputEventAckState ack_result);

  void ProcessGestureFlingStart(
      const GestureEventWithLatencyInfo& gesture_event);

  void ProcessGestureFlingCancel(
      const GestureEventWithLatencyInfo& gesture_event);

  bool fling_in_progress() const { return fling_in_progress_; }

  bool FlingCancellationIsDeferred() const;

  gfx::Vector2dF CurrentFlingVelocity() const;

  // Returns the |TouchpadTapSuppressionController| instance.
  TouchpadTapSuppressionController* GetTouchpadTapSuppressionController();

  void set_clock_for_testing(const base::TickClock* clock) { clock_ = clock; }

 protected:
  std::unique_ptr<ui::FlingBooster> fling_booster_;

 private:
  // Sub-filter for removing unnecessary GestureFlingCancels.
  bool ShouldForwardForGFCFiltering(
      const GestureEventWithLatencyInfo& gesture_event) const;

  // Sub-filter for suppressing taps immediately after a GestureFlingCancel.
  bool ShouldForwardForTapSuppression(
      const GestureEventWithLatencyInfo& gesture_event);

  // Sub-filter for suppressing gesture events to boost an active fling whenever
  // possible.
  bool FilterGestureEventForFlingBoosting(
      const GestureEventWithLatencyInfo& gesture_event);

  void ScheduleFlingProgress();

  // Used to generate synthetic wheel events from touchpad fling and send them.
  void GenerateAndSendWheelEvents(const gfx::Vector2dF& delta,
                                  blink::WebMouseWheelEvent::Phase phase);

  // Used to generate synthetic gesture scroll events from touchscreen fling and
  // send them.
  void GenerateAndSendGestureScrollEvents(
      blink::WebInputEvent::Type type,
      const gfx::Vector2dF& delta = gfx::Vector2dF());

  // Calls one of the GenerateAndSendWheelEvents or
  // GenerateAndSendGestureScrollEvents functions depending on the source
  // device of the current_fling_parameters_. We send GSU and wheel events
  // to progress flings with touchscreen and touchpad source respectively.
  // The reason for this difference is that during the touchpad fling we still
  // send wheel events to JS and generating GSU events directly is not enough.
  void GenerateAndSendFlingProgressEvents(const gfx::Vector2dF& delta);

  void GenerateAndSendFlingEndEvents();

  void CancelCurrentFling();

  bool UpdateCurrentFlingState(const blink::WebGestureEvent& fling_start_event,
                               const gfx::Vector2dF& velocity);

  FlingControllerEventSenderClient* event_sender_client_;

  FlingControllerSchedulerClient* scheduler_client_;

  // An object tracking the state of touchpad on the delivery of mouse events to
  // the renderer to filter mouse immediately after a touchpad fling canceling
  // tap.
  TouchpadTapSuppressionController touchpad_tap_suppression_controller_;

  // An object tracking the state of touchscreen on the delivery of gesture tap
  // events to the renderer to filter taps immediately after a touchscreen fling
  // canceling tap.
  TouchscreenTapSuppressionController touchscreen_tap_suppression_controller_;

  // Gesture curve of the current active fling.
  std::unique_ptr<blink::WebGestureCurve> fling_curve_;

  ActiveFlingParameters current_fling_parameters_;

  // True when a fling is active.
  bool fling_in_progress_;

  // Whether an active fling has seen a |ProgressFling()| call. This is useful
  // for determining if the fling start time should be re-initialized.
  bool has_fling_animation_started_;

  // The clock used; overridable for tests.
  const base::TickClock* clock_;

  base::WeakPtrFactory<FlingController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FlingController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_FLING_CONTROLLER_H_
