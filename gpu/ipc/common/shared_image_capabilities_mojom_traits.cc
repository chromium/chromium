// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/shared_image_capabilities_mojom_traits.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

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
  out->is_r16f_supported = data.is_r16f_supported();
  out->disable_r8_shared_images = data.disable_r8_shared_images();
  out->disable_webgpu_shared_images = data.disable_webgpu_shared_images();

  out->shared_image_d3d = data.shared_image_d3d();
  out->shared_image_swap_chain = data.shared_image_swap_chain();

  mojo::ArrayDataView<gfx::mojom::BufferUsageAndFormatDataView>
      usage_and_format_list;
  data.GetTextureTargetExceptionListDataView(&usage_and_format_list);
  for (size_t i = 0; i < usage_and_format_list.size(); ++i) {
    gfx::BufferUsageAndFormat usage_format;
    if (!usage_and_format_list.Read(i, &usage_format)) {
      return false;
    }
    out->texture_target_exception_list.push_back(usage_format);
  }

#if BUILDFLAG(IS_MAC)
  out->macos_specific_texture_target = data.macos_specific_texture_target();
#endif

  return true;
}

}  // namespace mojo
