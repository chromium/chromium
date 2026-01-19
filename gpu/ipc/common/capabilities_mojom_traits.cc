// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/capabilities_mojom_traits.h"

#include "ui/gfx/buffer_types.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::CapabilitiesDataView, gpu::Capabilities>::Read(
    gpu::mojom::CapabilitiesDataView data,
    gpu::Capabilities* out) {
  out->max_texture_size = data.max_texture_size();
  out->egl_image_external = data.egl_image_external();
  out->texture_format_bgra8888 = data.texture_format_bgra8888();
  out->texture_format_etc1_npot = data.texture_format_etc1_npot();
  out->sync_query = data.sync_query();
  out->texture_rg = data.texture_rg();
  out->texture_norm16 = data.texture_norm16();
  out->texture_half_float_linear = data.texture_half_float_linear();
  out->image_ycbcr_420v = data.image_ycbcr_420v();
  out->image_ar30 = data.image_ar30();
  out->image_ab30 = data.image_ab30();
  out->image_ycbcr_p010 = data.image_ycbcr_p010();
  out->render_buffer_format_bgra8888 = data.render_buffer_format_bgra8888();
  out->msaa_is_slow = data.msaa_is_slow();
  out->disable_one_component_textures = data.disable_one_component_textures();
  out->avoid_stencil_buffers = data.avoid_stencil_buffers();
  out->disable_2d_canvas_copy_on_write = data.disable_2d_canvas_copy_on_write();
  out->supports_rgb_to_yuv_conversion = data.supports_rgb_to_yuv_conversion();
  out->supports_yuv_readback = data.supports_yuv_readback();
  out->chromium_gpu_fence = data.chromium_gpu_fence();
  out->mesa_framebuffer_flip_y = data.mesa_framebuffer_flip_y();
  out->context_supports_distance_field_text =
      data.context_supports_distance_field_text();
  out->using_vulkan_context = data.using_vulkan_context();
  out->gpu_memory_buffer_formats =
      gfx::GpuMemoryBufferFormatSet::FromEnumBitmask(
          data.gpu_memory_buffer_formats());

  if (!data.ReadDrmFormatsAndModifiers(&out->drm_formats_and_modifiers)) {
    return false;
  }
  out->drm_device_id = data.drm_device_id();

  return true;
}

}  // namespace mojo
