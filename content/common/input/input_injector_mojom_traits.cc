// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_injector_mojom_traits.h"

namespace mojo {

// static
content::mojom::PointerActionType
EnumTraits<content::mojom::PointerActionType,
           content::SyntheticPointerActionParams::PointerActionType>::
    ToMojom(content::SyntheticPointerActionParams::PointerActionType input) {
  switch (input) {
    case content::SyntheticPointerActionParams::PointerActionType::
        NOT_INITIALIZED:
      return content::mojom::PointerActionType::kNotInitialized;
    case content::SyntheticPointerActionParams::PointerActionType::PRESS:
      return content::mojom::PointerActionType::kPress;
    case content::SyntheticPointerActionParams::PointerActionType::MOVE:
      return content::mojom::PointerActionType::kMove;
    case content::SyntheticPointerActionParams::PointerActionType::RELEASE:
      return content::mojom::PointerActionType::kRelease;
    case content::SyntheticPointerActionParams::PointerActionType::CANCEL:
      return content::mojom::PointerActionType::kCancel;
    case content::SyntheticPointerActionParams::PointerActionType::LEAVE:
      return content::mojom::PointerActionType::kLeave;
    case content::SyntheticPointerActionParams::PointerActionType::IDLE:
      return content::mojom::PointerActionType::kIdle;
  }

  NOTREACHED_IN_MIGRATION();
  return content::mojom::PointerActionType::kMaxValue;
}

// static
bool EnumTraits<content::mojom::PointerActionType,
                content::SyntheticPointerActionParams::PointerActionType>::
    FromMojom(
        content::mojom::PointerActionType input,
        content::SyntheticPointerActionParams::PointerActionType* output) {
  switch (input) {
    case content::mojom::PointerActionType::kNotInitialized:
      *output = content::SyntheticPointerActionParams::PointerActionType::
          NOT_INITIALIZED;
      return true;
    case content::mojom::PointerActionType::kPress:
      *output = content::SyntheticPointerActionParams::PointerActionType::PRESS;
      return true;
    case content::mojom::PointerActionType::kMove:
      *output = content::SyntheticPointerActionParams::PointerActionType::MOVE;
      return true;
    case content::mojom::PointerActionType::kRelease:
      *output =
          content::SyntheticPointerActionParams::PointerActionType::RELEASE;
      return true;
    case content::mojom::PointerActionType::kCancel:
      *output =
          content::SyntheticPointerActionParams::PointerActionType::CANCEL;
      return true;
    case content::mojom::PointerActionType::kLeave:
      *output = content::SyntheticPointerActionParams::PointerActionType::LEAVE;
      return true;
    case content::mojom::PointerActionType::kIdle:
      *output = content::SyntheticPointerActionParams::PointerActionType::IDLE;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
content::mojom::SyntheticButton
EnumTraits<content::mojom::SyntheticButton,
           content::SyntheticPointerActionParams::Button>::
    ToMojom(content::SyntheticPointerActionParams::Button input) {
  switch (input) {
    case content::SyntheticPointerActionParams::Button::NO_BUTTON:
      return content::mojom::SyntheticButton::kNoButton;
    case content::SyntheticPointerActionParams::Button::LEFT:
      return content::mojom::SyntheticButton::kLeft;
    case content::SyntheticPointerActionParams::Button::MIDDLE:
      return content::mojom::SyntheticButton::kMiddle;
    case content::SyntheticPointerActionParams::Button::RIGHT:
      return content::mojom::SyntheticButton::kRight;
    case content::SyntheticPointerActionParams::Button::BACK:
      return content::mojom::SyntheticButton::kBack;
    case content::SyntheticPointerActionParams::Button::FORWARD:
      return content::mojom::SyntheticButton::kForward;
  }

  NOTREACHED_IN_MIGRATION();
  return content::mojom::SyntheticButton::kMaxValue;
}

// static
bool EnumTraits<content::mojom::SyntheticButton,
                content::SyntheticPointerActionParams::Button>::
    FromMojom(content::mojom::SyntheticButton input,
              content::SyntheticPointerActionParams::Button* output) {
  switch (input) {
    case content::mojom::SyntheticButton::kNoButton:
      *output = content::SyntheticPointerActionParams::Button::NO_BUTTON;
      return true;
    case content::mojom::SyntheticButton::kLeft:
      *output = content::SyntheticPointerActionParams::Button::LEFT;
      return true;
    case content::mojom::SyntheticButton::kMiddle:
      *output = content::SyntheticPointerActionParams::Button::MIDDLE;
      return true;
    case content::mojom::SyntheticButton::kRight:
      *output = content::SyntheticPointerActionParams::Button::RIGHT;
      return true;
    case content::mojom::SyntheticButton::kBack:
      *output = content::SyntheticPointerActionParams::Button::BACK;
      return true;
    case content::mojom::SyntheticButton::kForward:
      *output = content::SyntheticPointerActionParams::Button::FORWARD;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
bool StructTraits<content::mojom::SyntheticSmoothDragDataView,
                  content::SyntheticSmoothDragGestureParams>::
    Read(content::mojom::SyntheticSmoothDragDataView data,
         content::SyntheticSmoothDragGestureParams* out) {
  if (!data.ReadStartPoint(&out->start_point) ||
      !data.ReadDistances(&out->distances))
    return false;

  out->gesture_source_type = data.gesture_source_type();
  out->speed_in_pixels_s = data.speed_in_pixels_s();
  out->vsync_offset_ms = data.vsync_offset_ms();
  out->input_event_pattern = data.input_event_pattern();
  return true;
}

// static
bool StructTraits<content::mojom::SyntheticSmoothScrollDataView,
                  content::SyntheticSmoothScrollGestureParams>::
    Read(content::mojom::SyntheticSmoothScrollDataView data,
         content::SyntheticSmoothScrollGestureParams* out) {
  if (!data.ReadAnchor(&out->anchor) || !data.ReadDistances(&out->distances) ||
      !data.ReadGranularity(&out->granularity))
    return false;

  out->gesture_source_type = data.gesture_source_type();
  out->prevent_fling = data.prevent_fling();
  out->speed_in_pixels_s = data.speed_in_pixels_s();
  out->fling_velocity_x = data.fling_velocity_x();
  out->fling_velocity_y = data.fling_velocity_y();
  out->modifiers = data.modifiers();
  out->vsync_offset_ms = data.vsync_offset_ms();
  out->input_event_pattern = data.input_event_pattern();
  return true;
}

// static
bool StructTraits<content::mojom::SyntheticPinchDataView,
                  content::SyntheticPinchGestureParams>::
    Read(content::mojom::SyntheticPinchDataView data,
         content::SyntheticPinchGestureParams* out) {
  if (!data.ReadAnchor(&out->anchor))
    return false;

  out->scale_factor = data.scale_factor();
  out->relative_pointer_speed_in_pixels_s =
      data.relative_pointer_speed_in_pixels_s();
  out->vsync_offset_ms = data.vsync_offset_ms();
  out->input_event_pattern = data.input_event_pattern();
  return true;
}

// static
bool StructTraits<content::mojom::SyntheticTapDataView,
                  content::SyntheticTapGestureParams>::
    Read(content::mojom::SyntheticTapDataView data,
         content::SyntheticTapGestureParams* out) {
  if (!data.ReadPosition(&out->position))
    return false;

  out->gesture_source_type = data.gesture_source_type();
  out->duration_ms = data.duration_ms();
  return true;
}

// static
bool StructTraits<content::mojom::SyntheticPointerActionParamsDataView,
                  content::SyntheticPointerActionParams>::
    Read(content::mojom::SyntheticPointerActionParamsDataView data,
         content::SyntheticPointerActionParams* out) {
  if (!data.ReadPointerActionType(&out->pointer_action_type_) ||
      !data.ReadPosition(&out->position_) || !data.ReadButton(&out->button_) ||
      !data.ReadTimestamp(&out->timestamp_) ||
      !data.ReadDuration(&out->duration_))
    return false;

  out->pointer_id_ = data.pointer_id();
  out->key_modifiers_ = data.key_modifiers();
  out->width_ = data.width();
  out->height_ = data.height();
  out->rotation_angle_ = data.rotation_angle();
  out->force_ = data.force();
  out->tangential_pressure_ = data.tangential_pressure();
  out->tilt_x_ = data.tilt_x();
  out->tilt_y_ = data.tilt_y();
  return true;
}

// static
bool StructTraits<content::mojom::SyntheticPointerActionDataView,
                  content::SyntheticPointerActionListParams>::
    Read(content::mojom::SyntheticPointerActionDataView data,
         content::SyntheticPointerActionListParams* out) {
  if (!data.ReadParams(&out->params))
    return false;

  out->gesture_source_type = data.gesture_source_type();

  return true;
}

}  // namespace mojo
