// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pointer_action.h"

#include "base/check_op.h"
#include "ui/latency/latency_info.h"

namespace content {

SyntheticPointerAction::SyntheticPointerAction(
    const SyntheticPointerActionListParams& params)
    : params_(params),
      gesture_source_type_(content::mojom::GestureSourceType::kDefaultInput),
      state_(GestureState::UNINITIALIZED),
      num_actions_dispatched_(0U) {}

SyntheticPointerAction::~SyntheticPointerAction() {}

SyntheticGesture::Result SyntheticPointerAction::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (state_ == GestureState::UNINITIALIZED) {
    gesture_source_type_ = params_.gesture_source_type;
    if (gesture_source_type_ ==
        content::mojom::GestureSourceType::kDefaultInput)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    if (!synthetic_pointer_driver_) {
      owned_synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
          gesture_source_type_, params_.from_devtools_debugger);
      synthetic_pointer_driver_ = owned_synthetic_pointer_driver_.get();
    }

    state_ = GestureState::RUNNING;
  }

  DCHECK_NE(gesture_source_type_,
            content::mojom::GestureSourceType::kDefaultInput);
  if (gesture_source_type_ == content::mojom::GestureSourceType::kDefaultInput)
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  state_ = ForwardTouchOrMouseInputEvents(timestamp, target);

  if (state_ == GestureState::INVALID)
    return POINTER_ACTION_INPUT_INVALID;

  return (state_ == GestureState::DONE) ? SyntheticGesture::GESTURE_FINISHED
                                        : SyntheticGesture::GESTURE_RUNNING;
}

bool SyntheticPointerAction::AllowHighFrequencyDispatch() const {
  return false;
}

void SyntheticPointerAction::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params_.GetGestureType(), gesture_source_type_,
                           std::move(callback));
}

SyntheticPointerAction::GestureState
SyntheticPointerAction::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (!params_.params.size())
    return GestureState::DONE;

  DCHECK_LT(num_actions_dispatched_, params_.params.size());
  SyntheticPointerActionListParams::ParamList& param_list =
      params_.params[num_actions_dispatched_];

  for (const SyntheticPointerActionParams& param : param_list) {
    if (!synthetic_pointer_driver_->UserInputCheck(param))
      return GestureState::INVALID;

    switch (param.pointer_action_type()) {
      case SyntheticPointerActionParams::PointerActionType::PRESS:
        synthetic_pointer_driver_->Press(
            param.position().x(), param.position().y(), param.pointer_id(),
            param.button(), param.key_modifiers(), param.width(),
            param.height(), param.rotation_angle(), param.force(),
            param.tangential_pressure(), param.tilt_x(), param.tilt_y(),
            timestamp);
        break;
      case SyntheticPointerActionParams::PointerActionType::MOVE:
        synthetic_pointer_driver_->Move(
            param.position().x(), param.position().y(), param.pointer_id(),
            param.key_modifiers(), param.width(), param.height(),
            param.rotation_angle(), param.force(), param.tangential_pressure(),
            param.tilt_x(), param.tilt_y(), param.button());
        break;
      case SyntheticPointerActionParams::PointerActionType::RELEASE:
        synthetic_pointer_driver_->Release(param.pointer_id(), param.button(),
                                           param.key_modifiers());
        break;
      case SyntheticPointerActionParams::PointerActionType::CANCEL:
        synthetic_pointer_driver_->Cancel(param.pointer_id());
        break;
      case SyntheticPointerActionParams::PointerActionType::LEAVE:
        synthetic_pointer_driver_->Leave(param.pointer_id());
        break;
      case SyntheticPointerActionParams::PointerActionType::IDLE:
        break;
      case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
        return GestureState::INVALID;
    }
    base::TimeTicks dispatch_timestamp =
        param.timestamp().is_null() ? timestamp : param.timestamp();
    synthetic_pointer_driver_->DispatchEvent(target, dispatch_timestamp);
  }

  num_actions_dispatched_++;
  if (num_actions_dispatched_ == params_.params.size())
    return GestureState::DONE;
  else
    return GestureState::RUNNING;
}

}  // namespace content
