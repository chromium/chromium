// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/widget/device_emulation_params_mojom_traits.h"

#include "ui/gfx/geometry/mojom/geometry.mojom.h"

namespace mojo {

bool StructTraits<blink::mojom::DeviceEmulationParamsDataView,
                  blink::DeviceEmulationParams>::
    Read(blink::mojom::DeviceEmulationParamsDataView data,
         blink::DeviceEmulationParams* out) {
  if (!data.ReadScreenSize(&out->screen_size) ||
      !data.ReadViewPosition(&out->view_position) ||
      !data.ReadViewSize(&out->view_size) ||
      !data.ReadViewportOffset(&out->viewport_offset) ||
      !data.ReadViewportSegments(&out->viewport_segments) ||
      !data.ReadDevicePosture(&out->device_posture)) {
    return false;
  }
  out->screen_type = data.screen_type();
  out->device_scale_factor = data.device_scale_factor();
  out->scale = data.scale();
  out->viewport_scale = data.viewport_scale();
  out->screen_orientation_type = data.screen_orientation_type();
  out->screen_orientation_angle = data.screen_orientation_angle();
  return true;
}

}  // namespace mojo
