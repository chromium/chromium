// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_CAPABILITIES_MOJOM_TRAITS_H_
#define GPU_IPC_COMMON_CAPABILITIES_MOJOM_TRAITS_H_

#include "base/containers/flat_map.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/ipc/common/capabilities.mojom-shared.h"
#include "gpu/ipc/common/gpu_ipc_common_export.h"

namespace mojo {

template <>
struct GPU_IPC_COMMON_EXPORT StructTraits<gpu::mojom::CapabilitiesDataView,
                                          gpu::Capabilities> {
  static int32_t max_texture_size(const gpu::Capabilities& cap) {
    return cap.max_texture_size;
  }
  static bool egl_image_external(const gpu::Capabilities& cap) {
    return cap.egl_image_external;
  }
  static bool texture_format_bgra8888(const gpu::Capabilities& cap) {
    return cap.texture_format_bgra8888;
  }
  static bool texture_format_etc1_npot(const gpu::Capabilities& cap) {
    return cap.texture_format_etc1_npot;
  }
  static bool sync_query(const gpu::Capabilities& cap) {
    return cap.sync_query;
  }
  static bool texture_rg(const gpu::Capabilities& cap) {
    return cap.texture_rg;
  }
  static bool texture_norm16(const gpu::Capabilities& cap) {
    return cap.texture_norm16;
  }
  static bool texture_half_float_linear(const gpu::Capabilities& cap) {
    return cap.texture_half_float_linear;
  }
  static bool image_ycbcr_420v(const gpu::Capabilities& cap) {
    return cap.image_ycbcr_420v;
  }
  static bool image_ar30(const gpu::Capabilities& cap) {
    return cap.image_ar30;
  }
  static bool image_ab30(const gpu::Capabilities& cap) {
    return cap.image_ab30;
  }
  static bool image_ycbcr_p010(const gpu::Capabilities& cap) {
    return cap.image_ycbcr_p010;
  }
  static bool render_buffer_format_bgra8888(const gpu::Capabilities& cap) {
    return cap.render_buffer_format_bgra8888;
  }
  static bool msaa_is_slow(const gpu::Capabilities& cap) {
    return cap.msaa_is_slow;
  }
  static bool disable_one_component_textures(const gpu::Capabilities& cap) {
    return cap.disable_one_component_textures;
  }
  static bool avoid_stencil_buffers(const gpu::Capabilities& cap) {
    return cap.avoid_stencil_buffers;
  }
  static bool disable_2d_canvas_copy_on_write(const gpu::Capabilities& cap) {
    return cap.disable_2d_canvas_copy_on_write;
  }
  static bool supports_rgb_to_yuv_conversion(const gpu::Capabilities& cap) {
    return cap.supports_rgb_to_yuv_conversion;
  }
  static bool supports_yuv_readback(const gpu::Capabilities& cap) {
    return cap.supports_yuv_readback;
  }
  static bool chromium_gpu_fence(const gpu::Capabilities& cap) {
    return cap.chromium_gpu_fence;
  }
  static bool mesa_framebuffer_flip_y(const gpu::Capabilities& cap) {
    return cap.mesa_framebuffer_flip_y;
  }
  static bool context_supports_distance_field_text(
      const gpu::Capabilities& cap) {
    return cap.context_supports_distance_field_text;
  }
  static bool using_vulkan_context(const gpu::Capabilities& cap) {
    return cap.using_vulkan_context;
  }
  static uint64_t gpu_memory_buffer_formats(const gpu::Capabilities& cap) {
    return cap.gpu_memory_buffer_formats.ToEnumBitmask();
  }
  static const base::flat_map<uint32_t, std::vector<uint64_t>>&
  drm_formats_and_modifiers(const gpu::Capabilities& cap) {
    return cap.drm_formats_and_modifiers;
  }
  static uint64_t drm_device_id(const gpu::Capabilities& cap) {
    return cap.drm_device_id;
  }

  static bool Read(gpu::mojom::CapabilitiesDataView data,
                   gpu::Capabilities* out);
};

}  // namespace mojo

#endif  // GPU_IPC_COMMON_CAPABILITIES_MOJOM_TRAITS_H_
