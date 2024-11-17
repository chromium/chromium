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

#if BUILDFLAG(IS_WIN)
  out->supports_scanout_shared_images_for_software_video_frames =
      data.supports_scanout_shared_images_for_software_video_frames();
#endif

  out->supports_luminance_shared_images =
      data.supports_luminance_shared_images();
  out->supports_r16_shared_images = data.supports_r16_shared_images();
  out->supports_native_nv12_mappable_shared_images =
      data.supports_native_nv12_mappable_shared_images();
  out->is_r16f_supported = data.is_r16f_supported();
  out->disable_r8_shared_images = data.disable_r8_shared_images();
  out->disable_webgpu_shared_images = data.disable_webgpu_shared_images();

  out->shared_image_d3d = data.shared_image_d3d();
  out->shared_image_swap_chain = data.shared_image_swap_chain();

#if BUILDFLAG(IS_MAC)
  out->texture_target_for_io_surfaces = data.texture_target_for_io_surfaces();
#endif

  return true;
}

}  // namespace mojo
