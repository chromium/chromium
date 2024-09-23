// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_tap_gesture.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/latency/latency_info.h"

namespace content {

SyntheticTapGesture::SyntheticTapGesture(
    const SyntheticTapGestureParams& gesture_params)
    : SyntheticGestureBase(gesture_params),
      gesture_source_type_(content::mojom::GestureSourceType::kDefaultInput),
      state_(SETUP) {
  CHECK_EQ(SyntheticGestureParams::TAP_GESTURE,
           gesture_params.GetGestureType());
  DCHECK_GE(params().duration_ms, 0);
  if (params().gesture_source_type ==
      content::mojom::GestureSourceType::kDefaultInput) {
    params().gesture_source_type =
        content::mojom::GestureSourceType::kTouchInput;
  }
}

SyntheticTapGesture::~SyntheticTapGesture() {}

SyntheticGesture::Result SyntheticTapGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  CHECK(dispatching_controller_);
  // Keep this on the stack so we can check if the forwarded event caused the
  // deletion of the controller (which owns `this`).
  base::WeakPtr<SyntheticGestureController> weak_controller =
      dispatching_controller_;

  if (state_ == SETUP) {
    gesture_source_type_ = params().gesture_source_type;
    if (gesture_source_type_ ==
        content::mojom::GestureSourceType::kDefaultInput)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    state_ = PRESS;
  }

  DCHECK_NE(gesture_source_type_,
            content::mojom::GestureSourceType::kDefaultInput);

  if (!synthetic_pointer_driver_)
    synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
        gesture_source_type_, params().from_devtools_debugger);

  if (gesture_source_type_ == content::mojom::GestureSourceType::kTouchInput ||
      gesture_source_type_ == content::mojom::GestureSourceType::kMouseInput) {
    ForwardTouchOrMouseInputEvents(timestamp, target);

    if (!weak_controller) {
      // ForwardTouchOrMouseInputEvents may cause the controller (and therefore
      // `this`) to be synchronously deleted (e.g. tapping tab-close). Return
      // immediately in this case.
      return SyntheticGesture::GESTURE_ABORT;
    }
  } else {
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
  }

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticTapGesture::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params().GetGestureType(), gesture_source_type_,
                           std::move(callback));
}

bool SyntheticTapGesture::AllowHighFrequencyDispatch() const {
  return false;
}

// CAUTION: Dispatching press/release events can cause `this` to be deleted.
void SyntheticTapGesture::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  // Keep this on the stack so we can check if the forwarded event caused the
  // deletion of the controller (which owns `this`).
  base::WeakPtr<SyntheticGestureController> weak_controller =
      dispatching_controller_;
  switch (state_) {
    case PRESS:
      synthetic_pointer_driver_->Press(params().position.x(),
                                       params().position.y());
      synthetic_pointer_driver_->DispatchEvent(target, timestamp);
      if (!weak_controller) {
        return;
      }
      // Release immediately if duration is 0.
      if (params().duration_ms == 0) {
        synthetic_pointer_driver_->Release();
        synthetic_pointer_driver_->DispatchEvent(target, timestamp);
        if (!weak_controller) {
          return;
        }
        state_ = DONE;
      } else {
        start_time_ = timestamp;
        state_ = WAITING_TO_RELEASE;
      }
      break;
    case WAITING_TO_RELEASE:
      if (timestamp - start_time_ >= GetDuration()) {
        synthetic_pointer_driver_->Release();
        synthetic_pointer_driver_->DispatchEvent(target,
                                                 start_time_ + GetDuration());
        if (!weak_controller) {
          return;
        }
        state_ = DONE;
      }
      break;
    case SETUP:
      NOTREACHED_IN_MIGRATION()
          << "State SETUP invalid for synthetic tap gesture.";
      break;
    case DONE:
      NOTREACHED_IN_MIGRATION()
          << "State DONE invalid for synthetic tap gesture.";
      break;
  }
}

base::TimeDelta SyntheticTapGesture::GetDuration() const {
  return base::Milliseconds(params().duration_ms);
}

}  // namespace content
