// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_injector_mojom_traits.h"

namespace mojo {

// static
content::mojom::GestureSourceType
EnumTraits<content::mojom::GestureSourceType,
           content::SyntheticGestureParams::GestureSourceType>::
    ToMojom(content::SyntheticGestureParams::GestureSourceType input) {
  switch (input) {
    case content::SyntheticGestureParams::GestureSourceType::DEFAULT_INPUT:
      return content::mojom::GestureSourceType::kDefaultInput;
    case content::SyntheticGestureParams::GestureSourceType::TOUCH_INPUT:
      return content::mojom::GestureSourceType::kTouchInput;
    case content::SyntheticGestureParams::GestureSourceType::MOUSE_INPUT:
      return content::mojom::GestureSourceType::kMouseInput;
    case content::SyntheticGestureParams::GestureSourceType::PEN_INPUT:
      return content::mojom::GestureSourceType::kPenInput;
  }

  NOTREACHED();
  return content::mojom::GestureSourceType::kGestureSourceTypeMax;
}

// static
bool EnumTraits<content::mojom::GestureSourceType,
                content::SyntheticGestureParams::GestureSourceType>::
    FromMojom(content::mojom::GestureSourceType input,
              content::SyntheticGestureParams::GestureSourceType* output) {
  switch (input) {
    case content::mojom::GestureSourceType::kDefaultInput:
      *output =
          content::SyntheticGestureParams::GestureSourceType::DEFAULT_INPUT;
      return true;
    case content::mojom::GestureSourceType::kTouchInput:
      *output = content::SyntheticGestureParams::GestureSourceType::TOUCH_INPUT;
      return true;
    case content::mojom::GestureSourceType::kMouseInput:
      *output = content::SyntheticGestureParams::GestureSourceType::MOUSE_INPUT;
      return true;
    case content::mojom::GestureSourceType::kPenInput:
      *output = content::SyntheticGestureParams::GestureSourceType::PEN_INPUT;
      return true;
  }

  NOTREACHED();
  return false;
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
  return true;
}

// static
bool StructTraits<content::mojom::SyntheticTapDataView,
                  content::SyntheticTapGestureParams>::
    Read(content::mojom::SyntheticTapDataView data,
         content::SyntheticTapGestureParams* out) {
  if (!data.ReadGestureSourceType(&out->gesture_source_type) ||
      !data.ReadPosition(&out->position))
    return false;

  out->duration_ms = data.duration_ms();
  return true;
}

}  // namespace mojo