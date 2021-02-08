// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_INJECTOR_MOJOM_TRAITS_H_
#define CONTENT_COMMON_INPUT_INPUT_INJECTOR_MOJOM_TRAITS_H_

#include "content/common/input/input_injector.mojom.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_drag_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/point_f.h"

namespace mojo {

template <>
struct CONTENT_EXPORT
    EnumTraits<content::mojom::GestureSourceType,
               content::SyntheticGestureParams::GestureSourceType> {
  static content::mojom::GestureSourceType ToMojom(
      content::SyntheticGestureParams::GestureSourceType input);
  static bool FromMojom(
      content::mojom::GestureSourceType input,
      content::SyntheticGestureParams::GestureSourceType* output);
};

template <>
struct CONTENT_EXPORT StructTraits<content::mojom::SyntheticSmoothDragDataView,
                                   content::SyntheticSmoothDragGestureParams> {
  static content::SyntheticGestureParams::GestureSourceType gesture_source_type(
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

  static bool Read(content::mojom::SyntheticSmoothDragDataView r,
                   content::SyntheticSmoothDragGestureParams* out);
};

template <>
struct CONTENT_EXPORT StructTraits<content::mojom::SyntheticPinchDataView,
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

  static bool Read(content::mojom::SyntheticPinchDataView r,
                   content::SyntheticPinchGestureParams* out);
};

template <>
struct CONTENT_EXPORT
    StructTraits<content::mojom::SyntheticSmoothScrollDataView,
                 content::SyntheticSmoothScrollGestureParams> {
  static content::SyntheticGestureParams::GestureSourceType gesture_source_type(
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

  static bool Read(content::mojom::SyntheticSmoothScrollDataView r,
                   content::SyntheticSmoothScrollGestureParams* out);
};

template <>
struct CONTENT_EXPORT StructTraits<content::mojom::SyntheticTapDataView,
                                   content::SyntheticTapGestureParams> {
  static content::SyntheticGestureParams::GestureSourceType gesture_source_type(
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

}  // namespace mojo

#endif  // CONTENT_COMMON_INPUT_INPUT_INJECTOR_MOJOM_TRAITS_H_
