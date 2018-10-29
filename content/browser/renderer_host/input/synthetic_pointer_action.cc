// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pointer_action.h"

#include "base/logging.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/latency/latency_info.h"

namespace content {

SyntheticPointerAction::SyntheticPointerAction(
    const SyntheticPointerActionListParams& params)
    : params_(params),
      gesture_source_type_(SyntheticGestureParams::DEFAULT_INPUT),
      state_(UNINITIALIZED),
      num_actions_dispatched_(0U) {}

SyntheticPointerAction::~SyntheticPointerAction() {}

SyntheticGesture::Result SyntheticPointerAction::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (state_ == UNINITIALIZED) {
    gesture_source_type_ = params_.gesture_source_type;
    if (gesture_source_type_ == SyntheticGestureParams::DEFAULT_INPUT)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    if (!synthetic_pointer_driver_) {
      synthetic_pointer_driver_ =
          SyntheticPointerDriver::Create(gesture_source_type_);
    }
    state_ = RUNNING;
  }

  DCHECK_NE(gesture_source_type_, SyntheticGestureParams::DEFAULT_INPUT);
  if (gesture_source_type_ == SyntheticGestureParams::DEFAULT_INPUT)
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  state_ = ForwardTouchOrMouseInputEvents(timestamp, target);

  if (state_ == INVALID)
    return POINTER_ACTION_INPUT_INVALID;

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

bool SyntheticPointerAction::AllowHighFrequencyDispatch() const {
  return false;
}

SyntheticPointerAction::GestureState
SyntheticPointerAction::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (!params_.params.size())
    return DONE;

  DCHECK_LT(num_actions_dispatched_, params_.params.size());
  SyntheticPointerActionListParams::ParamList& param_list =
      params_.params[num_actions_dispatched_];
  for (const SyntheticPointerActionParams& param : param_list) {
    if (!synthetic_pointer_driver_->UserInputCheck(param))
      return INVALID;

    switch (param.pointer_action_type()) {
      case SyntheticPointerActionParams::PointerActionType::PRESS:
        synthetic_pointer_driver_->Press(param.position().x(),
                                         param.position().y(),
                                         param.pointer_id(), param.button());
        break;
      case SyntheticPointerActionParams::PointerActionType::MOVE:
        synthetic_pointer_driver_->Move(
            param.position().x(), param.position().y(), param.pointer_id());
        break;
      case SyntheticPointerActionParams::PointerActionType::RELEASE:
        synthetic_pointer_driver_->Release(param.pointer_id(), param.button());
        break;
      case SyntheticPointerActionParams::PointerActionType::LEAVE:
        synthetic_pointer_driver_->Leave(param.pointer_id());
        break;
      case SyntheticPointerActionParams::PointerActionType::IDLE:
        break;
      case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
        return INVALID;
    }
    synthetic_pointer_driver_->DispatchEvent(target, timestamp);
  }

  num_actions_dispatched_++;
  if (num_actions_dispatched_ == params_.params.size())
    return DONE;
  else
    return RUNNING;
}

}  // namespace content
