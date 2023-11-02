// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/latency/latency_info.h"

namespace content {

SyntheticTapGesture::SyntheticTapGesture(
    const SyntheticTapGestureParams& params)
    : params_(params),
      gesture_source_type_(content::mojom::GestureSourceType::kDefaultInput),
      state_(SETUP) {
  DCHECK_GE(params_.duration_ms, 0);
  if (params_.gesture_source_type ==
      content::mojom::GestureSourceType::kDefaultInput)
    params_.gesture_source_type =
        content::mojom::GestureSourceType::kTouchInput;
}

SyntheticTapGesture::~SyntheticTapGesture() {}

SyntheticGesture::Result SyntheticTapGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  if (state_ == SETUP) {
    gesture_source_type_ = params_.gesture_source_type;
    if (gesture_source_type_ ==
        content::mojom::GestureSourceType::kDefaultInput)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    state_ = PRESS;
  }

  DCHECK_NE(gesture_source_type_,
            content::mojom::GestureSourceType::kDefaultInput);

  if (!synthetic_pointer_driver_)
    synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
        gesture_source_type_, params_.from_devtools_debugger);

  if (gesture_source_type_ == content::mojom::GestureSourceType::kTouchInput ||
      gesture_source_type_ == content::mojom::GestureSourceType::kMouseInput)
    ForwardTouchOrMouseInputEvents(timestamp, target);
  else
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticTapGesture::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params_.GetGestureType(), gesture_source_type_,
                           std::move(callback));
}

bool SyntheticTapGesture::AllowHighFrequencyDispatch() const {
  return false;
}

void SyntheticTapGesture::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp, SyntheticGestureTarget* target) {
  switch (state_) {
    case PRESS:
      synthetic_pointer_driver_->Press(params_.position.x(),
                                       params_.position.y());
      synthetic_pointer_driver_->DispatchEvent(target, timestamp);
      // Release immediately if duration is 0.
      if (params_.duration_ms == 0) {
        synthetic_pointer_driver_->Release();
        synthetic_pointer_driver_->DispatchEvent(target, timestamp);
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
        state_ = DONE;
      }
      break;
    case SETUP:
      NOTREACHED() << "State SETUP invalid for synthetic tap gesture.";
      break;
    case DONE:
      NOTREACHED() << "State DONE invalid for synthetic tap gesture.";
      break;
  }
}

base::TimeDelta SyntheticTapGesture::GetDuration() const {
  return base::Milliseconds(params_.duration_ms);
}

}  // namespace content
