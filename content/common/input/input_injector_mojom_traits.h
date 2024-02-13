// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_INJECTOR_MOJOM_TRAITS_H_
#define CONTENT_COMMON_INPUT_INPUT_INJECTOR_MOJOM_TRAITS_H_

#include "content/common/input/input_injector.mojom.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_pointer_action_list_params.h"
#include "content/common/input/synthetic_pointer_action_params.h"
#include "content/common/input/synthetic_smooth_drag_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/point_f.h"

namespace mojo {

template <>
struct EnumTraits<content::mojom::PointerActionType,
                  content::SyntheticPointerActionParams::PointerActionType> {
  static content::mojom::PointerActionType ToMojom(
      content::SyntheticPointerActionParams::PointerActionType input);
  static bool FromMojom(
      content::mojom::PointerActionType input,
      content::SyntheticPointerActionParams::PointerActionType* output);
};

template <>
struct EnumTraits<content::mojom::SyntheticButton,
                  content::SyntheticPointerActionParams::Button> {
  static content::mojom::SyntheticButton ToMojom(
      content::SyntheticPointerActionParams::Button input);
  static bool FromMojom(content::mojom::SyntheticButton input,
                        content::SyntheticPointerActionParams::Button* output);
};

template <>
struct StructTraits<content::mojom::SyntheticSmoothDragDataView,
                    content::SyntheticSmoothDragGestureParams> {
  static content::mojom::GestureSourceType gesture_source_type(
      const content::SyntheticSmoothDragGestureParams& r) {
    return r.gesture_source_type;
  }

  static const gfx::PointF& start_point(
      const content::SyntheticSmoothDragGestureParams& r) {
    return r.start_point;
  }

  static const std::vector<gfx::Vector2dF>& distances(
      const content::SyntheticSmoothDragGestureParams& r) {
    return r.distances;
  }

  static float speed_in_pixels_s(
      const content::SyntheticSmoothDragGestureParams& r) {
    return r.speed_in_pixels_s;
  }

  static float vsync_offset_ms(
      const content::SyntheticSmoothDragGestureParams& r) {
    return r.vsync_offset_ms;
  }

  static content::mojom::InputEventPattern input_event_pattern(
      const content::SyntheticSmoothDragGestureParams& r) {
    return r.input_event_pattern;
  }

  static bool Read(content::mojom::SyntheticSmoothDragDataView r,
                   content::SyntheticSmoothDragGestureParams* out);
};

template <>
struct StructTraits<content::mojom::SyntheticPinchDataView,
                    content::SyntheticPinchGestureParams> {
  static float scale_factor(const content::SyntheticPinchGestureParams& r) {
    return r.scale_factor;
  }

  static const gfx::PointF& anchor(
      const content::SyntheticPinchGestureParams& r) {
    return r.anchor;
  }

  static float relative_pointer_speed_in_pixels_s(
      const content::SyntheticPinchGestureParams& r) {
    return r.relative_pointer_speed_in_pixels_s;
  }

  static float vsync_offset_ms(const content::SyntheticPinchGestureParams& r) {
    return r.vsync_offset_ms;
  }

  static content::mojom::InputEventPattern input_event_pattern(
      const content::SyntheticPinchGestureParams& r) {
    return r.input_event_pattern;
  }

  static bool Read(content::mojom::SyntheticPinchDataView r,
                   content::SyntheticPinchGestureParams* out);
};

template <>
struct StructTraits<content::mojom::SyntheticSmoothScrollDataView,
                    content::SyntheticSmoothScrollGestureParams> {
  static content::mojom::GestureSourceType gesture_source_type(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.gesture_source_type;
  }

  static const gfx::PointF& anchor(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.anchor;
  }

  static const std::vector<gfx::Vector2dF>& distances(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.distances;
  }

  static bool prevent_fling(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.prevent_fling;
  }

  static float speed_in_pixels_s(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.speed_in_pixels_s;
  }

  static float fling_velocity_x(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.fling_velocity_x;
  }

  static float fling_velocity_y(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.fling_velocity_y;
  }

  static ui::ScrollGranularity granularity(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.granularity;
  }

  static int32_t modifiers(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.modifiers;
  }

  static float vsync_offset_ms(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.vsync_offset_ms;
  }

  static content::mojom::InputEventPattern input_event_pattern(
      const content::SyntheticSmoothScrollGestureParams& r) {
    return r.input_event_pattern;
  }

  static bool Read(content::mojom::SyntheticSmoothScrollDataView r,
                   content::SyntheticSmoothScrollGestureParams* out);
};

template <>
struct StructTraits<content::mojom::SyntheticTapDataView,
                    content::SyntheticTapGestureParams> {
  static content::mojom::GestureSourceType gesture_source_type(
      const content::SyntheticTapGestureParams& r) {
    return r.gesture_source_type;
  }

  static const gfx::PointF& position(
      const content::SyntheticTapGestureParams& r) {
    return r.position;
  }

  static float duration_ms(const content::SyntheticTapGestureParams& r) {
    return r.duration_ms;
  }

  static bool Read(content::mojom::SyntheticTapDataView r,
                   content::SyntheticTapGestureParams* out);
};

template <>
struct StructTraits<content::mojom::SyntheticPointerActionParamsDataView,
                    content::SyntheticPointerActionParams> {
  static content::SyntheticPointerActionParams::PointerActionType
  pointer_action_type(const content::SyntheticPointerActionParams& r) {
    return r.pointer_action_type_;
  }

  static gfx::PointF position(const content::SyntheticPointerActionParams& r) {
    return r.position_;
  }

  static uint32_t pointer_id(const content::SyntheticPointerActionParams& r) {
    return r.pointer_id_;
  }

  static content::SyntheticPointerActionParams::Button button(
      const content::SyntheticPointerActionParams& r) {
    return r.button_;
  }

  static uint32_t key_modifiers(
      const content::SyntheticPointerActionParams& r) {
    return r.key_modifiers_;
  }

  static float width(const content::SyntheticPointerActionParams& r) {
    return r.width_;
  }

  static float height(const content::SyntheticPointerActionParams& r) {
    return r.height_;
  }

  static float rotation_angle(const content::SyntheticPointerActionParams& r) {
    return r.rotation_angle_;
  }

  static float force(const content::SyntheticPointerActionParams& r) {
    return r.force_;
  }

  static float tangential_pressure(
      const content::SyntheticPointerActionParams& r) {
    return r.tangential_pressure_;
  }

  static uint32_t tilt_x(const content::SyntheticPointerActionParams& r) {
    return r.tilt_x_;
  }

  static uint32_t tilt_y(const content::SyntheticPointerActionParams& r) {
    return r.tilt_y_;
  }

  static base::TimeTicks timestamp(
      const content::SyntheticPointerActionParams& r) {
    return r.timestamp_;
  }

  static base::TimeDelta duration(
      const content::SyntheticPointerActionParams& r) {
    return r.duration_;
  }

  static bool Read(content::mojom::SyntheticPointerActionParamsDataView r,
                   content::SyntheticPointerActionParams* out);
};

template <>
struct StructTraits<content::mojom::SyntheticPointerActionDataView,
                    content::SyntheticPointerActionListParams> {
  static content::mojom::GestureSourceType gesture_source_type(
      const content::SyntheticPointerActionListParams& r) {
    return r.gesture_source_type;
  }

  static const std::vector<std::vector<content::SyntheticPointerActionParams>>&
  params(const content::SyntheticPointerActionListParams& r) {
    return r.params;
  }

  static bool Read(content::mojom::SyntheticPointerActionDataView r,
                   content::SyntheticPointerActionListParams* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_INPUT_INPUT_INJECTOR_MOJOM_TRAITS_H_
