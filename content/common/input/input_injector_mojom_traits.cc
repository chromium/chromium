// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_injector_mojom_traits.h"

namespace mojo {

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

}  // namespace mojo