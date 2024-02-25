// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_TOUCHSCREEN_PINCH_GESTURE_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_TOUCHSCREEN_PINCH_GESTURE_H_

#include <memory>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_pointer_driver.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {

class CONTENT_EXPORT SyntheticTouchscreenPinchGesture
    : public SyntheticGestureBase<SyntheticPinchGestureParams> {
 public:
  explicit SyntheticTouchscreenPinchGesture(
      const SyntheticPinchGestureParams& params);

  SyntheticTouchscreenPinchGesture(const SyntheticTouchscreenPinchGesture&) =
      delete;
  SyntheticTouchscreenPinchGesture& operator=(
      const SyntheticTouchscreenPinchGesture&) = delete;

  ~SyntheticTouchscreenPinchGesture() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;
  void WaitForTargetAck(base::OnceClosure callback,
                        SyntheticGestureTarget* target) const override;

 private:
  enum GestureState { SETUP, STARTED, MOVING, DONE };

  void ForwardTouchInputEvents(const base::TimeTicks& timestamp,
                               SyntheticGestureTarget* target);

  void PressTouchPoints(SyntheticGestureTarget* target,
                        const base::TimeTicks& timestamp);
  void MoveTouchPoints(SyntheticGestureTarget* target,
                       float delta,
                       const base::TimeTicks& timestamp);
  void ReleaseTouchPoints(SyntheticGestureTarget* target,
                          const base::TimeTicks& timestamp);

  void SetupCoordinatesAndStopTime(SyntheticGestureTarget* target);
  float GetDeltaForPointer0AtTime(const base::TimeTicks& timestamp) const;
  base::TimeTicks ClampTimestamp(const base::TimeTicks& timestamp) const;
  bool HasReachedTarget(const base::TimeTicks& timestamp) const;

  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  float start_y_0_;
  float start_y_1_;
  float max_pointer_delta_0_;
  content::mojom::GestureSourceType gesture_source_type_;
  GestureState state_;
  base::TimeTicks start_time_;
  base::TimeTicks stop_time_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_TOUCHSCREEN_PINCH_GESTURE_H_
