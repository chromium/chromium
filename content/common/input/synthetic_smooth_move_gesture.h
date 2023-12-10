// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_MOVE_GESTURE_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_MOVE_GESTURE_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/input/synthetic_gesture.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_gesture_target.h"
#include "content/common/input/synthetic_pointer_driver.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace content {

struct CONTENT_EXPORT SyntheticSmoothMoveGestureParams
    : public SyntheticGestureParams {
 public:
  SyntheticSmoothMoveGestureParams();
  SyntheticSmoothMoveGestureParams(
      const SyntheticSmoothMoveGestureParams& other);
  ~SyntheticSmoothMoveGestureParams() override;

  GestureType GetGestureType() const override;

  enum InputType { MOUSE_DRAG_INPUT, MOUSE_WHEEL_INPUT, TOUCH_INPUT };

  InputType input_type;
  gfx::PointF start_point;
  std::vector<gfx::Vector2dF> distances;
  int speed_in_pixels_s = SyntheticGestureParams::kDefaultSpeedInPixelsPerSec;
  int fling_velocity_x = 0;
  int fling_velocity_y = 0;
  bool prevent_fling = true;
  bool add_slop = true;
  ui::ScrollGranularity granularity = ui::ScrollGranularity::kScrollByPixel;
  // A bitfield of values from blink::WebInputEvent::Modifiers.
  int modifiers = 0;
};

// This class is used as helper class for simulation of scroll and drag.
// Simulates scrolling/dragging given a sequence of distances as a continuous
// gestures (i.e. when synthesizing touch or mouse drag events, the pointer is
// not lifted when changing scroll direction).
// If no distance is provided or the first one is 0, no touch events are
// generated.
// When synthesizing touch events for scrolling, the first distance is extended
// to compensate for the touch slop.
class CONTENT_EXPORT SyntheticSmoothMoveGesture
    : public SyntheticGestureBase<SyntheticSmoothMoveGestureParams> {
 public:
  explicit SyntheticSmoothMoveGesture(
      const SyntheticSmoothMoveGestureParams& gesture_params);

  SyntheticSmoothMoveGesture(const SyntheticSmoothMoveGesture&) = delete;
  SyntheticSmoothMoveGesture& operator=(const SyntheticSmoothMoveGesture&) =
      delete;

  ~SyntheticSmoothMoveGesture() override;

  // SyntheticGesture implementation:
  SyntheticGesture::Result ForwardInputEvents(
      const base::TimeTicks& timestamp,
      SyntheticGestureTarget* target) override;

 private:
  enum GestureState {
    SETUP,
    STARTED,
    MOVING,
    STOPPING,
    DONE
  };

  void ForwardTouchInputEvents(
      const base::TimeTicks& timestamp, SyntheticGestureTarget* target);
  void ForwardMouseWheelInputEvents(
      const base::TimeTicks& timestamp, SyntheticGestureTarget* target);
  void ForwardMouseClickInputEvents(
      const base::TimeTicks& timestamp, SyntheticGestureTarget* target);

  void ForwardMouseWheelEvent(SyntheticGestureTarget* target,
                              const gfx::Vector2dF& delta,
                              const blink::WebMouseWheelEvent::Phase phase,
                              const base::TimeTicks& timestamp,
                              int modifiers) const;

  void ForwardFlingGestureEvent(SyntheticGestureTarget* target,
                                const blink::WebInputEvent::Type type) const;

  void PressPoint(SyntheticGestureTarget* target,
                  const base::TimeTicks& timestamp);
  void MovePoint(SyntheticGestureTarget* target,
                 const gfx::Vector2dF& delta,
                 const base::TimeTicks& timestamp);
  void ReleasePoint(SyntheticGestureTarget* target,
                    const base::TimeTicks& timestamp);

  void AddTouchSlopToFirstDistance(SyntheticGestureTarget* target);
  gfx::Vector2dF GetPositionDeltaAtTime(const base::TimeTicks& timestamp) const;
  void ComputeNextMoveSegment();
  base::TimeTicks ClampTimestamp(const base::TimeTicks& timestamp) const;
  bool FinishedCurrentMoveSegment(const base::TimeTicks& timestamp) const;
  bool IsLastMoveSegment() const;
  bool MoveIsNoOp() const;

  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  // Used for mouse input.
  gfx::Vector2dF current_move_segment_total_delta_;
  // Used for touch input.
  gfx::PointF current_move_segment_start_position_;
  GestureState state_ = SETUP;
  int current_move_segment_;
  base::TimeTicks current_move_segment_start_time_;
  base::TimeTicks current_move_segment_stop_time_;
  // Used to set phase information for synthetic wheel events.
  bool needs_scroll_begin_ = true;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_SMOOTH_MOVE_GESTURE_H_
