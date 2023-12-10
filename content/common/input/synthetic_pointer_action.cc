// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pointer_action.h"

#include "base/check_op.h"
#include "ui/latency/latency_info.h"

namespace content {

SyntheticPointerAction::SyntheticPointerAction(
    const SyntheticPointerActionListParams& params)
    : SyntheticGestureBase(params) {
  CHECK_EQ(SyntheticGestureParams::POINTER_ACTION_LIST,
           params.GetGestureType());
}

SyntheticPointerAction::~SyntheticPointerAction() {}

SyntheticGesture::Result SyntheticPointerAction::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  CHECK(dispatching_controller_);

  // Keep this on the stack so we can check if the forwarded event caused the
  // deletion of the controller (which owns `this`).
  base::WeakPtr<SyntheticGestureController> weak_controller =
      dispatching_controller_;

  if (state_ == GestureState::UNINITIALIZED) {
    gesture_source_type_ = params().gesture_source_type;
    if (gesture_source_type_ ==
        content::mojom::GestureSourceType::kDefaultInput)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    if (!external_synthetic_pointer_driver_) {
      DCHECK(!internal_synthetic_pointer_driver_);
      internal_synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
          gesture_source_type_, params().from_devtools_debugger);
    }

    state_ = GestureState::RUNNING;
  }

  DCHECK_NE(gesture_source_type_,
            content::mojom::GestureSourceType::kDefaultInput);
  if (gesture_source_type_ == content::mojom::GestureSourceType::kDefaultInput)
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;

  GestureState state = ForwardTouchOrMouseInputEvents(timestamp, target);
  if (!weak_controller) {
    // A pointer gesture can cause the controller (and therefore `this`) to be
    // synchronously deleted (e.g. clicking tab-close). Return immediately in
    // this case.
    return SyntheticGesture::GESTURE_ABORT;
  }

  state_ = state;

  if (state_ == GestureState::INVALID)
    return SyntheticGesture::POINTER_ACTION_INPUT_INVALID;

  return (state_ == GestureState::DONE) ? SyntheticGesture::GESTURE_FINISHED
                                        : SyntheticGesture::GESTURE_RUNNING;
}

bool SyntheticPointerAction::AllowHighFrequencyDispatch() const {
  return false;
}

void SyntheticPointerAction::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params().GetGestureType(), gesture_source_type_,
                           std::move(callback));
}

SyntheticPointerAction::GestureState
SyntheticPointerAction::ForwardTouchOrMouseInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (!params().params.size()) {
    return GestureState::DONE;
  }

  // An external pointer driver could be destroyed while the gesture is running.
  if (!PointerDriver()) {
    return GestureState::DONE;
  }

  DCHECK_LT(num_actions_dispatched_, params().params.size());
  SyntheticPointerActionListParams::ParamList& param_list =
      params().params[num_actions_dispatched_];

  // CAUTION: Forwarding a pointer input can cause `this` to be deleted.
  // Keep this on the stack so we can check if the forwarded event caused the
  // deletion of the controller (which owns `this`).
  base::WeakPtr<SyntheticGestureController> weak_controller =
      dispatching_controller_;

  for (const SyntheticPointerActionParams& param : param_list) {
    if (!PointerDriver()->UserInputCheck(param)) {
      return GestureState::INVALID;
    }

    switch (param.pointer_action_type()) {
      case SyntheticPointerActionParams::PointerActionType::PRESS:
        PointerDriver()->Press(param.position().x(), param.position().y(),
                               param.pointer_id(), param.button(),
                               param.key_modifiers(), param.width(),
                               param.height(), param.rotation_angle(),
                               param.force(), param.tangential_pressure(),
                               param.tilt_x(), param.tilt_y(), timestamp);
        break;
      case SyntheticPointerActionParams::PointerActionType::MOVE:
        PointerDriver()->Move(
            param.position().x(), param.position().y(), param.pointer_id(),
            param.key_modifiers(), param.width(), param.height(),
            param.rotation_angle(), param.force(), param.tangential_pressure(),
            param.tilt_x(), param.tilt_y(), param.button());
        break;
      case SyntheticPointerActionParams::PointerActionType::RELEASE:
        PointerDriver()->Release(param.pointer_id(), param.button(),
                                 param.key_modifiers());
        break;
      case SyntheticPointerActionParams::PointerActionType::CANCEL:
        PointerDriver()->Cancel(param.pointer_id());
        break;
      case SyntheticPointerActionParams::PointerActionType::LEAVE:
        PointerDriver()->Leave(param.pointer_id());
        break;
      case SyntheticPointerActionParams::PointerActionType::IDLE:
        break;
      case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
        return GestureState::INVALID;
    }
    base::TimeTicks dispatch_timestamp =
        param.timestamp().is_null() ? timestamp : param.timestamp();
    PointerDriver()->DispatchEvent(target, dispatch_timestamp);

    if (!weak_controller) {
      // Return value is unused because the caller returns immediately in this
      // condition as well.
      return GestureState::DONE;
    }
  }

  num_actions_dispatched_++;
  if (num_actions_dispatched_ == params().params.size()) {
    return GestureState::DONE;
  } else {
    return GestureState::RUNNING;
  }
}

SyntheticPointerDriver* SyntheticPointerAction::PointerDriver() const {
  DCHECK(!internal_synthetic_pointer_driver_ ||
         !external_synthetic_pointer_driver_);
  if (internal_synthetic_pointer_driver_) {
    return internal_synthetic_pointer_driver_.get();
  }

  return external_synthetic_pointer_driver_.get();
}

}  // namespace content
