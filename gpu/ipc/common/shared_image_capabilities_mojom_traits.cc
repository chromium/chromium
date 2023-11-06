// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/shared_image_capabilities_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::SharedImageCapabilitiesDataView,
                  gpu::SharedImageCapabilities>::
    Read(gpu::mojom::SharedImageCapabilitiesDataView data,
         gpu::SharedImageCapabilities* out) {
  out->supports_scanout_shared_images = data.supports_scanout_shared_images();
  out->supports_luminance_shared_images =
      data.supports_luminance_shared_images();
  out->supports_r16_shared_images = data.supports_r16_shared_images();
  out->disable_r8_shared_images = data.disable_r8_shared_images();

  out->shared_image_d3d = data.shared_image_d3d();
  out->shared_image_swap_chain = data.shared_image_swap_chain();
  return true;
}

}  // namespace mojo
