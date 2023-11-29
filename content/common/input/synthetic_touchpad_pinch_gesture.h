// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_TOUCHPAD_PINCH_GESTURE_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_TOUCHPAD_PINCH_GESTURE_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {

class CONTENT_EXPORT SyntheticTouchpadPinchGesture
    : public SyntheticGestureBase<SyntheticPinchGestureParams> {
 public:
  explicit SyntheticTouchpadPinchGesture(
      const SyntheticPinchGestureParams& params);

  SyntheticTouchpadPinchGesture(const SyntheticTouchpadPinchGesture&) = delete;
  SyntheticTouchpadPinchGesture& operator=(
      const SyntheticTouchpadPinchGesture&) = delete;

  ~SyntheticTouchpadPinchGesture() override;

  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;
  void WaitForTargetAck(base::OnceClosure callback,
                        SyntheticGestureTarget* target) const override;

 private:
  enum GestureState { SETUP, STARTED, IN_PROGRESS, DONE };

  void ForwardGestureEvents(const base::TimeTicks& timestamp,
                            SyntheticGestureTarget* target);

  void UpdateTouchPoints(const base::TimeTicks& timestamp);

  void CalculateEndTime(SyntheticGestureTarget* target);
  float CalculateTargetScale(const base::TimeTicks& timestamp) const;
  base::TimeTicks ClampTimestamp(const base::TimeTicks& timestamp) const;
  bool HasReachedTarget(const base::TimeTicks& timestamp) const;

  content::mojom::GestureSourceType gesture_source_type_;
  GestureState state_;
  base::TimeTicks start_time_;
  base::TimeTicks stop_time_;
  float current_scale_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_TOUCHPAD_PINCH_GESTURE_H_
