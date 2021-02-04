// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_INJECTOR_MOJOM_TRAITS_H_
#define CONTENT_COMMON_INPUT_INPUT_INJECTOR_MOJOM_TRAITS_H_

#include "content/common/input/input_injector.mojom.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "ui/gfx/geometry/point_f.h"

namespace mojo {

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

}  // namespace mojo

#endif  // CONTENT_COMMON_INPUT_INPUT_INJECTOR_MOJOM_TRAITS_H_
